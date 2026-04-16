// =====================================================
// Ropera.cpp  v2.0
// - Dash independiente con i-frames reales
// - Combo: Estocada / Tajo90 / Paso-atras+Rafaga-x3
// - Ataque cargado: consume dash, super estocada
// - Q: dos tajos angulo cerrado, buff velocidad si golpea
// - E: +velocidad ataque, lifesteal 20%, +3% maxHp enemigo en basicos
// - R (Garras, <60% hp): espadas detras que se disparan escalonadas al golpear
// =====================================================
// =====================================================
#include "include/Ropera.h"
#include "include/CommonTypes.h"
#include "include/boss.h"
#include "include/ResourceManager.h"
#include "include/CombatUtils.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "include/graphics/VFXSystem.h"

// ─── Perp 2D ──────────────────────────────────────────
static Vector2 Perp2D(Vector2 v) { return {-v.y, v.x}; }

// =====================================================
// InitSwords – activa las 3 espadas y las coloca atras
// =====================================================
void Ropera::InitSwords() {
  for (int i = 0; i < 3; i++) {
    swords[i].active = true;
    swords[i].swordState = SwordState::BEHIND;
    swords[i].hasDealt = false;
    swords[i].flashTimer = 0.0f;
    swords[i].fireDelayTimer = 0.0f;
  }
}

// =====================================================
// TriggerSwords – inicia el vuelo escalonado al golpear
// =====================================================
void Ropera::TriggerSwords(Vector2 enemyPos) {
  swordTargetSnapshot = enemyPos;
  for (int i = 0; i < 3; i++) {
    if (!swords[i].active)
      continue;
    if (swords[i].swordState != SwordState::BEHIND)
      continue; // ya disparada
    swords[i].swordState = SwordState::FIRING;
    swords[i].targetPos = enemyPos;
    swords[i].hasDealt = false;
    swords[i].fireDelayTimer = swords[i].fireDelay; // 0, 0.1, 0.2s de delay
  }
}

// =====================================================
// UpdateSwords – actualiza la maquina de la espada
// =====================================================
void Ropera::UpdateSwords(float dt, Boss &boss) {
  if (!ultActive)
    return;

  Vector2 perpFacing = Perp2D(facing);
  float offsets[3] = {0.0f, -22.0f, 22.0f}; // dispersion lateral

  for (int i = 0; i < 3; i++) {
    if (!swords[i].active)
      continue;
    if (swords[i].flashTimer > 0)
      swords[i].flashTimer -= dt;

    switch (swords[i].swordState) {

    case SwordState::BEHIND: {
      // Flotar detras del jugador con pequena dispersion lateral
      Vector2 behind = Vector2Add(
          position, Vector2Scale(Vector2Negate(facing), swordBehindDist));
      Vector2 target = Vector2Add(behind, Vector2Scale(perpFacing, offsets[i]));
      // Lerp suave hacia la posicion de reposo
      swords[i].position.x += (target.x - swords[i].position.x) * 12.0f * dt;
      swords[i].position.y += (target.y - swords[i].position.y) * 12.0f * dt;
      break;
    }

    case SwordState::FIRING: {
      // Delay escalonado antes de moverse (0, 0.1, 0.2s)
      if (swords[i].fireDelayTimer > 0) {
        swords[i].fireDelayTimer -= dt;
        // Mientras espera, sigue detras
        Vector2 behind = Vector2Add(
            position, Vector2Scale(Vector2Negate(facing), swordBehindDist));
        swords[i].position.x += (behind.x - swords[i].position.x) * 12.0f * dt;
        swords[i].position.y += (behind.y - swords[i].position.y) * 12.0f * dt;
        break;
      }

      // Mover hacia target
      Vector2 toTarget =
          Vector2Subtract(swords[i].targetPos, swords[i].position);
      float dist = Vector2Length(toTarget);

      if (dist < 20.0f) {
        // Llegó al objetivo
        if (!swords[i].hasDealt && !boss.isDead && !boss.IsInvulnerable()) {
          lastDamageType = DamageType::MAGICAL;
          boss.hp -= (swordHitDamage * rpg.DamageMultiplierMagical());
          swords[i].hasDealt = true;
          swords[i].flashTimer = 0.20f;
          // Particulas de impacto
          for (int k = 0; k < 5; k++) {
            Graphics::VFXSystem::GetInstance().SpawnParticle(
                swords[i].position,
                {(float)GetRandomValue(-200, 200),
                 (float)GetRandomValue(-200, 200)},
                0.3f, {255, 160, 30, 255});
          }
        }
        swords[i].swordState = SwordState::RETURNING;
      } else {
        Vector2 dir = Vector2Scale(toTarget, 1.0f / dist);
        swords[i].position = Vector2Add(swords[i].position,
                                        Vector2Scale(dir, swordFireSpeed * dt));
      }
      break;
    }

    case SwordState::RETURNING: {
      // Volver detras del jugador
      Vector2 behind = Vector2Add(
          position, Vector2Scale(Vector2Negate(facing), swordBehindDist));
      Vector2 toHome = Vector2Subtract(behind, swords[i].position);
      float dist = Vector2Length(toHome);
      if (dist < 8.0f) {
        swords[i].swordState = SwordState::BEHIND;
        swords[i].hasDealt = false;
      } else {
        Vector2 dir = Vector2Scale(toHome, 1.0f / dist);
        swords[i].position = Vector2Add(
            swords[i].position, Vector2Scale(dir, swordReturnSpeed * dt));
      }
      break;
    }
    }
  }
}

// =====================================================
// UPDATE
// =====================================================
void Ropera::Update() {
  float dt = GetFrameTime() * g_timeScale;

  // ── Cooldowns globales ────────────────────────────
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

  // ── Buffs ─────────────────────────────────────────
  if (moveSpeedBuffTimer > 0)
    moveSpeedBuffTimer -= dt;
  if (eBuffTimer > 0) {
    eBuffTimer -= dt;
    eBuffActive = true;
  } else {
    eBuffActive = false;
  }

  // ── Ultimate timer ────────────────────────────────
  if (ultActive) {
    ultTimer -= dt;
    if (ultTimer <= 0) {
      ultActive = false;
      ultTimer = 0;
    }
  }

  // ── CC ────────────────────────────────────────────
  if (stunTimer > 0)
    stunTimer -= dt;
  if (slowTimer > 0)
    slowTimer -= dt;

  // Multiplicador velocidad de ataque (E buff lo acelera, Ult también)
  // 0.95f base = 5% de velocidad de ataque base extra
  float aMult = 0.95f;
  if (eBuffActive)
    aMult *= 0.58f;
  if (ultActive)
    aMult *= 0.50f;

  // ── Estado DASHING: i-frames breves y dash fisico ──────────────
  if (state == RoperaState::DASHING) {
    dashGraceTimer -= dt;
    if (dashGraceTimer <= 0) {
      state = RoperaState::NORMAL;
      dashGraceTimer = 0;
      velocity = Vector2Scale(velocity, 0.45f); // perder impulso al terminar (un poco mas suave)
    }
    // Movimiento fisico del dash (mas friccion para un efecto de rodar/peso)
    velocity = Vector2Scale(velocity, 0.94f); 
    Vector2 np = Vector2Add(position, Vector2Scale(velocity, dt));
    position = Arena::GetClampedPos(np, radius);

    // Estela de particulas durante el dash
    static float ghostTimer = 0;
    ghostTimer += dt;
    if (ghostTimer >= 0.06f) { // frecuencia equilibrada para suavidad
      ghostTimer = 0;
      Graphics::VFXSystem::GetInstance().SpawnGhost(position, {0, 0, (float)radius * 2, (float)radius * 2}, 0.20f, Fade(GetHUDColor(), 0.15f), (facing.x > 0), 1.0f, {radius, radius}, ResourceManager::roperaDash); // menos alpha
    }

    if (GetRandomValue(0, 100) < 60) {
      Graphics::SpawnDashTrail(position);
    }
    return;
  }

  // =====================================================
  // MAQUINA DE ESTADOS PRINCIPAL
  // =====================================================
  switch (state) {

  // ─── NORMAL ──────────────────────────────────────────
  case RoperaState::NORMAL: {
    if (stunTimer > 0)
      break;

    // Velocidad de movimiento
    float spd = 460.0f; // Aumentado (era 395)
    if (moveSpeedBuffTimer > 0)
      spd = 640.0f; // Aumentado (era 590)
    if (slowTimer > 0)
      spd *= 0.5f;
    if (isCharging)
      spd *= 0.55f;

    // Movimiento WASD
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
      Vector2 np = Vector2Add(position, Vector2Scale(move, spd * dt));
      position = Arena::GetClampedPos(np, radius);
    }

    // Facing al mouse
    Vector2 aim = Vector2Subtract(targetAim, position);
    if (Vector2Length(aim) > 0)
      facing = Vector2Normalize(aim);

    // ── Dash (SPACE) – independiente, i-frames reales ──
    if (IsKeyPressed(controls.dash) && CanDash()) {
      // Direccion: WASD si se mueve, si no facing
      Vector2 bdir =
          (Vector2Length(move) > 0) ? Vector2Normalize(move) : facing;
      velocity =
          Vector2Scale(bdir, 975.0f); // Reducido un 15% (era 1150)
      state = RoperaState::DASHING;
      dashGraceTimer = 0.40f; // Mas i-frames (era 0.28)
      UseDash();
      // Spawn initial ghost
      Graphics::VFXSystem::GetInstance().SpawnGhost(position, {0, 0, (float)radius * 2, (float)radius * 2}, 0.35f, Fade(GetHUDColor(), 0.5f), (facing.x > 0), 1.0f, {radius, radius}, ResourceManager::roperaDash);
      // Explosion de particulas al iniciar
      for (int k = 0; k < 8; k++)
        Graphics::VFXSystem::GetInstance().SpawnParticle(
            position,
            {(float)GetRandomValue(-200, 200),
             (float)GetRandomValue(-200, 200)},
            0.25f, {0, 220, 180, 255});
    }

    // ── Hold click: carga heavy ──
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
      holdTimer += dt;
      isCharging = true;
    }

    // ── Release click ──
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && isCharging) {
      if (holdTimer >= 0.35f) {
        // Ataque cargado – consume cooldown de dash
        if (CanDash())
          UseDash();
        velocity = Vector2Scale(facing, 1500.0f);
        state = RoperaState::HEAVY_ATTACK;
        attackPhase = AttackPhase::STARTUP;
        attackPhaseTimer = 0.10f * aMult;
        heavyHasHit = false;
      } else {
        // Click rapido: combo
        state = RoperaState::ATTACKING;
        hasHit = false;
        step3BackDone = false;
        burstHitCount = 0;
        burstSubActive = false;
        burstSubTimer = 0;
        attackPhase = AttackPhase::STARTUP;
        attackPhaseTimer = combo[comboStep].startup * aMult;
        comboTimer = 1.4f;
      }
      holdTimer = 0;
      isCharging = false;
    }

    // ── Habilidad Q (bloqueada en Ult) ──
    if (IsKeyPressed(controls.boomerang) && qCooldown <= 0 && energy >= 20.0f &&
        !ultActive) {
      energy -= 20.0f;
      qCooldown = qMaxCooldown;
      qActive = true;
      qSlashIndex = 0;
      qSlashActiveTimer = 0.18f * aMult; // primera ventana activa
      qSlashGapTimer = 0.0f;
      qHasHit = false;
      state = RoperaState::CASTING_Q;
    }

    // ── Habilidad E ──
    if (IsKeyPressed(controls.berserker) && eCooldown <= 0 && energy >= 30.0f) {
      energy -= 30.0f;
      eCooldown = eMaxCooldown;
      eBuffTimer = 6.0f;
      eBuffActive = true;
    }

    // ── Ultimate R: solo si hp < 60% ──
    if (IsKeyPressed(controls.ultimate) && ultCooldown <= 0 && !ultActive &&
        hp / maxHp < 0.60f && energy >= 50.0f) {
      energy -= 50.0f;
      ultActive = true;
      ultTimer = 8.0f;
      ultCooldown = ultMaxCooldown;
      InitSwords();
    }
    break;
  }

  // ─── ATTACKING (Combo 3 hits) ─────────────────────
  case RoperaState::ATTACKING: {
    // Dirección bloqueada durante la ejecución del combo (User request)

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      attackBufferTimer = BUFFER_WINDOW; // Guardar pulsacion en el buffer
    }

    // ── Hit 3: paso atrás al entrar en STARTUP ──
    if (comboStep == 2 && !step3BackDone &&
        attackPhase == AttackPhase::STARTUP) {
      velocity =
          Vector2Add(velocity, Vector2Scale(Vector2Negate(facing), 217.0f));
      step3BackDone = true;
    }

    attackPhaseTimer -= dt;
    if (hitCooldownTimer > 0)
      hitCooldownTimer -= dt;

    // ── Hit 3 burst sub-timer ──
    if (comboStep == 2 && attackPhase == AttackPhase::ATTACK_ACTIVE &&
        burstSubActive) {
      burstSubTimer -= dt;
    }

    if (attackPhaseTimer <= 0) {
      switch (attackPhase) {
      case AttackPhase::STARTUP:
        attackPhase = AttackPhase::ATTACK_ACTIVE;
        attackPhaseTimer = combo[comboStep].active * aMult;
        hitCooldownTimer = 0;
        attackId++;
        // Impulso tactico (Auto-Lunge) para cerrar distancia en los dos primeros hits
        if (comboStep < 2) {
          velocity = Vector2Add(velocity, Vector2Scale(facing, 180.0f));
        }
        if (comboStep == 2) {
          burstSubActive = true;
          burstSubTimer = 0;
        }
        break;
      case AttackPhase::ATTACK_ACTIVE:
        attackPhase = AttackPhase::RECOVERY;
        attackPhaseTimer = combo[comboStep].recovery * aMult;
        burstSubActive = false;
        break;
      case AttackPhase::RECOVERY:
        comboStep = (comboStep + 1) % 3;
        if (attackBufferTimer > 0.0f) {
          attackBufferTimer = 0.0f;
          attackPhase = AttackPhase::STARTUP;
          attackPhaseTimer = combo[comboStep].startup * aMult;
          comboTimer = 1.4f;
          hasHit = false;
          burstHitCount = 0;
          burstSubActive = false;
          step3BackDone = false;
        } else {
          state = RoperaState::NORMAL;
          attackPhase = AttackPhase::NONE;
          hasHit = false;
          burstHitCount = 0;
          burstSubActive = false;
          step3BackDone = false;
        }
        break;
      default:
        break;
      }
    }

    // ── Dash cancela el combo ──
    if (IsKeyPressed(controls.dash) && CanDash()) {
      Vector2 aim2 = Vector2Subtract(targetAim, position);
      Vector2 bdir =
          (Vector2Length(aim2) > 0) ? Vector2Normalize(aim2) : facing;
      velocity = Vector2Scale(bdir, 975.0f); // Reducido un 15% (era 1100)
      state = RoperaState::DASHING;
      dashGraceTimer = 0.25f; // Mas i-frames (era 0.15)
      UseDash();
      attackPhase = AttackPhase::NONE;
      // Spawn ghost for dash cancel
      Graphics::VFXSystem::GetInstance().SpawnGhost(position, {0, 0, (float)radius * 2, (float)radius * 2}, 0.3f, Fade(GetHUDColor(), 0.4f), (facing.x > 0), 1.0f, {radius, radius}, ResourceManager::roperaDash);
      for (int k = 0; k < 8; k++)
        Graphics::VFXSystem::GetInstance().SpawnParticle(
            position,
            {(float)GetRandomValue(-200, 200),
             (float)GetRandomValue(-200, 200)},
            0.25f, {0, 220, 180, 255});
    }
    break;
  }

  // ─── HEAVY_ATTACK (Super Estocada) ────────────────
  case RoperaState::HEAVY_ATTACK: {
    attackPhaseTimer -= dt;

    // Propulsion frontal durante startup
    if (attackPhase == AttackPhase::STARTUP) {
      Vector2 np = Vector2Add(position, Vector2Scale(velocity, dt));
      position = Arena::GetClampedPos(np, radius);
      velocity = Vector2Scale(velocity, 0.78f);
    }

    if (attackPhaseTimer <= 0) {
      switch (attackPhase) {
      case AttackPhase::STARTUP:
        attackPhase = AttackPhase::ATTACK_ACTIVE;
        attackPhaseTimer = 0.18f;
        velocity = {0, 0};
        attackId++;
        break;
      case AttackPhase::ATTACK_ACTIVE:
        attackPhase = AttackPhase::RECOVERY;
        attackPhaseTimer = 0.40f;
        break;
      case AttackPhase::RECOVERY:
        state = RoperaState::NORMAL;
        attackPhase = AttackPhase::NONE;
        heavyHasHit = false;
        break;
      default:
        break;
      }
    }
    break;
  }

  // ─── CASTING_Q (Dos tajos en angulo cerrado) ──────
  case RoperaState::CASTING_Q: {
    // Tajo actual tiene ventana activa
    if (qSlashActiveTimer > 0) {
      qSlashActiveTimer -= dt;
    } else if (qSlashIndex == 0) {
      // Pausa entre tajos 1 y 2
      qSlashGapTimer -= dt;
      if (qSlashGapTimer <= 0) {
        qSlashIndex = 1;
        qSlashActiveTimer = 0.18f * aMult;
        qHasHit = false;
        attackId++;
      }
    } else {
      // Fin de la habilidad Q
      qActive = false;
      state = RoperaState::NORMAL;
    }

    // Iniciar gap si el primer tajo acaba de terminar
    if (qSlashIndex == 0 && qSlashActiveTimer <= 0 && qSlashGapTimer <= 0) {
      qSlashGapTimer = 0.10f * aMult;
    }

    // Dirección bloqueada durante Q (User request)
    break;
  }

  case RoperaState::DASHING:
    break; // ya manejado arriba
  case RoperaState::CHARGING_HEAVY:
    break;
  case RoperaState::ULT_ACTIVE:
    state = RoperaState::NORMAL;
    break;
  } // end switch

  // ── Friccion ─────────────────────────────────────
  velocity = Vector2Scale(velocity, 0.87f);

  // ── Fisica (excepto HEAVY startup) ───────────────
  if (state != RoperaState::HEAVY_ATTACK ||
      attackPhase != AttackPhase::STARTUP) {
    Vector2 np = Vector2Add(position, Vector2Scale(velocity, dt));
    position = Arena::GetClampedPos(np, radius);
  }

  // ── Animación del Sprite ─────────────────────────────────────────
  frameTimer += dt;

  // ── Combo timeout ─────────────────────────────────
  if (state != RoperaState::ATTACKING && comboTimer > 0) {
    comboTimer -= dt;
    if (comboTimer <= 0)
      comboStep = 0;
  }
}

// =====================================================
// RESET
// =====================================================
void Ropera::Reset(Vector2 pos) {
  position = pos;
  hp = maxHp;
  energy = 100.0f;
  velocity = {0, 0};
  state = RoperaState::NORMAL;
  attackPhase = AttackPhase::NONE;
  comboStep = 0;
  comboTimer = 0;
  hasHit = false;
  heavyHasHit = false;
  dashCharges = maxDashCharges;
  dashCooldown1 = 0.0f;
  dashCooldown2 = 0.0f;
  dashGraceTimer = 0;
  qCooldown = 0;
  eCooldown = 0;
  ultCooldown = 0;
  eBuffTimer = 0;
  eBuffActive = false;
  moveSpeedBuffTimer = 0;
  ultActive = false;
  ultTimer = 0;
  qActive = false;
  stunTimer = 0;
  slowTimer = 0;
  holdTimer = 0;
  isCharging = false;
  step3BackDone = false;
  burstHitCount = 0;
  burstSubActive = false;
  for (int i = 0; i < 3; i++) {
    swords[i].active = false;
    swords[i].swordState = SwordState::BEHIND;
    swords[i].hasDealt = false;
  }
}

// =====================================================
// HANDLE SKILLS (update de espadas, llamado cada frame)
// =====================================================
void Ropera::HandleSkills(Boss &boss) {
  float dt = GetFrameTime() * g_timeScale;
  UpdateSwords(dt, boss);
}

// =====================================================
// CHECK COLLISIONS
// =====================================================
void Ropera::CheckCollisions(Boss &boss) {
  if (boss.isDead || boss.isDying || boss.IsInvulnerable())
    return;

  float aMult = 1.0f;
  if (eBuffActive)
    aMult *= 0.58f;
  if (ultActive)
    aMult *= 0.50f;

  // ─── Combo ────────────────────────────────────────
  if (CheckComboCollision(boss)) {
    int step = comboStep;
    float dmg = combo[step].damage;
    if (eBuffActive)
      dmg += boss.maxHp * eMaxHpBonusFrac;
    if (ultActive)
      dmg *= 0.85f; // ult tradeoff

    dmg *= rpg.DamageMultiplierPhysical();
    lastDamageType = DamageType::PHYSICAL;
    boss.hp -= dmg;
    energy = fminf(maxEnergy, energy + 5.0f);

    if (eBuffActive)
      hp = fminf(maxHp, hp + dmg * eLifestealFrac);

    // Hitstop y empuje (reducido 50%)
    hitstopTimer = 0.08f;
    velocity = Vector2Add(velocity, Vector2Scale(facing, 280.0f));

    // Solo el ultimo ataque de la secuencia empujaba al boss (desactivado)
    if (comboStep == 2) {
      screenShake = fmaxf(screenShake, 1.8f); // Reducido (era 2.5) y usando fmaxf
    }

    // Disparar espadas escalonadas
    if (ultActive)
      TriggerSwords(boss.position);
  }

  // ─── Heavy ────────────────────────────────────────
  if (CheckHeavyCollision(boss)) {
    float dmg = 64.8f; // Daño original revertido
    if (eBuffActive)
      dmg += boss.maxHp * eMaxHpBonusFrac;
      
    dmg *= rpg.DamageMultiplierPhysical();
    lastDamageType = DamageType::PHYSICAL;
    boss.hp -= dmg;
    energy = fminf(maxEnergy, energy + 10.0f);
    if (eBuffActive)
      hp = fminf(maxHp, hp + dmg * eLifestealFrac);

    // Hitstop y empuje
    hitstopTimer = 0.12f;
    velocity =
        Vector2Add(velocity, Vector2Scale(facing, 385.0f)); // reducido 50%

    if (ultActive)
      TriggerSwords(boss.position);
  }

  // ─── Q (dos tajos) ────────────────────────────────
  if (qActive && state == RoperaState::CASTING_Q && qSlashActiveTimer > 0) {
    if (CheckQCollision(boss, qSlashIndex)) {
      float dmg = 24.0f * rpg.DamageMultiplierPhysical(); // Daño original revertido
      lastDamageType = DamageType::PHYSICAL;
      boss.hp -= dmg;
      energy = fminf(maxEnergy, energy + 8.0f);
      qHasHit = true;
      if (eBuffActive)
        hp = fminf(maxHp, hp + dmg * eLifestealFrac);

      // Hitstop más ligero para los tajos rápidos
      hitstopTimer = 0.05f;
      velocity = Vector2Add(velocity, Vector2Scale(facing, 350.0f));

      // Buff de velocidad al acertar
      if (!ultActive)
        moveSpeedBuffTimer = 3.5f;
    }
  }
}



// =====================================================
// CHECK COMBO COLLISION
// =====================================================
bool Ropera::CheckComboCollision(Boss &boss) {
  if (state != RoperaState::ATTACKING ||
      attackPhase != AttackPhase::ATTACK_ACTIVE)
    return false;

  float attackMult = (eBuffActive ? 0.58f : (ultActive ? 0.50f : 1.0f));
  float activeTotal = combo[comboStep].active * attackMult;
  float progMult = ultActive ? 1.4f : 2.8f; // Mas snappier en normal (era 2.2)
  // Empezar con un 20% de progreso para cubrir el area inmediata al personaje
  float progress = 0.20f + CombatUtils::GetProgress(attackPhaseTimer, activeTotal) * progMult;
  if (progress > 1.0f) progress = 1.0f;

  float maxRange = combo[comboStep].range * (ultActive ? 0.92f : 1.0f);
  float currentRange = maxRange * progress; // Crece hacia adelante
  
  // Usar los angulos definidos en el combo siempre, 16.0 era demasiado estrecho
  float angleWidth = combo[comboStep].angleWidth;
  float halfAngle  = angleWidth / 2.0f;

  // Hit 3: hasta 3 sub-hits con su propio timer (0.12s entre c/u)
  if (comboStep == 2) {
    if (burstHitCount >= 3)
      return false;
    if (!burstSubActive)
      return false;
    if (burstSubTimer > 0)
      return false;

    if (CombatUtils::IsoArc(position, facing, boss.position, boss.radius, currentRange, halfAngle)) {
      burstHitCount++;
      burstSubTimer = 0.12f; // proximo sub-hit en 0.12s
      return true;
    }
    return false;
  }

  // Hits 1 y 2
  if (hitCooldownTimer > 0)
    return false;

  bool hitDetected = false;
  if (comboStep == 0) {
      // Estocada: Progresión radial/frontal
      hitDetected = CombatUtils::CheckProgressiveThrust(position, facing, boss.position, boss.radius, maxRange, angleWidth / 2.0f, progress);
  } else if (comboStep == 1) {
      // Tajo 90: Progresión angular (Reloj)
      hitDetected = CombatUtils::CheckProgressiveSweep(position, facing, boss.position, boss.radius, maxRange, -angleWidth/2.0f, angleWidth, 1.0f, progress);
  }

  if (hitDetected) {
    hitCooldownTimer = combo[comboStep].hitCooldown;
    hasHit = true;
    return true;
  }
  return false;
}

// =====================================================
// CHECK HEAVY COLLISION
// =====================================================
bool Ropera::CheckHeavyCollision(Boss &boss) {
  if (state != RoperaState::HEAVY_ATTACK ||
      attackPhase != AttackPhase::ATTACK_ACTIVE)
    return false;
  if (heavyHasHit)
    return false;

  float activeTotal = 0.18f;
  float progMult = ultActive ? 1.4f : 2.2f; // Ropera base hitboxes faster, ult same (was 1.4f)
  float progress = CombatUtils::GetProgress(attackPhaseTimer, activeTotal) * progMult;
  if (progress > 1.0f) progress = 1.0f;
  float maxRange = 265.0f; // Adjusted reach (+12% from 236)
  
  // Usar angulo de combo 0 para modo normal o el de garras en ult
  float angleWidth = ultActive ? 72.0f : combo[0].angleWidth;

  if (CombatUtils::CheckProgressiveThrust(position, facing, boss.position, boss.radius, maxRange, angleWidth/2.0f, progress)) {
    heavyHasHit = true;
    return true;
  }
  return false;
}

// =====================================================
// CHECK Q COLLISION
// =====================================================
bool Ropera::CheckQCollision(Boss &boss, int slashIdx) {
  if (qHasHit) return false;
  
  float offsetDeg = (slashIdx == 0) ? -45.0f : 45.0f;
  float sweepDir = (slashIdx == 0) ? 1.0f : -1.0f; // Primer tajo CCW, segundo CW
  float progress = CombatUtils::GetProgress(qSlashActiveTimer, 0.18f * (eBuffActive ? 0.58f : 1.0f)) * 2.2f; // Snappier Q (was 1.4f)
  if (progress > 1.0f) progress = 1.0f;

  // Rango Q: 155px (Adjusted from 139), Ancho: 90 grados para que el tajo se sienta importante
  return CombatUtils::CheckProgressiveSweep(position, facing, boss.position, boss.radius, 155.0f, offsetDeg, 90.0f, sweepDir, progress);
}

// =====================================================
// GET ABILITIES
// =====================================================
std::vector<AbilityInfo> Ropera::GetAbilities() const {
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
                 {80, 200, 220, 255}});
  abs.push_back({"Q Tajos",
                 qCooldown,
                 qMaxCooldown,
                 20.0f,
                 qCooldown <= 0 && energy >= 20.0f && !ultActive,
                 {0, 220, 180, 255}});
  abs.push_back({"E Furia",
                 eCooldown,
                 eMaxCooldown,
                 30.0f,
                 eCooldown <= 0 && energy >= 30.0f,
                 {60, 255, 180, 255}});
  abs.push_back(
      {"R Garras",
       ultCooldown,
       ultMaxCooldown,
       50.0f,
       ultCooldown <= 0 && !ultActive && hp / maxHp < 0.60f && energy >= 50.0f,
       {255, 120, 0, 255}});
  return abs;
}

// =====================================================
// GET SPECIAL STATUS
// =====================================================
std::string Ropera::GetSpecialStatus() const {
  if (ultActive)
    return TextFormat("[ GARRAS %.1fs ]", ultTimer);
  if (eBuffActive)
    return TextFormat("[ FURIA %.1fs ]", eBuffTimer);
  if (moveSpeedBuffTimer > 0)
    return "[ VEL+ ]";
  return "";
}

// =====================================================
// DRAW
// =====================================================
void Ropera::Draw() {
  float t = (float)g_gameTime;
  // --- Sombra proyectada se maneja en GameplayScene ---
  // DrawEllipse((int)position.x, (int)position.y, radius, radius*0.5f,
  // Fade(BLACK, 0.4f));

  // ── Aura ─────────────────────────────────────────
  float ap = 0.3f + 0.15f * sinf(t * 3.2f);
  Color ac = ultActive                ? Color{255, 120, 0, 255}
             : eBuffActive            ? Color{0, 255, 150, 255}
             : moveSpeedBuffTimer > 0 ? Color{80, 255, 220, 255}
                                      : Color{0, 200, 160, 255};
  DrawCircleGradient((int)position.x, (int)position.y - 20,
                     radius * (ultActive ? 3.8f : 1.9f),
                     Fade(ac, ap * (ultActive ? 0.9f : 0.5f)), Fade(ac, 0));

  // ── Cuerpo (Sprite) ───────────────────────────────
  {
    Texture2D activeTex = ResourceManager::roperaIdle;

    // 1. Determinar Textura Activa
    if (hp <= 0) {
      activeTex = ResourceManager::roperaDeath;
    } else if (hitFlashTimer > 0) {
      activeTex = ResourceManager::roperaHit;
    } else if (state == RoperaState::DASHING) {
      activeTex = ResourceManager::roperaDash;
    } else if (state == RoperaState::CASTING_Q) {
      activeTex =
          ResourceManager::roperaAttack1; // El cuerpo usa pose de ataque1
    } else if (state == RoperaState::HEAVY_ATTACK) {
      activeTex = ResourceManager::roperaHeavy;
    } else if (state == RoperaState::ATTACKING) {
      if (comboStep == 0 || comboStep == 1)
        activeTex = ResourceManager::roperaAttack1;
      else
        activeTex = ResourceManager::roperaAttack3;
    } else {
      // NORMAL state
      if (Vector2Length(velocity) > 20.0f) {
        activeTex = ResourceManager::roperaRun;
      } else {
        activeTex = ResourceManager::roperaIdle;
      }
    }

    // 2. Determinar numero de frames de forma dinámica
    int numFrames = 1;
    Image *animSource = nullptr;

    if (state == RoperaState::CASTING_Q) {
      numFrames = ResourceManager::roperaAttack1Frames;
      animSource = &ResourceManager::roperaAttack1Im;
    } else if (hp <= 0) {
      // Death is still PNG
      if (activeTex.height > 0)
        numFrames = activeTex.width / activeTex.height;
    } else if (hitFlashTimer > 0) {
      // Hit is still PNG
      if (activeTex.height > 0)
        numFrames = activeTex.width / activeTex.height;
    } else if (state == RoperaState::DASHING) {
      numFrames = ResourceManager::roperaDashFrames;
      animSource = &ResourceManager::roperaDashIm;
    } else if (state == RoperaState::HEAVY_ATTACK) {
      numFrames = ResourceManager::roperaHeavyFrames;
      animSource = &ResourceManager::roperaHeavyIm;
    } else if (state == RoperaState::ATTACKING) {
      if (comboStep == 0 || comboStep == 1) {
        numFrames = ResourceManager::roperaAttack1Frames;
        animSource = &ResourceManager::roperaAttack1Im;
      } else {
        numFrames = ResourceManager::roperaAttack3Frames;
        animSource = &ResourceManager::roperaAttack3Im;
      }
    } else {
      // NORMAL
      if (Vector2Length(velocity) > 20.0f) {
        numFrames = ResourceManager::roperaRunFrames;
        animSource = &ResourceManager::roperaRunIm;
      } else {
        numFrames = ResourceManager::roperaIdleFrames;
        animSource = &ResourceManager::roperaIdleIm;
      }
    }

    if (numFrames <= 0)
      numFrames = 1;

    // 3. Calcular qué frame mostrar
    int currentFrame = 0;

    if (state == RoperaState::NORMAL && animSource != nullptr) {
      // Loop de animacion por tiempo para Idle/Run
      float animSpeed =
          (activeTex.id == ResourceManager::roperaRun.id) ? 0.08f : 0.12f;
      currentFrame = (int)(t / animSpeed) % numFrames;
    } else {
      // Animacion basada en progreso del ataque/estado
      float progress = 0.0f;

      if (state == RoperaState::ATTACKING) {
        float aMult = (eBuffActive ? 0.58f : (ultActive ? 0.50f : 1.0f));
        float st = combo[comboStep].startup * aMult;
        float ac = combo[comboStep].active * aMult;
        float rc = combo[comboStep].recovery * aMult;
        float tot = st + ac + rc;

        float elapsed = 0.0f;
        if (attackPhase == AttackPhase::STARTUP)
          elapsed = st - attackPhaseTimer;
        else if (attackPhase == AttackPhase::ATTACK_ACTIVE)
          elapsed = st + (ac - attackPhaseTimer);
        else if (attackPhase == AttackPhase::RECOVERY)
          elapsed = st + ac + (rc - attackPhaseTimer);

        progress = elapsed / tot;
      } else if (state == RoperaState::HEAVY_ATTACK) {
        float tot = 0.10f + 0.18f + 0.40f;
        float elapsed = 0.0f;
        if (attackPhase == AttackPhase::STARTUP)
          elapsed = 0.10f - attackPhaseTimer;
        else if (attackPhase == AttackPhase::ATTACK_ACTIVE)
          elapsed = 0.10f + (0.18f - attackPhaseTimer);
        else if (attackPhase == AttackPhase::RECOVERY)
          elapsed = 0.10f + 0.18f + (0.40f - attackPhaseTimer);
        progress = elapsed / tot;
      } else if (state == RoperaState::DASHING) {
        progress = 1.0f - (dashGraceTimer / 0.15f);
      }
      // Eliminamos la lógica de avance del GIF de tajo del cuerpo
      // ya que ahora se maneja como un efecto overlay independiente en la
      // sección Visual Q.

      progress = std::clamp(progress, 0.0f, 0.99f);
      currentFrame = (int)(progress * numFrames);
    }

    // --- ACTUALIZACION TEXTURA DESDE GIF ---
    if (animSource != nullptr && numFrames > 0) {
      unsigned int nextFrameDataOffset =
          animSource->width * animSource->height * 4 * currentFrame;
      UpdateTexture(activeTex,
                    ((unsigned char *)animSource->data) + nextFrameDataOffset);
      currentFrame = 0;
      numFrames = 1;
    }

    // 4. Dibujar textura
    if (activeTex.id != 0) {
      float frameW = (float)activeTex.width / numFrames;
      float frameH = (float)activeTex.height;

      // Voltear horizontalmente segun el facing (asumiendo que miran a la
      // derecha por defecto)
      float flipMult = (facing.x < 0) ? -1.0f : 1.0f;

      Rectangle src = {(float)currentFrame * frameW, 0.0f, frameW * flipMult,
                       frameH};

      // Origen centrado en la base
      float scale = 1.5f; // Ajusta esto segun el tamano deseado de tu personaje
      Rectangle dest = {position.x, position.y, frameW * scale, frameH * scale};

      // Sombra en los pies (para referencia de suelo)
      DrawEllipse((int)position.x, (int)position.y, 35, 12, Fade(BLACK, 0.4f));

      // Ajuste agresivo del pivot: bajamos el origen mas al centro de la imagen
      // para que los pies (que suelen estar mas arriba en frames de Ropera)
      // toquen el suelo.
      Vector2 orig = {(frameW * scale) / 2.0f, (frameH * scale) * 0.55f};

      Color tint = WHITE;
      if (hitFlashTimer > 0)
        tint = {255, 100, 100, 255};
      else if (ultActive)
        tint = {255, 220, 100, 255};
      else if (eBuffActive)
        tint = {180, 255, 200, 255};

      DrawTexturePro(activeTex, src, dest, orig, 0.0f, tint);

    } else {
      // Fallback primitivo si falla carga
      Color tint = WHITE;
      if (hitFlashTimer > 0)
        tint = {255, 100, 100, 255};
      DrawCircleV({position.x, position.y - 20}, radius, tint);
    }
  }

  // ── Visual Combo Progresivo ───────────────────────
  if (state == RoperaState::ATTACKING) {
    int step = comboStep;
    float aMult = eBuffActive ? 0.58f : (ultActive ? 0.50f : 1.0f);
    float activeTotal = combo[step].active * aMult;
    float progMult = ultActive ? 1.4f : 2.2f; // Visual match for faster base hitboxes
    float progress = CombatUtils::GetProgress(attackPhaseTimer, activeTotal) * progMult;
    if (progress > 1.0f) progress = 1.0f;

    float maxRange = combo[step].range * (ultActive ? 0.92f : 1.0f); // Fixed reach to match logic (was 0.68)
    float drawRange = maxRange;
    if (attackPhase == AttackPhase::ATTACK_ACTIVE) {
      drawRange = maxRange * progress;
    } else if (attackPhase == AttackPhase::STARTUP) {
      drawRange = maxRange * 0.3f; // preview corto
    }

    float angW = ultActive ? combo[step].angleWidth : 16.0f; // Fina como estocada en normal
    float ha = angW / 2.0f;
    float sa = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG - ha;
    
    float alpha = (attackPhase == AttackPhase::STARTUP)    ? 0.25f
                  : (attackPhase == AttackPhase::RECOVERY) ? 0.15f
                                                           : 0.85f;

    Color sc = ultActive ? Color{255, 160, 0, 255} : Color{0, 255, 200, 255};
    rlPushMatrix();
    rlTranslatef(position.x, position.y, 0);
    rlScalef(1.0f, 0.5f, 1.0f);
    
    DrawCircleSector({0, 0}, drawRange, sa, sa + angW, 24, Fade(sc, alpha));
    
    if (attackPhase == AttackPhase::ATTACK_ACTIVE) {
      DrawCircleSectorLines({0, 0}, drawRange, sa, sa + angW, 24, WHITE);
      // Punta del estoque brillante
      float midAng = (sa + ha) * DEG2RAD;
      DrawCircleV({cosf(midAng) * drawRange, sinf(midAng) * drawRange}, 5.0f, WHITE);
    }
    rlPopMatrix();
  }

  // ── Visual Heavy Progresivo ───────────────────────
  if (state == RoperaState::HEAVY_ATTACK) {
    float activeTotal = 0.18f;
    float progMult = ultActive ? 1.4f : 2.2f; // Visual match for faster base hitboxes
    float progress = CombatUtils::GetProgress(attackPhaseTimer, activeTotal) * progMult;
    if (progress > 1.0f) progress = 1.0f;
    
    float maxRange = 265.0f; // Fixed reach to match logic (was 236)
    float drawRange = maxRange;
    if (attackPhase == AttackPhase::ATTACK_ACTIVE) {
      drawRange = maxRange * progress;
    } else if (attackPhase == AttackPhase::STARTUP) {
      drawRange = maxRange * 0.3f;
    }

    float angW = ultActive ? 72.0f : 16.0f;
    float ha = angW / 2.0f;
    float sa = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG - ha;
    
    float alp = (attackPhase == AttackPhase::ATTACK_ACTIVE) ? 0.90f : 0.25f;
    Color sc = ultActive ? Color{255, 160, 0, 255} : Color{0, 255, 200, 255};
    
    rlPushMatrix();
    rlTranslatef(position.x, position.y, 0);
    rlScalef(1.0f, 0.5f, 1.0f);
    
    DrawCircleSector({0, 0}, drawRange, sa, sa + angW, 24, Fade(sc, alp));
    
    if (attackPhase == AttackPhase::ATTACK_ACTIVE) {
      DrawCircleSectorLines({0, 0}, drawRange, sa, sa + angW, 24, WHITE);
      DrawCircleSectorLines({0, 0}, drawRange + 8.0f, sa - 2.0f, sa + angW + 2.0f, 24, Fade(WHITE, 0.3f));
      float midAng = (sa + ha) * DEG2RAD;
      DrawCircleV({cosf(midAng) * drawRange, sinf(midAng) * drawRange}, 8.0f, WHITE);
    }
    rlPopMatrix();
  }

  // ── Visual Q (GIF) ────────────────────────────────
  if (qActive && state == RoperaState::CASTING_Q && qSlashActiveTimer > 0) {
    float off = (qSlashIndex == 0) ? -28.0f : 28.0f;
    float ang = atan2f(facing.y, facing.x) * RAD2DEG + off;

    // Calcular progreso para el GIF del tajo
    float progress = 0;
    if (qSlashIndex == 0)
      progress = 1.0f - (qSlashActiveTimer / 0.18f) * 0.5f;
    else
      progress = 0.5f + (1.0f - (qSlashActiveTimer / 0.18f)) * 0.5f;

    // Actualizar textura del GIF
    int totalFrames = ResourceManager::roperaTajoFrames;
    if (totalFrames > 0) {
      int frame = (int)(progress * totalFrames) % totalFrames;
      unsigned int frameSize = ResourceManager::roperaTajoDobleIm.width *
                               ResourceManager::roperaTajoDobleIm.height * 4;
      UpdateTexture(ResourceManager::roperaTajoDoble,
                    ((unsigned char *)ResourceManager::roperaTajoDobleIm.data) +
                        (frameSize * frame));
    }

    float scale = 2.4f; // Ajuste para que se vea imponente
    float w = (float)ResourceManager::roperaTajoDoble.width * scale;
    float h = (float)ResourceManager::roperaTajoDoble.height * scale;

    // Dibujar el tajo rotado
    Rectangle src = {0, 0, (float)ResourceManager::roperaTajoDoble.width,
                     (float)ResourceManager::roperaTajoDoble.height};
    // Si facing es a la izquierda, podemos elegir si flipear el tajo o dejar
    // que la rotación lo maneje
    if (facing.x < 0)
      src.height *= -1; // Invertimos verticalmente si miramos a la izquierda
                        // para que el arco se mantenga natural

    Rectangle dest = {position.x, position.y - 20, w, h};
    Vector2 origin = {w * 0.5f, h * 0.5f};

    DrawTexturePro(ResourceManager::roperaTajoDoble, src, dest, origin, ang,
                   WHITE);
  }

  // ── Visual Carga ──────────────────────────────────
  if (isCharging && holdTimer > 0) {
    float cp = fminf(holdTimer / 0.35f, 1.0f);
    DrawCircleLines((int)position.x, (int)position.y - 20,
                    radius + 8.0f + 20.0f * (1.0f - cp),
                    Fade({0, 220, 180, 255}, cp));
    if (cp >= 1.0f) {
      Color rc = CanDash() ? Color{0, 255, 180, 255} : Color{255, 200, 0, 255};
      if (sinf((float)g_gameTime * 30.0f) > 0)
        DrawCircleLines((int)position.x, (int)position.y - 20, radius + 14, rc);
    }
  }

  // ── Visual Dash ───────────────────────────────────
  if (state == RoperaState::DASHING) {
    DrawCircleGradient((int)position.x, (int)position.y - 20, radius * 2.5f,
                       Fade({0, 220, 180, 255}, 0.4f),
                       Fade({0, 220, 180, 255}, 0));
  }

  // ── Buff velocidad anillo ─────────────────────────
  if (moveSpeedBuffTimer > 0) {
    float pulse = 0.5f + 0.4f * sinf(t * 6.0f);
    rlPushMatrix();
    rlTranslatef(position.x, position.y, 0);
    rlScalef(1.0f, 0.5f, 1.0f);
    DrawCircleLines(0, 0, radius + 15.0f, Fade({0, 255, 200, 255}, pulse));
    rlPopMatrix();
  }

  // ── Espadas Voladoras ─────────────────────────────
  if (ultActive) {
    for (int i = 0; i < 3; i++) {
      if (!swords[i].active)
        continue;
      Vector2 sp = swords[i].position;

      bool firing = (swords[i].swordState == SwordState::FIRING &&
                     swords[i].fireDelayTimer <= 0);
      bool flash = (swords[i].flashTimer > 0);

      Color sc_col = flash    ? Color{255, 80, 0, 255}
                     : firing ? Color{255, 200, 60, 255}
                              : Color{220, 230, 255, 255};

      // Dibuja espada como rectangulo rotado + punta
      float angle = atan2f(sp.y - position.y, sp.x - position.x) * RAD2DEG;
      // Blade
      DrawRectanglePro({sp.x, sp.y, 32.0f, 5.0f}, {16.0f, 2.5f}, angle, sc_col);
      // Guard
      DrawRectanglePro({sp.x, sp.y, 5.0f, 14.0f}, {2.5f, 7.0f}, angle + 90.0f,
                       Fade(LIGHTGRAY, 0.8f));
      // Glow
      if (flash) {
        DrawCircleGradient((int)sp.x, (int)sp.y, 22.0f,
                           Fade({255, 120, 0, 255}, 0.75f),
                           Fade({255, 60, 0, 255}, 0));
      }

      // Estela durante el vuelo
      if (firing) {
        Vector2 tail = Vector2Subtract(
            sp, Vector2Scale(
                    Vector2Normalize(Vector2Subtract(swords[i].targetPos, sp)),
                    20.0f));
        DrawLineEx(sp, tail, 3.0f, Fade({255, 200, 60, 255}, 0.5f));
      }
    }
  }
}
