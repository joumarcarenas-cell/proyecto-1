#include "include/ElementalMage.h"
#include "include/CombatUtils.h"
#include "include/graphics/VFXSystem.h"
#include "include/graphics/AnimeVFX.h"
#include "include/scenes/CharacterSelectScene.h" // Might not need but for safety
#include "include/scenes/GameplayScene.h"
#include "rlgl.h"
#include <cmath>

extern float screenShake;
extern float hitstopTimer;

ElementalMage::ElementalMage(Vector2 pos) {
  position = pos;
  maxHp = 200.0f;
  hp = maxHp;
  maxEnergy = 120.0f; // Un poco mas de energía para el mago
  energy = maxEnergy;
  radius = 18.0f;

  dashMaxCD = 1.6f;
  qCooldown = 0.0f;
  eCooldown = 0.0f;
  ultCooldown = 0.0f;

  eChargesCooldowns[0] = 0.0f;
  eChargesCooldowns[1] = 0.0f;
  eChargesCooldowns[2] = 0.0f;

  currentMode = ElementMode::WATER_ICE;
  lastDamageType = DamageType::MAGICAL;
}

int ElementalMage::GetAvailableECharges() const {
  int count = 0;
  for (int i = 0; i < 3; i++) {
    if (eChargesCooldowns[i] <= 0.0f)
      count++;
  }
  return count;
}

void ElementalMage::Update() {
  float dt = GetFrameTime();

  if (hitstopTimer <= 0.0f) {
    UpdateState(dt);
    HandleInput(dt);
    UpdateDash(dt);
    UpdateEntities(dt);

    // Clamping to Arena
    position = Arena::GetClampedPos(position, radius);

    // Regeneración de energía lenta
    if (energy < maxEnergy)
      energy += 8.0f * dt;
  }
}

void ElementalMage::Reset(Vector2 pos) {
  position = pos;
  hp = maxHp;
  energy = maxEnergy;
  state = MageState::NORMAL;
  projectiles.clear();
  tornados.clear();
  lightningRays.clear();
  hitAreas.clear();
  isOverloaded = false;
  eMarkActive = false;
  dashCharges = maxDashCharges;
  lastDamageType = DamageType::MAGICAL;
}

void ElementalMage::ChangeMode() {
  if (currentMode == ElementMode::WATER_ICE) {
    currentMode = ElementMode::LIGHTNING;
    lastDamageType = DamageType::MAGICAL;
  } else {
    currentMode = ElementMode::WATER_ICE;
    lastDamageType = DamageType::MAGICAL;
  }

  // Q Buff: Attack speed +20% for 3 seconds
  attackSpeedBuffTimer = 3.0f;
  qCooldown = 1.0f; // Soft cooldown

  staticStackBasicAvailable = true;
  staticStackEAvailable = true;
  staticStackUltAvailable = true;

  // Generar VFX visual de cambio
  Color c = (currentMode == ElementMode::WATER_ICE) ? SKYBLUE : YELLOW;
  for (int i = 0; i < 15; i++) {
    Graphics::VFXSystem::GetInstance().SpawnParticleEx(
        position,
        {(float)GetRandomValue(-200, 200), (float)GetRandomValue(-200, 200)},
        0.5f, c, 5.0f, Graphics::RenderType::RHOMB, BLEND_ADDITIVE);
  }
}

void ElementalMage::HandleInput(float dt) {
  if (state == MageState::LOCKED || state == MageState::DASHING ||
      state == MageState::HEAVY_ATTACK)
    return;

  // Movement handling (Basic top-down isomorphic)
  if (state == MageState::NORMAL) {
    Vector2 input = {0, 0};
    if (IsKeyDown(KEY_W))
      input.y -= 1;
    if (IsKeyDown(KEY_S))
      input.y += 1;
    if (IsKeyDown(KEY_A))
      input.x -= 1;
    if (IsKeyDown(KEY_D))
      input.x += 1;

    if (Vector2Length(input) > 0) {
      input = Vector2Normalize(input);
      // Move speed buff if R mode is active
      float speed = 320.0f;
      if (isOverloaded)
        speed *= 1.25f; // R buff
      position = Vector2Add(position, Vector2Scale(input, speed * dt));
    }
  }

  // Orientate towards mouse - Locked during attacks and casting (User request)
  if (state == MageState::NORMAL && attackPhase == AttackPhase::NONE && 
      heavyCastTimer <= 0 && eCastTimer <= 0) {
    Vector2 dir = Vector2Subtract(targetAim, position);
    if (Vector2Length(dir) > 0) {
      facing = Vector2Normalize(dir);
    }
  }

  // DASH
  if (IsKeyPressed(controls.dash) && CanDash() && state == MageState::NORMAL) {
    UseDash();
    state = MageState::DASHING;
    attackPhaseTimer = 0.32f; // Mas i-frames (era 0.18)
    hitstopTimer = 0.0f;

    // Dirección basada en Input (WASD)
    Vector2 input = {0, 0};
    if (IsKeyDown(KEY_W))
      input.y -= 1;
    if (IsKeyDown(KEY_S))
      input.y += 1;
    if (IsKeyDown(KEY_A))
      input.x -= 1;
    if (IsKeyDown(KEY_D))
      input.x += 1;

    if (Vector2Length(input) == 0)
      input = facing;
    velocity = Vector2Scale(Vector2Normalize(input), 975.0f); // Igualado con Ropera (-15%)
  }

  // Q - Change Mode
  if (qCooldown <= 0.0f && IsKeyPressed(controls.boomerang)) {
    ChangeMode();
  }

  // BASIC ATTACKS
  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && state == MageState::NORMAL) {
    StartBasicAttack();
  }

  // HOLD ATTACK
  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && state == MageState::NORMAL &&
      holdTimer > 0.3f) {
    if (energy >= 10.0f) {
      energy -= 10.0f;
      StartHeavyAttack();
    } else {
      // No hay suficiente energía para el cargado, resetear carga para evitar
      // loop de intento
      holdTimer = 0.0f;
    }
  }
  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && state == MageState::NORMAL) {
    holdTimer += dt;
  } else if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
    holdTimer = 0.0f;
  }

  // E SKILL (Charge-up)
  if (IsKeyPressed(controls.berserker) && state == MageState::NORMAL) {
    bool canCast = false;
    if (currentMode == ElementMode::WATER_ICE) {
      if (canReactivateE && eMarkActive)
        canCast = true;
      else if (eCooldown <= 0.0f && energy >= 25.0f)
        canCast = true;
    } else {
      if (GetAvailableECharges() > 0 && energy >= 25.0f)
        canCast = true;
    }

    if (canCast) {
      if (currentMode == ElementMode::LIGHTNING) {
          state = MageState::CHARGING_E;
          eHoldTimer = 0.0f;
          isSuperE = false;
          castDir = facing;
      } else {
          state = MageState::CASTING_E;
          // Si es reactivación de agua, es más rápido
          eCastTimer = (canReactivateE) ? 0.12f : 0.20f; // Acelerado (era 0.15/0.25)
          castDir = facing;
      }
    }
  }

  // ULTIMATE R
  if (IsKeyPressed(controls.ultimate) && ultCooldown <= 0.0f &&
      energy >= 60.0f) {
    energy -= 60.0f;
    screenShake = fmaxf(screenShake, 3.0f); // Reducido (era 5.0) y usando fmaxf
    if (currentMode == ElementMode::WATER_ICE) {
      MageTornado t;
      t.position = targetAim;
      t.durationTimer = 0.8f; // Mas rápido
      t.pullRadius = 300.0f;
      t.explodeRadius = 250.0f;
      t.explodeDamage = 85.0f * rpg.DamageMultiplierMagical();
      t.active = true;
      t.exploded = false;
      tornados.push_back(t);
      ultCooldown = 18.0f;
    } else {
      isOverloaded = true;
      overloadTimer = 8.0f;
      overloadAuraTimer = 0.0f;
      ultCooldown = 22.0f; // Mas corto

      // Explosión visual al sobrecargar
      Graphics::SpawnImpactBurst(position, {0, -1}, YELLOW, WHITE, 25, 12);
    }
  }
}

void ElementalMage::StartBasicAttack() {
  state = MageState::ATTACKING;
  float baseTimer =
      (comboStep == 2) ? 0.32f : 0.20f; // Acelerado (era 0.4 / 0.25)
  if (attackSpeedBuffTimer > 0.0f)
    baseTimer *= 0.8f;
  attackPhaseTimer = baseTimer;
  hasHit = false;

  // Disparar proyectil (Agua) o Rayo (Rayo)
  if (currentMode == ElementMode::WATER_ICE) {
    MageProjectile p;
    p.speed = 1000.0f; // Aumentado (era 800)
    p.range = 420.0f;  // Rango reducido y unificado
    p.traveled = 0.0f;
    p.damage = 15.0f * rpg.DamageMultiplierMagical();
    p.active = true;
    p.piercing = false;
    p.isKunai = false;
    p.isCrescent = false;
    p.element = ElementMode::WATER_ICE;
    p.hitCount = 0;

    Vector2 perp = {-facing.y, facing.x};
    if (comboStep == 0) {
      p.position = Vector2Add(position, Vector2Scale(perp, 20.0f));
      p.isTargetingMouse = true;
      p.targetPos = targetAim;
    } else if (comboStep == 1) {
      p.position = Vector2Add(position, Vector2Scale(perp, -20.0f));
      p.isTargetingMouse = true;
      p.targetPos = targetAim;
    } else {
      p.position = position;
      p.isTargetingMouse = false;
      p.direction = facing;
    }

    // Si no está targeteando mouse, apuntamos via target - initPos
    if (p.isTargetingMouse) {
      Vector2 d = Vector2Subtract(p.targetPos, p.position);
      p.direction = Vector2Normalize(d);
    }

    projectiles.push_back(p);
  } else {
    // LIGHTNING
    float maxRange = 420.0f;
    Vector2 diff = Vector2Subtract(targetAim, position);
    float dist = Vector2Length(diff);
    Vector2 clampedTarget = targetAim;
    if (dist > maxRange) {
      clampedTarget =
          Vector2Add(position, Vector2Scale(Vector2Normalize(diff), maxRange));
    }

    float baseRadius = 40.0f;
    if (comboStep == 1)
      baseRadius *= 1.15f;
    if (comboStep == 2)
      baseRadius *= 1.30f;
    if (isOverloaded)
      baseRadius *= 1.3f;

    VisualHitArea area;
    area.pos = clampedTarget; // Ahora usa el target clameado
    area.radius = baseRadius;
    area.lifeTimer = 0.2f;
    area.spawnDelay = 0.0f; // Fix: initialize to zero
    area.color = YELLOW;
    area.damageDealt = false;

    float globalDmg = isOverloaded ? 1.3f : 1.0f;
    area.damage = 20.0f * rpg.DamageMultiplierMagical() * globalDmg;
    area.isHeavy = false;
    hitAreas.push_back(area);

    // VFX Lightning
    AnimeVFX::AnimeEmitter::SpawnLightning(position, clampedTarget, YELLOW);
    AnimeVFX::AnimeEmitter::SpawnAnimeImpact(clampedTarget, YELLOW);
  }
}
void ElementalMage::StartHeavyAttack() {
  state = MageState::HEAVY_ATTACK;
  float baseTimer = 0.45f; // Más rápido (era 0.6)
  if (attackSpeedBuffTimer > 0.0f)
    baseTimer *= 0.8f;
  attackPhaseTimer = baseTimer;
  hasHit = false;
  holdTimer = 0.0f;

  if (currentMode == ElementMode::WATER_ICE) {
    // Dispara hoja de hielo
    MageProjectile p;
    p.position = position;
    p.direction = facing;
    p.speed = 1050.0f; // Aumentado (era 800)
    p.range = 600.0f;  // Rango unificado
    p.traveled = 0.0f;
    p.damage = 25.0f * rpg.DamageMultiplierMagical(); // Reducido (era 45.0)
    p.active = true;
    p.piercing = true; // Atraviesa pero pega una vez (sistema hitTracking)
    p.hitCount = 0;
    p.isKunai = false;
    p.isCrescent = true; // El ataque cargado es una media luna
    p.element = ElementMode::WATER_ICE;
    p.isTargetingMouse = false;
    projectiles.push_back(p);
    screenShake = fmaxf(screenShake, 1.0f); // Reducido (era 1.5) y usando fmaxf
  } else {
    // Rayos procedurales del cielo
    int bolts = 6;
    float spacing = 95.0f; // Un poco menor para que el rango total sea ~570-600
    for (int i = 0; i < bolts; i++) {
      Vector2 boltPos =
          Vector2Add(position, Vector2Scale(facing, (i + 1) * spacing));

      VisualHitArea area;
      area.pos = boltPos;
      area.radius = 80.0f;
      area.lifeTimer = 0.3f;
      area.spawnDelay = i * 0.12f; // Procedural delay
      area.color = YELLOW;
      area.damageDealt = false;
      area.isHeavy = true; // Detona estática
      area.damage = 35.0f * rpg.DamageMultiplierMagical();
      hitAreas.push_back(area);
    }
  }
}

void ElementalMage::UpdateState(float dt) {
  if (qCooldown > 0)
    qCooldown -= dt;
  if (eCooldown > 0)
    eCooldown -= dt;
  if (ultCooldown > 0)
    ultCooldown -= dt;
  if (attackSpeedBuffTimer > 0)
    attackSpeedBuffTimer -= dt;

  if (canReactivateE) {
    eMarkTimer -= dt;
    if (eMarkTimer <= 0)
      canReactivateE = false;
  }

  for (int i = 0; i < 3; i++) {
    if (eChargesCooldowns[i] > 0)
      eChargesCooldowns[i] -= dt;
  }

  if (isOverloaded) {
    overloadTimer -= dt;
    overloadAuraTimer -= dt;
    if (overloadTimer <= 0)
      isOverloaded = false;
  }

  if (attackPhaseTimer > 0) {
    attackPhaseTimer -= dt;
    if (state == MageState::DASHING) {
      // Movimiento físico con fricción similar a Ropera
      velocity = Vector2Scale(velocity, 0.94f);
      Vector2 np = Vector2Add(position, Vector2Scale(velocity, dt));
      position = Arena::GetClampedPos(np, radius);

      static float ghostTimer = 0;
      ghostTimer += dt;
      if (ghostTimer >= 0.06f) {
        ghostTimer = 0;
        Graphics::SpawnDashTrail(position);
        Graphics::VFXSystem::GetInstance().SpawnGhost(position, {0, 0, (float)radius * 2, (float)radius * 2}, 0.25f, Fade(GetHUDColor(), 0.35f), (facing.x > 0), 1.0f, {radius, radius}, ResourceManager::texPlayer); // Usando sprite genérico
      }

      if (attackPhaseTimer <= 0) {
        state = MageState::NORMAL;
        velocity = Vector2Scale(velocity, 0.45f); // Frenazo al terminar
      }
    } else if (state == MageState::ATTACKING) {
      if (attackPhaseTimer <= 0) {
        state = MageState::NORMAL;
        comboStep = (comboStep + 1) % 3;
      }
    } else if (state == MageState::HEAVY_ATTACK) {
      position = Arena::GetClampedPos(position, radius); // Clamping consistent
      if (attackPhaseTimer <= 0) {
        state = MageState::NORMAL;
        comboStep = 0;
      }
    }
  } else {
    if (state == MageState::CASTING_E) {
      eCastTimer -= dt;
      if (eCastTimer <= 0) {
        // Ejecutar Skill E
        state = MageState::NORMAL;

        if (currentMode == ElementMode::WATER_ICE) {
          if (canReactivateE && eMarkActive && eMarkedEnemy != nullptr) {
            MageTornado explosion;
            explosion.position = eMarkedEnemy->position;
            explosion.durationTimer = 0.0f;
            explosion.explodeDamage = 50.0f * rpg.DamageMultiplierMagical();
            explosion.explodeRadius = 180.0f;
            explosion.active = true;
            explosion.exploded = false;
            tornados.push_back(explosion);

            eCooldown = 4.0f; // Cooldown tras explosion
            canReactivateE = false;
            eMarkActive = false;
          } else {
            energy -= 25.0f;
            eCooldown = 1.0f; // Pequeño delay para no spammar el lanzamiento
            MageProjectile p;
            p.position = position;
            p.direction = castDir;
            p.speed = 1200.0f;
            p.range = 800.0f;
            p.traveled = 0.0f;
            p.damage = 25.0f * rpg.DamageMultiplierMagical();
            p.active = true;
            p.piercing = false;
            p.isKunai = true;
            p.isCrescent = false;
            p.element = ElementMode::WATER_ICE;
            p.hitCount = 0;
            projectiles.push_back(p);
          }
        } else {
          energy -= 25.0f;
          int chargesReady = GetAvailableECharges();
          
          if (chargesReady > 0) {
            int chargesToConsume = isSuperE ? 3 : 1;
            if (chargesReady < chargesToConsume) chargesToConsume = chargesReady; // Safety fallback
            
            for (int i = 0, consumed = 0; i < 3 && consumed < chargesToConsume; i++) {
              if (eChargesCooldowns[i] <= 0.0f) {
                eChargesCooldowns[i] = (chargesToConsume > 1) ? 5.5f : 4.0f; // Cooldown un poco más largo para el super
                consumed++;
              }
            }
            
            MageProjectile p;
            p.position = position;
            p.direction = castDir;
            p.speed = isSuperE ? 1800.0f : 1500.0f; // Un poco más rápido si es super
            p.range = 1000.0f;
            p.traveled = 0.0f;
            
            float baseDmg = 30.0f * rpg.DamageMultiplierMagical();
            p.damage = isSuperE ? (baseDmg * 2.5f) : baseDmg;
            
            p.active = true;
            p.piercing = isSuperE; 
            p.isKunai = false;
            p.isCrescent = false;
            p.element = ElementMode::LIGHTNING;
            p.hitCount = 0;
            projectiles.push_back(p);

            if (isSuperE) {
                screenShake = fmaxf(screenShake, 2.5f);
                hitstopTimer = 0.12f;
            }
          }
        }
      }
    } else if (state == MageState::CHARGING_E) {
        eHoldTimer += dt;
        
        // Lentificación al cargar
        float slowMult = 0.4f;
        Vector2 input = {0,0};
        if (IsKeyDown(KEY_W)) input.y -= 1;
        if (IsKeyDown(KEY_S)) input.y += 1;
        if (IsKeyDown(KEY_A)) input.x -= 1;
        if (IsKeyDown(KEY_D)) input.x += 1;
        if (Vector2Length(input) > 0) {
            position = Vector2Add(position, Vector2Scale(Vector2Normalize(input), 320.0f * slowMult * dt));
        }
        
        // Orientación hacia el ratón durante la carga
        Vector2 dir = Vector2Subtract(targetAim, position);
        if (Vector2Length(dir) > 0) facing = Vector2Normalize(dir);
        castDir = facing;

        // VFX: Partículas que succionan hacia el mago
        if (GetRandomValue(0, 100) < 30) {
            float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
            float dist = (float)GetRandomValue(100, 200);
            Vector2 spawnPos = Vector2Add(position, {cosf(angle) * dist, sinf(angle) * dist});
            Vector2 vel = Vector2Scale(Vector2Normalize(Vector2Subtract(position, spawnPos)), 400.0f);
            Graphics::VFXSystem::GetInstance().SpawnParticleEx(spawnPos, vel, 0.4f, YELLOW, 4.0f, Graphics::RenderType::RHOMB, BLEND_ADDITIVE);
        }

        bool released = !IsKeyDown(controls.berserker);
        bool autoFire = (eHoldTimer >= 0.8f);
        
        if (released || autoFire) {
            state = MageState::CASTING_E;
            eCastTimer = 0.10f; // Startup rápido tras soltar
            
            // Condición para Super E: carga suficiente y 3 cargas
            if (eHoldTimer >= 0.6f && GetAvailableECharges() >= 3) {
                isSuperE = true;
            } else {
                isSuperE = false;
            }
        }
    } else if (state != MageState::NORMAL)
      state = MageState::NORMAL;
  }
}

void ElementalMage::UpdateEntities(float dt) {
  // Projectiles
  for (auto &p : projectiles) {
    if (!p.active)
      continue;
    p.position =
        Vector2Add(p.position, Vector2Scale(p.direction, p.speed * dt));
    p.traveled += p.speed * dt;
    if (p.traveled >= p.range)
      p.active = false;

    if (p.element == ElementMode::WATER_ICE) {
        Graphics::VFXSystem::GetInstance().SpawnParticleEx(
            p.position, {0, 0}, 0.2f, SKYBLUE, 3.0f, Graphics::RenderType::CIRCLE, BLEND_ALPHA);
    } else {
        // Lightning particles use Rhomb and Additive
        Graphics::VFXSystem::GetInstance().SpawnParticleEx(
            p.position, {0, 0}, 0.15f, YELLOW, 4.0f, Graphics::RenderType::RHOMB, BLEND_ADDITIVE);
    }
  }

  // Tornados
  for (auto &t : tornados) {
    if (!t.active)
      continue;
    if (!t.exploded) {
      t.durationTimer -= dt;
      if (t.durationTimer <= 0) {
        t.exploded = true;
        // Dejar activo un frame para que CheckCollisions haga el explode
      }
      Graphics::VFXSystem::GetInstance().SpawnParticleEx(
          t.position,
          {(float)GetRandomValue(-100, 100), (float)GetRandomValue(-10, 10)},
          0.4f, SKYBLUE, 6.0f);
    }
  }

  // Rays
  for (auto &lr : lightningRays) {
    lr.lifeTimer -= dt;
  }
  lightningRays.erase(
      std::remove_if(lightningRays.begin(), lightningRays.end(),
                     [](const LightningRay &r) { return r.lifeTimer <= 0; }),
      lightningRays.end());

  // Areas
  for (auto &ha : hitAreas) {
    if (ha.spawnDelay > 0) {
      ha.spawnDelay -= dt;
      if (ha.spawnDelay <= 0) {
        // VFX al aparecer
        if (ha.color.r > 200 && ha.color.g > 200) { // Lightning
            AnimeVFX::AnimeEmitter::SpawnLightning(Vector2Add(ha.pos, {0, -600}), ha.pos, YELLOW);
            AnimeVFX::AnimeEmitter::SpawnAnimeImpact(ha.pos, YELLOW);
        } else {
            Graphics::VFXSystem::GetInstance().SpawnParticleEx(ha.pos, {0, -500},
                                                               0.2f, ha.color, 12.0f, Graphics::RenderType::RHOMB, BLEND_ADDITIVE);
        }
        screenShake =
            fmaxf(screenShake, 0.6f); // Reducido (era 0.8) y usando fmaxf
      }
      continue;
    }
    ha.lifeTimer -= dt;
  }
  hitAreas.erase(std::remove_if(hitAreas.begin(), hitAreas.end(),
                                [](const VisualHitArea &h) {
                                  return h.lifeTimer <= 0 && h.spawnDelay <= 0;
                                }),
                 hitAreas.end());
}

// ===========================================================================
void ElementalMage::CheckCollisions(Boss &boss) {
  if (boss.isDead || boss.IsInvulnerable())
    return;

  float globalDmg = isOverloaded ? 1.5f : 1.0f;

  // Aura Overload (ULT LIGHTNING) - Tick rate 0.5s
  if (isOverloaded && overloadAuraTimer <= 0.0f) {
    if (CombatUtils::CheckProgressiveRadial(position, boss.position,
                                            boss.radius, 250.0f, 1.0f)) {
      boss.hp -= 18.0f * rpg.DamageMultiplierMagical();
      boss.ApplyElement(ElementMode::LIGHTNING);
      overloadAuraTimer = 0.5f; // RESET TIMER
      if (staticStackUltAvailable) {
        if (boss.staticStacks < 15)
          boss.staticStacks++;
        staticStackUltAvailable = false;
      }
      // VFX de aura
      Graphics::SpawnImpactBurst(boss.position, {0, -1}, YELLOW, WHITE, 5, 2);
    }
  }

  // Projectiles
  for (auto &p : projectiles) {
    if (!p.active)
      continue;
    
    float hitRadius = (p.isCrescent) ? 45.0f : (p.isKunai ? 22.0f : 25.0f);
    float dist = CombatUtils::GetIsoDistance(p.position, boss.position);
    if (dist <= hitRadius + boss.radius && !p.alreadyHit(&boss)) {
      p.addHit(&boss);
      boss.hp -= p.damage;
      boss.ApplyElement(p.element);

      // Otorgar carga si el ataque está disponible
      if (p.isKunai || p.element == ElementMode::LIGHTNING) {
        if (staticStackEAvailable) {
          if (boss.staticStacks < 15)
            boss.staticStacks++;
          staticStackEAvailable = false;
        }
      } else {
        if (staticStackBasicAvailable) {
          if (boss.staticStacks < 15)
            boss.staticStacks++;
          staticStackBasicAvailable = false;
        }
      }

      if (p.isKunai) {
        eMarkActive = true;
        eMarkedEnemy = &boss;
        canReactivateE = true;
        eMarkTimer = 3.0f;
      }

      // Explosion de proyectil agua basico
      if (p.element == ElementMode::WATER_ICE && !p.isKunai && !p.piercing) {
        // Generar HitArea visual y real instantáneo
        VisualHitArea v;
        v.pos = p.position;
        v.radius = 60.0f;
        v.lifeTimer = 0.2f;
        v.color = SKYBLUE;
        v.damage = 5.0f * rpg.DamageMultiplierMagical();
        v.damageDealt = false;
        v.isHeavy = false;
        hitAreas.push_back(v);
      }

      if (!p.piercing)
        p.active = false;
      Graphics::SpawnImpactBurst(boss.position, p.direction, WHITE, WHITE, 5,
                                 3);
    }
  }

  // Tornados
  for (auto &t : tornados) {
    if (!t.active)
      continue;
    float dist = CombatUtils::GetIsoDistance(t.position, boss.position);
    if (!t.exploded) {
      // Pull enemies
      if (dist <= t.pullRadius && dist > 10.0f) {
        Vector2 pullDir =
            Vector2Normalize(Vector2Subtract(t.position, boss.position));
        boss.position = Vector2Add(
            boss.position, Vector2Scale(pullDir, 80.0f * GetFrameTime()));
      }
    } else {
      // Explote
      if (dist <= t.explodeRadius) {
        boss.hp -= t.explodeDamage;
        boss.ApplyElement(ElementMode::WATER_ICE);
        if (staticStackUltAvailable) {
          if (boss.staticStacks < 15)
            boss.staticStacks++;
          staticStackUltAvailable = false;
        }

        // VFX de ICEBERG (Explosión de picos de hielo)
        for (int i = 0; i < 4; i++) {
          Vector2 off = {(float)GetRandomValue(-40, 40),
                         (float)GetRandomValue(-40, 40)};
          Graphics::VFXSystem::GetInstance().SpawnParticleEx(
              Vector2Add(boss.position, off), {0, -250}, 0.7f, WHITE, 12.0f, Graphics::RenderType::RHOMB, BLEND_ADDITIVE);
        }
        Graphics::SpawnImpactBurst(boss.position, {0, -1}, SKYBLUE, WHITE, 20,
                                   8);
        screenShake = fmaxf(screenShake, 3.0f); // Aumentado (era 2.5)
        hitstopTimer = 0.15f;
      }
      t.active =
          false; // Se limpia en UpdateEntities porque su duracion ya es 0
    }
  }

  // Rays (Detonador Rayo)
  for (auto &lr : lightningRays) {
    if (lr.lifeTimer >= 0.35f) { // Solo en los primeros 0.05 segundos impacta
      // Usando check de thrust para linea recta rápida
      if (CombatUtils::CheckProgressiveThrust(
              lr.start, Vector2Normalize(Vector2Subtract(lr.end, lr.start)),
              boss.position, boss.radius, 800.0f, 15.0f, 1.0f)) {
        // Hit detonador
        float basedmg = 50.0f * rpg.DamageMultiplierMagical() * globalDmg;
        basedmg += (boss.staticStacks * 5.0f * rpg.DamageMultiplierMagical());
        boss.hp -= basedmg;
        boss.staticStacks = 0; // Reinicia
        Graphics::SpawnImpactBurst(boss.position, {0, -1}, YELLOW, WHITE, 20,
                                   8);
      }
    }
  }

  // Hit Areas Procedurales
  for (auto &ha : hitAreas) {
    if (ha.spawnDelay > 0)
      continue;
    if (!ha.damageDealt &&
        CombatUtils::CheckProgressiveRadial(ha.pos, boss.position, boss.radius,
                                            ha.radius, 1.0f)) {
      float finalDmg = ha.damage;

      // Explosión de estática para el modo rayo pesado
      if (ha.isHeavy && boss.staticStacks > 0) {
        float explosionDmg =
            boss.staticStacks * 12.0f * rpg.DamageMultiplierMagical();
        finalDmg += explosionDmg;
        boss.staticStacks = 0;

        // VFX de explosión
        Graphics::SpawnImpactBurst(boss.position, {0, -1}, YELLOW, WHITE, 15,
                                   6);
        screenShake = fmaxf(screenShake, 2.0f);
      }

      boss.hp -= finalDmg;
      boss.ApplyElement(ElementMode::LIGHTNING);
      if (staticStackBasicAvailable) {
        if (boss.staticStacks < 15)
          boss.staticStacks++;
        staticStackBasicAvailable = false;
      }
      ha.damageDealt = true;
      Graphics::SpawnImpactBurst(boss.position, {0, -1}, ha.color, WHITE, 10,
                                 5);
      screenShake =
          fmaxf(screenShake, 0.6f); // Reducido (era 1.0) y usando fmaxf
      hitstopTimer = 0.05f;
    }
  }
}

void ElementalMage::HandleSkills(Boss &boss) {
  // Eliminado el reset del aura de aquí para que funcione correctamente en
  // CheckCollisions
}

void ElementalMage::Draw() {
  Color mag = GetHUDColor();
  DrawCircleV(position, radius, mag);

  for (auto &p : projectiles) {
    if (p.active) {
      if (p.isCrescent) {
        // Dibujar Media Luna (Crescent Moon) - Tamaño incrementado
        float baseAngle = atan2f(p.direction.y, p.direction.x) * RAD2DEG;
        Color cInner = Fade(mag, 0.8f);
        Color cOuter = mag;
        
        // Dibujamos el arco de la media luna (Radios aumentados de 18/32 a 28/48)
        DrawRing(p.position, 28.0f, 48.0f, baseAngle - 60.0f, baseAngle + 60.0f, 24, cOuter);
        DrawRing(p.position, 16.0f, 28.0f, baseAngle - 45.0f, baseAngle + 45.0f, 18, cInner);
        
        // Glow central proporcional
        DrawCircleV(p.position, 18.0f, Fade(WHITE, 0.4f));
      } else {
        DrawCircleV(p.position, (p.isKunai) ? 8.0f : 12.0f, mag);
      }
    }
  }

  for (auto &t : tornados) {
    if (t.active && !t.exploded) {
      DrawCircleLines(t.position.x, t.position.y, t.pullRadius,
                      Fade(SKYBLUE, 0.5f));
      DrawCircle(t.position.x, t.position.y, 25.0f, SKYBLUE);
    }
  }

  for (auto &lr : lightningRays) {
    DrawLineEx(lr.start, lr.end, 15.0f * (lr.lifeTimer / 0.4f), YELLOW);
  }

  for (auto &ha : hitAreas) {
    if (ha.spawnDelay > 0)
      continue;
    DrawCircleLines(ha.pos.x, ha.pos.y, ha.radius,
                    Fade(ha.color, ha.lifeTimer / 0.2f));
    // Draw bolt visual
    DrawLineEx({ha.pos.x, ha.pos.y - 400}, ha.pos,
               10.0f * (ha.lifeTimer / 0.2f), WHITE);
  }

  if (state == MageState::DASHING) {
    DrawCircleGradient((int)position.x, (int)position.y - 20, radius * 2.5f,
                       Fade(SKYBLUE, 0.4f), Fade(SKYBLUE, 0));
  }

  // --- Visual de Carga de RAYO (E) ---
  if (state == MageState::CHARGING_E) {
      float chargePct = fminf(eHoldTimer / 0.6f, 1.0f);
      Color chargeCol = (chargePct >= 1.0f && GetAvailableECharges() >= 3) ? WHITE : YELLOW;
      
      // Aura pulsante
      DrawCircleLines((int)position.x, (int)position.y - 20, radius + 5.0f + (10.0f * (1.0f - chargePct)), Fade(chargeCol, 0.6f * chargePct));
      
      // Partículas internas
      if (GetFrameTime() > 0) {
          for (int i = 0; i < (int)(chargePct * 3); i++) {
              Graphics::VFXSystem::GetInstance().SpawnParticle(
                  Vector2Add(position, {(float)GetRandomValue(-20, 20), (float)GetRandomValue(-40, 0)}),
                  {0, (float)GetRandomValue(-100, -50)},
                  0.3f, chargeCol
              );
          }
      }
      
      // Flash si está listo para el Super
      if (chargePct >= 1.0f && GetAvailableECharges() >= 3) {
          if (((int)(g_gameTime * 15) % 2) == 0) {
               DrawCircleLines((int)position.x, (int)position.y - 20, radius + 12.0f, WHITE);
          }
      }
  }

  if (isOverloaded) {
    DrawCircleLines(position.x, position.y, 250.0f, Fade(YELLOW, 0.4f));
  }
}

std::vector<AbilityInfo> ElementalMage::GetAbilities() const {
  std::vector<AbilityInfo> abs;

  float dashCooldown = (dashCooldown1 > 0 && dashCooldown2 > 0)
                           ? fminf(dashCooldown1, dashCooldown2)
                           : 0;
  abs.push_back(AbilityInfo{
      "Dash", dashCooldown, dashMaxCD, 0.0f, CanDash(), SKYBLUE, {0}});

  abs.push_back(AbilityInfo{
      "Switch Mode", qCooldown, 1.0f, 0.0f, qCooldown <= 0.0f, YELLOW, {0}});

  if (currentMode == ElementMode::WATER_ICE) {
    bool reddy = (eCooldown <= 0.0f && energy >= 25.0f) ||
                 (canReactivateE && eMarkActive);
    abs.push_back(
        AbilityInfo{canReactivateE ? "Explode Mark" : "Water Kunai [25]",
                    eCooldown,
                    5.0f,
                    25.0f,
                    reddy,
                    SKYBLUE,
                    {0}});
    abs.push_back(AbilityInfo{"Tornado D. [60]",
                              ultCooldown,
                              18.0f,
                              60.0f,
                              ultCooldown <= 0.0f && energy >= 60,
                              SKYBLUE,
                              {0}});
  } else {
    float fst = 4.0f;
    int chas = GetAvailableECharges();
    if (chas == 0)
      fst = fminf(eChargesCooldowns[0],
                  fminf(eChargesCooldowns[1], eChargesCooldowns[2]));
    abs.push_back(AbilityInfo{TextFormat("Spears [%d] [25]", chas),
                              fst,
                              4.0f,
                              25.0f,
                              chas > 0 && energy >= 25,
                              YELLOW,
                              {0}});
    abs.push_back(AbilityInfo{"Overload [60]",
                              ultCooldown,
                              22.0f,
                              60.0f,
                              ultCooldown <= 0.0f && energy >= 60,
                              YELLOW,
                              {0}});
  }

  return abs;
}
