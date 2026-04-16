// =====================================================
// Reaper.cpp - Implementacion del Personaje Segador
// =====================================================
#include "include/Reaper.h"
#include "include/CommonTypes.h"
#include "include/boss.h"
#include "include/ResourceManager.h"
#include "include/CombatUtils.h"
#include "rlgl.h"
#include <cmath>
#include <string>
#include <vector>

#include "include/graphics/VFXSystem.h"

// =====================================================
// REAPER - UPDATE
// =====================================================
void Reaper::Update() {
  float dt = GetFrameTime() * g_timeScale;

  // --- Cooldowns globales ---
  if (hitFlashTimer > 0)
    hitFlashTimer -= dt;
  UpdateDash(dt);
  if (qCooldown > 0)
    qCooldown -= dt;
  if (eCooldown > 0)
    eCooldown -= dt;
  if (ultCooldown > 0)
    ultCooldown -= dt;
  if (attackBufferTimer > 0)
    attackBufferTimer -= dt;

  // --- Buff timer (Fase 3 Ult) ---
  if (buffTimer > 0) {
    buffTimer -= dt;
    isBuffed = true;
  } else {
    isBuffed = false;
  }

  float attackMult = (isBuffed ? 0.65f : 1.0f);

  // =====================================================
  // UPDATE: Ground Bursts (Q) – cadena secuencial
  // =====================================================
  if (qActive) {
    qBurstTimer -= dt;
    if (qBurstTimer <= 0.0f && qBurstsSpawned < 5) {
      // Spawner del siguiente estallido
      bool isTip = (qBurstsSpawned == 4);
      float burstRadius = isTip ? 140.0f : 100.0f; 
      float burstDmg = isTip ? 30.5f : 19.5f; // +20% Damage

      // Posicion: avanza 75px por estallido (antes 55px) para mayor alcance y claridad
      float dist = (qBurstsSpawned + 1) * 75.0f; 
      GroundBurst &gb = groundBursts[qBurstsSpawned];
      gb.position = Vector2Add(qBurstOrigin, Vector2Scale(qBurstDir, dist));
      gb.direction = qBurstDir;
      gb.radius = burstRadius;
      gb.visualRadius = burstRadius;
      gb.lifetime = 0.45f; // Mas corto y rapido (era 0.55)
      gb.maxLifetime = 0.45f;
      gb.active = true;
      gb.hasDealtDamage = false;
      gb.isTip = isTip;
      gb.damage = burstDmg;

      qBurstsSpawned++;
      qBurstTimer = (qBurstsSpawned < 5) ? 0.06f : 0.0f; // Mas rapido (era 0.1)
    }
    if (qBurstsSpawned >= 5)
      qActive = false;
  }

  // Update de cada burst activo
  for (int i = 0; i < 5; i++)
    groundBursts[i].Update(dt);

  // =====================================================
  // UPDATE: Sombras de la Ultimate
  // =====================================================
  for (int i = 0; i < 2; i++) {
    if (ultShadows[i].active) {
      ultShadows[i].lifetime -= dt;
      ultShadows[i].position = Vector2Add(
          ultShadows[i].position, Vector2Scale(ultShadows[i].velocity, dt));
      if (ultShadows[i].lifetime <= 0)
        ultShadows[i].active = false;
    }
  }

  // =====================================================
  // MAQUINA DE ESTADOS
  // =====================================================

  // --- INPUT LOCK durante la Ult (fases 1 y 2) ---
  bool inputLocked = (ultSeqPhase == 1 || ultSeqPhase == 2);

  // --- CC Timers ---
  if (stunTimer > 0)
    stunTimer -= dt;
  if (slowTimer > 0)
    slowTimer -= dt;

  switch (state) {

  // ─── NORMAL ──────────────────────────────────────────
  case ReaperState::NORMAL: {
    if (!inputLocked && stunTimer <= 0) {
      float currentSpeed = (isBuffed ? 620.0f : 460.0f); // Aumentado (era 560/400)
      if (slowTimer > 0)
        currentSpeed *= 0.5f;

      Vector2 move = {0, 0};
      if (IsKeyDown(KEY_W))
        move.y -= 1;
      if (IsKeyDown(KEY_S))
        move.y += 1;
      if (IsKeyDown(KEY_A))
        move.x -= 1;
      if (IsKeyDown(KEY_D))
        move.x += 1;

      // Ralentizar al 50% durante la carga del heavy
      if (isCharging && holdTimer > 0)
        currentSpeed *= 0.5f;

      if (Vector2Length(move) > 0) {
        move = Vector2Normalize(move);
        Vector2 nextPos =
            Vector2Add(position, Vector2Scale(move, currentSpeed * dt));
        position = Arena::GetClampedPos(nextPos, radius);
      }

      if (state == ReaperState::NORMAL) {
        // Facing hacia el mouse solo fuera de ataques
        Vector2 aimDiff = Vector2Subtract(targetAim, position);
        if (Vector2Length(aimDiff) > 0)
          facing = Vector2Normalize(aimDiff);
      }

      // --- Hold Click: iniciar CHARGING_HEAVY ---
      if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        holdTimer += dt;
        isCharging = true;
      }

      // --- Release Click ---
      if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && isCharging) {
        if (holdTimer >= 0.35f) {
          // Ataque cargado: mini-dash + heavy attack -> impulsado mas lejos
          // (ajuste hacia adelante)
          velocity = Vector2Scale(facing, 1500.0f);
          miniDashTimer = 0.20f;
          heavyHasHit = false;
          state = ReaperState::CHARGING_HEAVY;
        } else {
          // Click rapido: combo
          state = ReaperState::ATTACKING;
          hasHit = false;
          attackPhase = AttackPhase::STARTUP;
          attackPhaseTimer = combo[comboStep].startup * attackMult;
          comboTimer = 1.2f;
        }
        holdTimer = 0.0f;
        isCharging = false;
      }

      // --- Dash (Blink) ---
      if (IsKeyPressed(controls.dash) && CanDash()) {
        Vector2 blinkDir = facing;
        {
          Vector2 move2 = {0, 0};
          if (IsKeyDown(KEY_W))
            move2.y -= 1;
          if (IsKeyDown(KEY_S))
            move2.y += 1;
          if (IsKeyDown(KEY_A))
            move2.x -= 1;
          if (IsKeyDown(KEY_D))
            move2.x += 1;
          if (Vector2Length(move2) > 0)
            blinkDir = Vector2Normalize(move2);
        }

        // --- VFX: Imagen residual en el punto de origen ---
        Graphics::VFXSystem::GetInstance().SpawnGhost(position, {0, 0, (float)radius * 2, (float)radius * 2}, 0.50f, Fade(GetHUDColor(), 0.6f), (facing.x > 0), 1.0f, {radius, radius}, ResourceManager::texPlayer);
        // Particulas de salida
        for (int k = 0; k < 12; k++)
            Graphics::VFXSystem::GetInstance().SpawnParticle(position, {(float)GetRandomValue(-300, 300), (float)GetRandomValue(-300, 300)}, 0.35f, GetHUDColor());

        Vector2 newPos =
            Vector2Add(position, Vector2Scale(blinkDir, blinkDistance));
        position = Arena::GetClampedPos(newPos, radius);
        state = ReaperState::DASHING;
        blinkGraceTimer = 0.18f; // Nueva duración reducida para mayor fluidez
        UseDash();

        // Particulas de entrada
        for (int k = 0; k < 8; k++)
            Graphics::VFXSystem::GetInstance().SpawnParticle(position, {(float)GetRandomValue(-200, 200), (float)GetRandomValue(-200, 200)}, 0.25f, WHITE);
      }

      // --- Habilidad Q: Ground Bursts secuenciales ---
      if (IsKeyPressed(controls.boomerang) && qCooldown <= 0 &&
          energy >= 18.0f) {
        energy -= 18.0f;
        qCooldown = 6.6f; // Reduced CD (~10%)
        StartGroundBurstChain();
      }

      // --- Habilidad E: Orbes Teledirigidos ---
      if (IsKeyPressed(controls.berserker) && eCooldown <= 0 &&
          energy >= 20.0f) {
        state = ReaperState::CASTING_E;
        energy -= 20.0f;
        eCooldown = 5.0f; // Buffed CD
      }

      // --- Ultimate ---
      if (IsKeyPressed(controls.ultimate) && ultCooldown <= 0 &&
          energy >= 58.0f && ultSeqPhase == 0) {
        energy -= 58.0f;
        // Señal para HandleSkills
        ultSeqPhase = -1;
      }
    }
    break;
  }

  // ─── ATTACKING (Combo de 3 hits) ─────────────────────
  case ReaperState::ATTACKING: {
    // Direccion bloqueada al inicio del golpe (User request)

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      attackBufferTimer = BUFFER_WINDOW; // Guardar pulsacion en el buffer
    }

    attackPhaseTimer -= dt;
    if (hitCooldownTimer > 0)
      hitCooldownTimer -= dt;

    if (attackPhaseTimer <= 0) {
      switch (attackPhase) {
      case AttackPhase::STARTUP:
        attackPhase = AttackPhase::ATTACK_ACTIVE;
        attackPhaseTimer = combo[comboStep].active * attackMult;
        velocity = Vector2Add(velocity, Vector2Scale(facing, 126.0f));
        hitCooldownTimer = 0.0f;
        attackId++;
        break;
      case AttackPhase::ATTACK_ACTIVE:
        attackPhase = AttackPhase::RECOVERY;
        attackPhaseTimer = combo[comboStep].recovery * attackMult;
        break;
      case AttackPhase::RECOVERY:
        comboStep = (comboStep + 1) % 3;
        if (attackBufferTimer > 0.0f) {
          attackBufferTimer = 0.0f;
          attackPhase = AttackPhase::STARTUP;
          attackPhaseTimer = combo[comboStep].startup * attackMult;
          comboTimer = 1.2f;
          hasHit = false;
        } else {
          state = ReaperState::NORMAL;
          attackPhase = AttackPhase::NONE;
          hasHit = false;
        }
        break;
      default:
        break;
      }
    }
    // Cancelar con dash
    if (IsKeyPressed(controls.dash) && CanDash()) {
      Vector2 newPos =
          Vector2Add(position, Vector2Scale(facing, blinkDistance));
      position = Arena::GetClampedPos(newPos, radius);
      state = ReaperState::DASHING;
      attackPhase = AttackPhase::NONE;
      UseDash();
      // Spawn ghost for dash cancel
      Graphics::VFXSystem::GetInstance().SpawnGhost(position, {0, 0, (float)radius * 2, (float)radius * 2}, 0.35f, Fade(GetHUDColor(), 0.5f), false, 1.0f, {radius, radius}, ResourceManager::texPlayer);
    }
    break;
  }

  // ─── CHARGING_HEAVY (Mini-Dash + Tajo Frontal) ───────
  case ReaperState::CHARGING_HEAVY: {
    miniDashTimer -= dt;
    if (miniDashTimer > 0) {
      Vector2 nextPos = Vector2Add(position, Vector2Scale(velocity, dt));
      position = Arena::GetClampedPos(nextPos, radius);
      velocity = Vector2Scale(velocity, 0.82f);
    } else {
      state = ReaperState::HEAVY_ATTACK;
      attackPhase = AttackPhase::STARTUP;
      attackPhaseTimer = 0.08f;
      velocity = {0, 0};
      heavyHasHit = false;
    }
    break;
  }

  // ─── HEAVY_ATTACK (Tajo Frontal Cargado) ─────────────
  case ReaperState::HEAVY_ATTACK: {
    attackPhaseTimer -= dt;
    if (attackPhaseTimer <= 0) {
      switch (attackPhase) {
      case AttackPhase::STARTUP:
        attackPhase = AttackPhase::ATTACK_ACTIVE;
        attackPhaseTimer = 0.20f;
        attackId++;
        break;
      case AttackPhase::ATTACK_ACTIVE:
        attackPhase = AttackPhase::RECOVERY;
        attackPhaseTimer = 0.45f;
        break;
      case AttackPhase::RECOVERY:
        state = ReaperState::NORMAL;
        attackPhase = AttackPhase::NONE;
        break;
      default:
        break;
      }
    }
    break;
  }

  // ─── DASHING (Blink – i-frames breves) ───────────────
  case ReaperState::DASHING: {
    blinkGraceTimer -= dt;
    if (blinkGraceTimer <= 0.0f) {
      state = ReaperState::NORMAL;
    }
    // Permitir input de movimiento amortiguado durante los i-frames
    float currentSpeed = (isBuffed ? 400.0f : 300.0f); 
    Vector2 move = {0, 0};
    if (IsKeyDown(KEY_W)) move.y -= 1;
    if (IsKeyDown(KEY_S)) move.y += 1;
    if (IsKeyDown(KEY_A)) move.x -= 1;
    if (IsKeyDown(KEY_D)) move.x += 1;
    if (Vector2Length(move) > 0) {
        move = Vector2Normalize(move);
        position = Vector2Add(position, Vector2Scale(move, currentSpeed * dt));
        position = Arena::GetClampedPos(position, radius);
    }
    break;
  }

  // ─── CASTING_E (Orbes Teledirigidos) ─────────────────
  case ReaperState::CASTING_E: {
    state = ReaperState::NORMAL;
    break;
  }

  // ─── LOCKED (Time Stop + Sombras + Tajo Final — input locked) ─
  case ReaperState::LOCKED: {
    velocity = {0, 0}; // Input lock total

    if (ultSeqPhase == 1) { // Fase de Sombras
      ultSeqTimer -= dt;
      if (ultSeqTimer <= 0) {
        ultSeqPhase = 2;
        attackPhase = AttackPhase::STARTUP;
        attackPhaseTimer = 0.20f; // Startup breve cinematico
        ultFinalSlash = true;
        ultFinalSlashHit = false;
        attackId++;
      }
    } else if (ultSeqPhase == 2) { // Fase de Tajo Final Automático
      attackPhaseTimer -= dt;
      if (attackPhaseTimer <= 0) {
        switch (attackPhase) {
        case AttackPhase::STARTUP:
          attackPhase = AttackPhase::ATTACK_ACTIVE;
          attackPhaseTimer = 0.25f;
          break;
        case AttackPhase::ATTACK_ACTIVE:
          attackPhase = AttackPhase::RECOVERY;
          attackPhaseTimer = 0.50f;
          break;
        case AttackPhase::RECOVERY:
          isTimeStopped = false;
          ultSeqPhase = 3; // Buff
          buffTimer = 6.0f;
          ultCooldown = 20.5f; // Reduced CD (~10%)
          state = ReaperState::ULT_PHASE3;
          attackPhase = AttackPhase::NONE;
          ultFinalSlash = false;
          break;
        default:
          break;
        }
      }
    }
    break;
  }

  // ─── ULT_PHASE3 (Buff activo – movimiento libre) ─────
  case ReaperState::ULT_PHASE3: {
    if (buffTimer <= 0) {
      state = ReaperState::NORMAL;
      isBuffed = false;
      ultSeqPhase = 0;
    }
    // Movimiento libre durante el buff
    {
      float currentSpeed = 560.0f; // Buffed speed
      Vector2 move = {0, 0};
      if (IsKeyDown(KEY_W))
        move.y -= 1;
      if (IsKeyDown(KEY_S))
        move.y += 1;
      if (IsKeyDown(KEY_A))
        move.x -= 1;
      if (IsKeyDown(KEY_D))
        move.x += 1;
      if (Vector2Length(move) > 0) {
        move = Vector2Normalize(move);
        Vector2 nextPos =
            Vector2Add(position, Vector2Scale(move, currentSpeed * dt));
        position = Arena::GetClampedPos(nextPos, radius);
      }
      Vector2 aimDiff = Vector2Subtract(targetAim, position);
      if (Vector2Length(aimDiff) > 0)
        facing = Vector2Normalize(aimDiff);
      // Habilidades basicas disponibles durante el buff
      if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        state = ReaperState::ATTACKING;
        hasHit = false;
        attackPhase = AttackPhase::STARTUP;
        attackPhaseTimer =
            combo[comboStep].startup * 0.65f; // attackMult buffed
        comboTimer = 1.2f;
      }
      if (IsKeyPressed(controls.dash) && CanDash()) {
        Vector2 blinkDir = (Vector2Length(move) > 0) ? move : facing;
        Vector2 newPos =
            Vector2Add(position, Vector2Scale(blinkDir, blinkDistance));
        position = Arena::GetClampedPos(newPos, radius);
        state = ReaperState::DASHING;
        UseDash();
      }
    }
    break;
  }
  } // end switch

  // --- Friccion general (excepto Input Lock) ---
  if (!inputLocked)
    velocity = Vector2Scale(velocity, 0.88f);

  // --- Fisica base (excepto Input Lock) ---
  if (!inputLocked) {
    Vector2 nextPhysPos = Vector2Add(position, Vector2Scale(velocity, dt));
    position = Arena::GetClampedPos(nextPhysPos, radius);
  }

  // --- Combo timer reset ---
  if (state != ReaperState::ATTACKING && comboTimer > 0) {
    comboTimer -= dt;
    if (comboTimer <= 0)
      comboStep = 0;
  }

  // =====================================================
  // UPDATE: Proyectiles activos (Homing orbs / sombras)
  // =====================================================
  for (int i = (int)activeProjectiles.size() - 1; i >= 0; i--) {
    Projectile &p = activeProjectiles[i];
    if (!p.active) {
      activeProjectiles.erase(activeProjectiles.begin() + i);
      continue;
    }

    // Trail
    for (int j = 7; j > 0; j--) {
      p.trail[j] = p.trail[j - 1];
    }
    p.trail[0] = p.position;
    if (p.trailCount < 8)
      p.trailCount++;

    if (p.isShadow || p.isHoming) {
      p.position =
          Vector2Add(p.position, Vector2Scale(p.direction, p.speed * dt));
      if (Vector2Distance(p.startPos, p.position) > p.maxDistance)
        p.active = false;
    }
  }
}

void Reaper::Reset(Vector2 pos) {
  position = pos;
  hp = maxHp;
  energy = 100.0f;
  velocity = {0, 0};
  activeProjectiles.clear();
  state = ReaperState::NORMAL;
  ultSeqPhase = 0;
  ultCooldown = 0.0f;
  buffTimer = 0.0f;
  isBuffed = false;
  qCooldown = 0.0f;
  dashCharges = maxDashCharges;
  dashCooldown1 = 0.0f;
  dashCooldown2 = 0.0f;
  eCooldown = 0.0f;
  comboStep = 0;
  prevReaperState = ReaperState::NORMAL;
}

void Reaper::HandleSkills(Boss &boss) {
  float dt = GetFrameTime() * g_timeScale;
  // --- Detectar señal de Ult (ultSeqPhase == -1 → ActivateUltimate) ---
  if (ultSeqPhase == -1) {
    ActivateUltimate(boss.position);
    // Efecto de entrada
    for (int si = 0; si < 20; si++) {
      Graphics::VFXSystem::GetInstance().SpawnParticle(
          position,
          {(float)GetRandomValue(-350, 350), (float)GetRandomValue(-350, 350)},
          0.9f, {0, 200, 255, 255});
    }
  }

  // --- Detectar entrada en CASTING_E para lanzar orbes ---
  if (prevReaperState != state) {
    if (state == ReaperState::CASTING_E) {
      LaunchHomingOrbs(boss);
    }
    prevReaperState = state;
  }

  // --- Homing: mover orbes hacia el boss ---
  for (auto &p : activeProjectiles) {
    if (p.active && p.isHoming && !boss.isDead && !boss.IsInvulnerable() && boss.isBleeding) {
      Vector2 toEnemy = Vector2Subtract(boss.position, p.position);
      if (Vector2Length(toEnemy) > 1.0f) {
        Vector2 desired = Vector2Normalize(toEnemy);
        p.direction.x += (desired.x - p.direction.x) * p.homingStrength * dt;
        p.direction.y += (desired.y - p.direction.y) * p.homingStrength * dt;
        float len = Vector2Length(p.direction);
        if (len > 0)
          p.direction = Vector2Scale(p.direction, 1.0f / len);
      }
    }
  }
}

void Reaper::CheckCollisions(Boss &boss) {
  if (boss.isDead || boss.isDying || boss.IsInvulnerable())
    return;

  // --- Combo (3 hits) ---
  if (CheckComboCollision(boss)) {
    float dmg = combo[comboStep].damage;
    if (comboStep < 2) {
      dmg *= rpg.DamageMultiplierPhysical();
      lastDamageType = DamageType::PHYSICAL;
    } else {
      dmg *= rpg.DamageMultiplierMixed();
      lastDamageType = DamageType::MIXED;
    }
    
    boss.hp -= dmg;
    energy = fminf(maxEnergy, energy + 7.5f);

    // Curacion si el enemigo esta sangrando
    if (boss.isBleeding) {
      hp = fminf(maxHp, hp + dmg * 0.12f);
      Graphics::SpawnImpactBurst(position, {0, -1}, {255, 50, 50, 255}, {255, 100, 100, 255}, 5, 2);
    }

    // Hitstop y empuje (reducido 50%)
    hitstopTimer = 0.08f;
    velocity = Vector2Add(velocity, Vector2Scale(facing, 297.0f));

    // Solo el ultimo ataque de la secuencia empujaba al boss (desactivado)
    if (comboStep == 2) {
      screenShake = fmaxf(screenShake, 1.8f); // Reducido (era 2.5) y usando fmaxf
    }
  }

  // --- Heavy Attack (Tajo frontal cargado) ---
  if (CheckHeavyCollision(boss)) {
    float dmg = 82.0f * rpg.DamageMultiplierMagical(); // Damage +15%
    lastDamageType = DamageType::MAGICAL;
    boss.hp -= dmg;
    energy = fminf(maxEnergy, energy + 12.5f);

    hitstopTimer = 0.12f;
    velocity =
        Vector2Add(velocity, Vector2Scale(facing, 420.0f)); // reducido 50%
  }

  // --- Ground Bursts Q: colision hit-once con el boss ---
  for (int i = 0; i < 5; i++) {
    GroundBurst &gb = groundBursts[i];
    if (!gb.active || gb.hasDealtDamage)
      continue;
    // Usando la misma logica radial isometrica que el cargado del mago pero siempre al radio maximo
    if (CombatUtils::CheckProgressiveRadial(gb.position, boss.position, boss.radius, gb.radius, 1.0f)) {
      gb.hasDealtDamage = true;
      lastDamageType = DamageType::MAGICAL;
      boss.hp -= (gb.damage * rpg.DamageMultiplierMagical());
      boss.ApplyBleed();
    }
  }

  // --- Orbes Homing E: colision con el boss ---
  for (auto &p : activeProjectiles) {
    if (!p.active || !p.isHoming)
      continue;
    if (CombatUtils::GetIsoDistance(p.position, boss.position) <= 14.0f + boss.radius) {
      p.active = false;
      lastDamageType = DamageType::MAGICAL;
      boss.hp -= (20.2f * rpg.DamageMultiplierMagical()); // Damage +20%
    }
  }

  // --- Ultimate Fase 2: Tajo Final AUTOMATICO → DoT Pop ---
  if (CheckUltFinalSlash(boss)) {
    float baseUltDmg = 138.0f * rpg.DamageMultiplierMagical(); // Damage +20%
    float popDmg = boss.GetRemainingBleedDamage();
    float totalDmg = baseUltDmg + popDmg;

    lastDamageType = DamageType::DoT;
    boss.hp -= totalDmg;
    boss.bleedTimer = 0;
    boss.isBleeding = false;

    // La curación solo depende del sangrado detonado, no del daño base de 80
    if (popDmg > 0) {
      hp = fminf(maxHp, hp + popDmg);
    }
  }
}

std::vector<AbilityInfo> Reaper::GetAbilities() const {
  std::vector<AbilityInfo> abs;
  float currentDashCD =
      (dashCooldown1 > 0 && dashCooldown2 > 0)
          ? fminf(dashCooldown1, dashCooldown2)
          : ((dashCooldown1 > 0) ? dashCooldown1 : dashCooldown2);
  abs.push_back({TextFormat("DASH [%d]", dashCharges),
                 currentDashCD,
                 dashMaxCD,
                 0.0f,
                 CanDash(),
                 {180, 0, 255, 255},
                 ResourceManager::texBoomerang /* Placeholder for Dash */});
  abs.push_back({"Q Sangre",
                 qCooldown,
                 7.2f,
                 18.0f,
                 qCooldown <= 0 && energy >= 18.0f,
                 {220, 0, 255, 255},
                 ResourceManager::reaperQ});
  abs.push_back({"E Orbes",
                 eCooldown,
                 5.0f,
                 20.0f,
                 eCooldown <= 0 && energy >= 20.0f,
                 {255, 60, 255, 255},
                 ResourceManager::reaperE});
  abs.push_back({"R Ult",
                 ultCooldown,
                 22.5f,
                 58.0f,
                 ultCooldown <= 0 && energy >= 58.0f,
                 {0, 200, 255, 255},
                 ResourceManager::reaperR});
  return abs;
}

std::string Reaper::GetSpecialStatus() const {
  if (state == ReaperState::LOCKED && ultSeqPhase == 1)
    return "<<< TIEMPO DETENIDO >>>";
  if (state == ReaperState::LOCKED && ultSeqPhase == 2)
    return "[ TAJO FINAL ]";
  if (qActive)
    return TextFormat("SANGRE [%d/5]", qBurstsSpawned);
  return "";
}

// =====================================================
// REAPER - START GROUND BURST CHAIN (Habilidad Q)
// =====================================================
void Reaper::StartGroundBurstChain() {
  qActive = true;
  qBurstsSpawned = 0;
  qBurstTimer = 0.0f; // El primero aparece inmediatamente
  qBurstOrigin = position;
  qBurstDir = facing;
  // Resetear todos
  for (int i = 0; i < 5; i++)
    groundBursts[i].active = false;
}

// =====================================================
// REAPER - PROGRESSIVE SWEEP SYSTEM
// =====================================================

// Parametros de barrido por combo step:
//  step 0: de izquierda a derecha (-90 -> +90 relativo al facing)
//  step 1: de derecha a izquierda (+90 -> -90, sentido opuesto)
//  step 2: giro 360 completo (siempre hit dentro de rango)
struct SweepParams { float startOffset; float totalDeg; float dir; };
static SweepParams GetSweepParams(int step) {
  switch (step) {
    case 0: return { -90.0f, 180.0f, +1.0f }; // izq -> der (CCW)
    case 1: return { +90.0f, 180.0f, -1.0f }; // der -> izq (CW)
    default: return {   0.0f, 360.0f, +1.0f }; // spin completo
  }
}

// =====================================================
// REAPER - CHECK COMBO COLLISION
// Logica progresiva: el golpe solo conecta cuando el
// 'filo' del tajo alcanza el angulo del enemigo.
// =====================================================
bool Reaper::CheckComboCollision(Boss &boss) {
  if (state != ReaperState::ATTACKING ||
      attackPhase != AttackPhase::ATTACK_ACTIVE)
    return false;
  if (hitCooldownTimer > 0.0f)
    return false;

  // --- Distancia isometrica al boss ---
  Vector2 diff  = Vector2Subtract(boss.position, position);
  float   isoY  = diff.y * 2.0f;  // comprimir Y x2 para perspectiva iso
  float   isoDist = sqrtf(diff.x * diff.x + isoY * isoY);
  float   totalRange = combo[comboStep].range + boss.radius;
  if (isoDist >= totalRange) return false; // fuera de alcance

  // --- Progreso del barrido en este frame ---
  float attackMult = isBuffed ? 0.65f : 1.0f;
  float activeTotal = combo[comboStep].active * attackMult;
  float progress    = CombatUtils::GetProgress(attackPhaseTimer, activeTotal) * 1.4f;
  if (progress > 1.0f) progress = 1.0f;

  SweepParams sp = GetSweepParams(comboStep);

  if (CombatUtils::CheckProgressiveSweep(position, facing, boss.position, boss.radius, 
                                         combo[comboStep].range, sp.startOffset, sp.totalDeg, sp.dir, progress)) {
    hitCooldownTimer = combo[comboStep].hitCooldown;
    hasHit = true;
    return true;
  }
  return false;
}

// =====================================================
// REAPER - CHECK HEAVY COLLISION (Tajo frontal)
// =====================================================
bool Reaper::CheckHeavyCollision(Boss &boss) {
  if (state != ReaperState::HEAVY_ATTACK ||
      attackPhase != AttackPhase::ATTACK_ACTIVE)
    return false;
  if (heavyHasHit)
    return false;

  // Tajo frontal cargado: barrido de 80 grados
  float progress = CombatUtils::GetProgress(attackPhaseTimer, 0.20f) * 1.4f;
  if (progress > 1.0f) progress = 1.0f;
  if (CombatUtils::CheckProgressiveSweep(position, facing, boss.position, boss.radius, 200.0f, -40.0f, 80.0f, 1.0f, progress)) {
    heavyHasHit = true;
    return true;
  }
  return false;
}

// =====================================================
// REAPER - CHECK ULT FINAL SLASH
// =====================================================
bool Reaper::CheckUltFinalSlash(Boss &boss) {
  if (state != ReaperState::LOCKED || ultSeqPhase != 2)
    return false;
  if (attackPhase != AttackPhase::ATTACK_ACTIVE)
    return false;
  if (ultFinalSlashHit)
    return false;

  float progress = CombatUtils::GetProgress(attackPhaseTimer, 0.25f) * 1.4f;
  if (progress > 1.0f) progress = 1.0f;
  // El tajo final es un barrido de 200° centrado en facing (de -100 a +100)
  if (CombatUtils::CheckProgressiveSweep(position, facing, boss.position, boss.radius, 253.0f, -100.0f, 200.0f, 1.0f, progress)) {
    ultFinalSlashHit = true;
    return true;
  }
  return false;
}

// =====================================================
// REAPER - LAUNCH HOMING ORBS (Habilidad E)
// =====================================================
void Reaper::LaunchHomingOrbs(Boss &boss) {
  float orbSpeed = 650.0f; // Aumentado un poco solamente (antes 500.0f)

  if (boss.isBleeding) {
    // Lanzamiento en cono dirigido si el boss ya sangra (5 orbes en ~56 grados
    // total)
    for (int i = 0; i < 5; i++) {
      float spreadAngle = ((float)i - 2.0f) * 28.0f;
      float baseAngle = atan2f(facing.y, facing.x) * RAD2DEG + spreadAngle;
      float rad = baseAngle * DEG2RAD;
      Vector2 dir = {cosf(rad), sinf(rad)};

      Projectile orb = {};
      orb.position = position;
      orb.startPos = position;
      orb.direction = dir;
      orb.maxDistance = 1500.0f;
      orb.active = true;
      orb.damage = 16.8f; // +20% adicional
      orb.isHoming = true;
      orb.homingStrength = 3.5f;
      orb.speed = orbSpeed;

      activeProjectiles.push_back(orb);
    }
  } else {
    // Si no hay sangrado, se dispersan en un angulo de 180 hacia delante
    const int numOrbs = 5;
    for (int i = 0; i < numOrbs; i++) {
      // De -90 a 90 grados respecto al facing
      float spreadAngle = ((float)i - 2.0f) * 45.0f;
      float baseAngle = atan2f(facing.y, facing.x) * RAD2DEG + spreadAngle;
      float rad = baseAngle * DEG2RAD;
      Vector2 dir = {cosf(rad), sinf(rad)};

      Projectile orb = {};
      orb.position = position;
      orb.startPos = position;
      orb.direction = dir;
      orb.maxDistance = 1500.0f;
      orb.active = true;
      orb.damage = 16.8f;
      orb.isHoming = true;
      orb.homingStrength = 3.5f;
      orb.speed = orbSpeed;

      activeProjectiles.push_back(orb);
    }
  }
}

// =====================================================
// REAPER - ACTIVATE ULTIMATE
// Recibe la posicion actual del boss para posicionar
// las sombras correctamente en forma de X.
// =====================================================
void Reaper::ActivateUltimate(Vector2 bossPos) {
  state = ReaperState::LOCKED;
  isTimeStopped = true;
  ultSeqTimer = 1.8f; // Duracion de la fase de sombras
  ultSeqPhase = 1;
  ultFinalSlash = false;
  ultFinalSlashHit = false;

  // Limpiar sombras previas
  for (int i = 0; i < 2; i++)
    ultShadows[i].active = false;

  // Calcular la X: dos sombras que flanquean al jefe y cruzan en X
  Vector2 toEnemy = Vector2Subtract(bossPos, position);
  float dist = CombatUtils::GetIsoDistance(position, bossPos);
  if (dist < 1.0f) {
    toEnemy = facing;
    dist = 1.0f;
  }
  Vector2 dirFwd = Vector2Normalize(toEnemy);
  Vector2 dirPerp = {-dirFwd.y, dirFwd.x}; // perpendicular

  float offset = 350.0f; // distancia de separacion en cada eje

  // Sombra 0: esquina superior-izquierda a esquina inferior-derecha (hacia el
  // boss)
  Vector2 corner0 =
      Vector2Add(Vector2Scale(dirFwd, -offset), Vector2Scale(dirPerp, offset));
  ultShadows[0].position = Vector2Add(bossPos, corner0);
  ultShadows[0].velocity = Vector2Scale(Vector2Normalize(corner0), -2800.0f);
  ultShadows[0].lifetime = 0.35f;
  ultShadows[0].active = true;

  // Sombra 1: esquina superior-derecha a esquina inferior-izquierda
  Vector2 corner1 =
      Vector2Add(Vector2Scale(dirFwd, -offset), Vector2Scale(dirPerp, -offset));
  ultShadows[1].position = Vector2Add(bossPos, corner1);
  ultShadows[1].velocity = Vector2Scale(Vector2Normalize(corner1), -2800.0f);
  ultShadows[1].lifetime = 0.35f;
  ultShadows[1].active = true;

  // Orientar el facing del Reaper hacia el boss para el tajo final
  if (dist > 1.0f)
    facing = dirFwd;
}

// =====================================================
// REAPER - DRAW
// =====================================================
void Reaper::Draw() {
  float t = (float)g_gameTime;

  // --- Ground Bursts (Q) en el suelo ---
  for (int i = 0; i < 5; i++)
    groundBursts[i].Draw();

  // --- Sombras de la Ultimate ---
  for (int i = 0; i < 2; i++) {
    if (!ultShadows[i].active)
      continue;
    float alpha = ultShadows[i].lifetime * 3.0f;
    if (alpha > 1.0f)
      alpha = 1.0f;
    DrawCircleV(ultShadows[i].position, 24.0f, Fade({0, 200, 255, 255}, alpha));
    DrawCircleLines((int)ultShadows[i].position.x,
                    (int)ultShadows[i].position.y, 28.0f,
                    Fade(WHITE, alpha * 0.8f));
    // Estela
    DrawCircleV(
        Vector2Add(
            ultShadows[i].position,
            Vector2Scale(Vector2Normalize(ultShadows[i].velocity), -35.0f)),
        16.0f, Fade({0, 150, 255, 255}, alpha * 0.4f));
  }

  // --- Sombra proyectada se maneja en GameplayScene ---
  // DrawEllipse((int)position.x, (int)position.y, radius, radius * 0.5f,
  // Fade(BLACK, 0.4f));

  // --- Aura vampirica ---
  float auraPulse = 0.3f + 0.15f * sinf(t * 3.5f);
  if (isBuffed) {
    DrawCircleGradient((int)position.x, (int)position.y - 20, radius * 2.8f,
                       Fade({220, 0, 255, 255}, auraPulse),
                       Fade({80, 0, 120, 255}, 0));
  } else {
    DrawCircleGradient((int)position.x, (int)position.y - 20, radius * 1.6f,
                       Fade({160, 0, 220, 200}, auraPulse * 0.6f),
                       Fade({80, 0, 120, 255}, 0));
  }

  // --- Cuerpo del Segador ---
  Color reaperColor;
  if (hitFlashTimer > 0)
    reaperColor = WHITE;
  else if (ultSeqPhase == 1 || ultSeqPhase == 2)
    reaperColor = {0, 200, 255, 255}; // Cyan Ult
  else if (isBuffed)
    reaperColor = {230, 50, 255, 255};
  else
    reaperColor = {160, 0, 220, 255};

  DrawCircleV({position.x, position.y - 20}, radius, reaperColor);
  DrawCircleLines((int)position.x, (int)position.y - 20, radius - 3,
                  Fade(WHITE, 0.5f));

  // === Visual del Combo: sector progresivo ===================================
  // El sector CRECE en tiempo real conforme la animacion avanza,
  // mostrando exactamente que region ya ha sido barrida por el filo.
  if (state == ReaperState::ATTACKING) {
    float attackMult  = isBuffed ? 0.65f : 1.0f;
    float range       = combo[comboStep].range;
    float facingAngle = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
    Color comboCol    = isBuffed ? Color{255, 140, 0, 255} : Color{200, 0, 255, 255};

    if (attackPhase == AttackPhase::STARTUP) {
      // Indicador anticipatorio: sectorcito pequeno donde comenzara el tajo
      SweepParams sp = GetSweepParams(comboStep);
      float startA   = facingAngle + sp.startOffset;
      float previewA = startA + sp.dir * 25.0f; // solo 25 grados de preview
      rlPushMatrix();
      rlTranslatef(position.x, position.y, 0);
      rlScalef(1.0f, 0.5f, 1.0f);
      DrawCircleSector({0,0}, range * 0.55f,
                       fminf(startA, previewA), fmaxf(startA, previewA),
                       12, Fade(comboCol, 0.18f));
      rlPopMatrix();
    } else if (attackPhase == AttackPhase::ATTACK_ACTIVE) {
      // ── PROGRESIVO: sector crece desde startAngle hasta leadingAngle ──
      float activeTotal = combo[comboStep].active * attackMult;
      float progress    = CombatUtils::GetProgress(attackPhaseTimer, activeTotal);

      if (comboStep == 2) {
        // Hit 3: spin 360 – el circulo se rellena completamente
        float coveredDeg = 360.0f * progress;
        rlPushMatrix();
        rlTranslatef(position.x, position.y, 0);
        rlScalef(1.0f, 0.5f, 1.0f);
        DrawCircleSector({0,0}, range, facingAngle, facingAngle + coveredDeg,
                         36, Fade(comboCol, 0.55f));
        // Borde del arco ya cubierto
        DrawCircleSectorLines({0,0}, range, facingAngle,
                              facingAngle + coveredDeg, 36, Fade(WHITE, 0.5f));
        // Filo (leading edge)
        float le = (facingAngle + coveredDeg) * DEG2RAD;
        DrawLineEx({0,0}, {cosf(le)*range, sinf(le)*range},
                   3.0f, WHITE);
        rlPopMatrix();
      } else {
        // Hits 1 y 2: sector que barre de un lado al otro
        SweepParams sp   = GetSweepParams(comboStep);
        float coveredDeg = sp.totalDeg * progress;
        float startA     = facingAngle + sp.startOffset;
        float endA       = startA + sp.dir * coveredDeg;
        float secMin     = fminf(startA, endA);
        float secMax     = fmaxf(startA, endA);

        rlPushMatrix();
        rlTranslatef(position.x, position.y, 0);
        rlScalef(1.0f, 0.5f, 1.0f);

        // Fill region ya barrida
        DrawCircleSector({0,0}, range, secMin, secMax,
                         24, Fade(comboCol, 0.55f));

        // Borde exterior del area cubierta
        DrawCircleSectorLines({0,0}, range, secMin, secMax,
                              24, Fade(WHITE, 0.45f));

        // Filo (leading edge): linea brillante en el angulo actual del tajo
        float leRad = endA * DEG2RAD;
        DrawLineEx({0,0}, {cosf(leRad)*range, sinf(leRad)*range},
                   3.5f, WHITE);
        // Destello en la punta del filo
        DrawCircleV({cosf(leRad)*range, sinf(leRad)*range},
                    5.0f, Fade(WHITE, 0.9f));

        rlPopMatrix();
      }
    } else { // RECOVERY: muestra el area cubierta desvanecida
      SweepParams sp = GetSweepParams(comboStep);
      float startA   = facingAngle + sp.startOffset;
      float endA     = startA + sp.dir * sp.totalDeg;
      float secMin   = fminf(startA, endA);
      float secMax   = fmaxf(startA, endA);
      if (comboStep == 2) { secMin = facingAngle; secMax = facingAngle + 360.0f; }
      rlPushMatrix();
      rlTranslatef(position.x, position.y, 0);
      rlScalef(1.0f, 0.5f, 1.0f);
      DrawCircleSector({0,0}, range, secMin, secMax,
                       24, Fade(comboCol, 0.12f));
      rlPopMatrix();
    }
  }

  // --- Visual del Heavy Attack / Ult Final Slash ---
  bool isUltSlash =
      (state == ReaperState::LOCKED && ultSeqPhase == 2 && ultFinalSlash);
  if (state == ReaperState::HEAVY_ATTACK || isUltSlash) {
    float slashAngle = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
    float halfW = isUltSlash ? 100.0f : 40.0f; // Ult slash mas ancho
    float slashRange = isUltSlash ? 253.0f : 200.0f;
    Color slashCol =
        isUltSlash ? Color{0, 220, 255, 255} : Color{255, 50, 200, 255};
    float maxTotalTime = isUltSlash ? 0.25f : 0.20f;

    rlPushMatrix();
    rlTranslatef(position.x, position.y, 0);
    rlScalef(1.0f, 0.5f, 1.0f);

    if (attackPhase == AttackPhase::STARTUP) {
        // Area hint
        DrawCircleSector({0, 0}, slashRange * 0.55f, slashAngle - halfW, slashAngle + halfW, 24, Fade(slashCol, 0.18f));
    } else if (attackPhase == AttackPhase::ATTACK_ACTIVE) {
        float progressArr = CombatUtils::GetProgress(attackPhaseTimer, maxTotalTime) * 1.4f;
        if (progressArr > 1.0f) progressArr = 1.0f;
        float coveredDeg = (halfW * 2.0f) * progressArr;
        
        float startA = slashAngle - halfW;
        float currentA = startA + coveredDeg;

        DrawCircleSector({0,0}, slashRange, startA, currentA, 36, Fade(slashCol, 0.55f));
        DrawCircleSectorLines({0,0}, slashRange, startA, currentA, 36, Fade(WHITE, 0.5f));
        
        // Leading edge (manecilla)
        float le = currentA * DEG2RAD;
        DrawLineEx({0,0}, {cosf(le)*slashRange, sinf(le)*slashRange}, 4.0f, WHITE);
        // Destello en la punta
        DrawCircleV({cosf(le)*slashRange, sinf(le)*slashRange}, 6.0f, Fade(WHITE, 0.9f));

    } else if (attackPhase == AttackPhase::RECOVERY) {
        // Fade out del area total afectada
        DrawCircleSector({0, 0}, slashRange, slashAngle - halfW, slashAngle + halfW, 24, Fade(slashCol, 0.12f));
    }
    rlPopMatrix();
  }

  // --- Visual de Carga (Hold Click) ---
  if (isCharging && holdTimer > 0) {
    float chargePct = fminf(holdTimer / 0.35f, 1.0f);
    DrawCircleLines((int)position.x, (int)position.y - 20,
                    radius + 8.0f + 20.0f * (1.0f - chargePct),
                    Fade({200, 0, 255, 255}, chargePct));
    DrawCircleGradient(
        (int)position.x, (int)position.y - 20, radius * 2.0f * chargePct,
        Fade({200, 0, 255, 160}, 0.5f), Fade({200, 0, 255, 255}, 0));
    if (chargePct >= 1.0f) {
      float blink = sinf(t * 30.0f);
      if (blink > 0)
        DrawCircleLines((int)position.x, (int)position.y - 20, radius + 12,
                        WHITE);
    }
  }

  // --- Visual Blink ---
  if (state == ReaperState::DASHING) {
    DrawCircleGradient((int)position.x, (int)position.y - 20, radius * 2.5f,
                       Fade({200, 0, 255, 255}, 0.4f),
                       Fade({200, 0, 255, 255}, 0));
  }

  // --- Orbes homing activos ---
  for (auto &p : activeProjectiles) {
    if (!p.active)
      continue;
    float pRad = p.isShadow ? 8.0f : (p.isHoming ? 14.0f : 10.0f);
    Color pCol = p.isShadow ? Fade({0, 200, 255, 255}, 0.6f)
                            : (p.isHoming ? Color{255, 60, 255, 255}
                                          : Color{220, 0, 255, 255});

    for (int i = 0; i < p.trailCount; i++) {
      float ts = 1.0f - ((float)i / p.trailCount);
      DrawCircleV(p.trail[i], pRad * ts * 0.7f, Fade(pCol, 0.4f * ts));
    }
    DrawCircleV(p.position, pRad, pCol);
    DrawCircleLines((int)p.position.x, (int)p.position.y, pRad + 2.0f,
                    Fade(WHITE, 0.6f));
  }
}
