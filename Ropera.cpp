// =====================================================
// Ropera.cpp  v2.0 - FULL RESTORATION (APRIL 20 22:00 STATE)
// =====================================================
#include "include/graphics/VFXSystem.h"
#include "include/graphics/AnimeVFX.h"
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

// --- Perp 2D ---
static Vector2 Perp2D(Vector2 v) { return {-v.y, v.x}; }

// =====================================================
// InitSwords
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
// TriggerSwords
// =====================================================
void Ropera::TriggerSwords(Vector2 enemyPos) {
  swordTargetSnapshot = enemyPos;
  for (int i = 0; i < 3; i++) {
    if (!swords[i].active)
      continue;
    if (swords[i].swordState != SwordState::BEHIND)
      continue; 
    swords[i].swordState = SwordState::FIRING;
    swords[i].targetPos = enemyPos;
    swords[i].hasDealt = false;
    swords[i].fireDelayTimer = swords[i].fireDelay; 
  }
}

// =====================================================
// UpdateSwords
// =====================================================
void Ropera::UpdateSwords(float dt, Boss &boss) {
  if (!ultActive)
    return;

  Vector2 perpFacing = Perp2D(facing);
  float offsets[3] = {0.0f, -22.0f, 22.0f}; 

  for (int i = 0; i < 3; i++) {
    if (!swords[i].active)
      continue;
    if (swords[i].flashTimer > 0)
      swords[i].flashTimer -= dt;

    switch (swords[i].swordState) {

    case SwordState::BEHIND: {
      Vector2 behind = Vector2Add(position, Vector2Scale(Vector2Negate(facing), swordBehindDist));
      Vector2 target = Vector2Add(behind, Vector2Scale(perpFacing, offsets[i]));
      swords[i].position.x += (target.x - swords[i].position.x) * 12.0f * dt;
      swords[i].position.y += (target.y - swords[i].position.y) * 12.0f * dt;
      break;
    }

    case SwordState::FIRING: {
      if (swords[i].fireDelayTimer > 0) {
        swords[i].fireDelayTimer -= dt;
        Vector2 behind = Vector2Add(position, Vector2Scale(Vector2Negate(facing), swordBehindDist));
        swords[i].position.x += (behind.x - swords[i].position.x) * 12.0f * dt;
        swords[i].position.y += (behind.y - swords[i].position.y) * 12.0f * dt;
        break;
      }

      Vector2 toTarget = Vector2Subtract(swords[i].targetPos, swords[i].position);
      float dist = Vector2Length(toTarget);

      if (dist < 20.0f) {
        if (!swords[i].hasDealt && !boss.isDead && !boss.IsInvulnerable()) {
          lastDamageType = DamageType::MAGICAL;
          boss.TakeDamage(swordHitDamage * rpg.DamageMultiplierMagical(), 2.0f, {0, 0});
          swords[i].hasDealt = true;
          swords[i].flashTimer = 0.20f;
          for (int k = 0; k < 5; k++) {
            Graphics::VFXSystem::GetInstance().SpawnParticle(swords[i].position, {(float)GetRandomValue(-200, 200), (float)GetRandomValue(-200, 200)}, 0.3f, {255, 160, 30, 255});
          }
        }
        swords[i].swordState = SwordState::RETURNING;
      } else {
        Vector2 dir = Vector2Scale(toTarget, 1.0f / dist);
        swords[i].position = Vector2Add(swords[i].position, Vector2Scale(dir, swordFireSpeed * dt));
      }
      break;
    }

    case SwordState::RETURNING: {
      Vector2 behind = Vector2Add(position, Vector2Scale(Vector2Negate(facing), swordBehindDist));
      Vector2 toHome = Vector2Subtract(behind, swords[i].position);
      float dist = Vector2Length(toHome);
      if (dist < 8.0f) {
        swords[i].swordState = SwordState::BEHIND;
        swords[i].hasDealt = false;
      } else {
        Vector2 dir = Vector2Scale(toHome, 1.0f / dist);
        swords[i].position = Vector2Add(swords[i].position, Vector2Scale(dir, swordReturnSpeed * dt));
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
  if (hp <= 0) {
      velocity = {0, 0};
      return;
  }
  float dt = GetFrameTime() * g_timeScale;

  if (energy < maxEnergy)
    energy += 4.0f * dt;

  // SLOW MOTION during Ultimate Heavy sequence - Mas agresivo (0.15x)
  if (ultActive && state == RoperaState::HEAVY_ATTACK) {
      g_timeScale = Lerp(g_timeScale, 0.15f, 15.0f * GetFrameTime());
  } else if (ultActive && state != RoperaState::HEAVY_ATTACK && g_timeScale < 0.9f) {
      g_timeScale = Lerp(g_timeScale, 1.0f, 8.0f * GetFrameTime());
  }

  if (hitFlashTimer > 0)
    hitFlashTimer -= dt;
  UpdateDash(dt);

  if (isStaggered) {
      staggerTimer -= dt;
      if (staggerTimer <= 0.0f) isStaggered = false;
      velocity = Vector2Scale(velocity, 0.85f);
      Vector2 np = Vector2Add(position, Vector2Scale(velocity, dt));
      position = Arena::GetClampedPos(np, radius);
      return;
  }
  if (qCooldown > 0)
    qCooldown -= dt;
  if (eCooldown > 0)
    eCooldown -= dt;
  if (ultCooldown > 0)
    ultCooldown -= dt;
  if (attackBufferTimer > 0)
    attackBufferTimer -= dt;

  if (moveSpeedBuffTimer > 0)
    moveSpeedBuffTimer -= dt;
  if (eBuffTimer > 0) {
    eBuffTimer -= dt;
    eBuffActive = true;
  } else {
    eBuffActive = false;
  }

  if (ultActive) {
    ultTimer -= dt;
    if (ultTimer <= 0) {
      ultActive = false;
      ultTimer = 0;
    }
  }

  if (stunTimer > 0)
    stunTimer -= dt;
  if (slowTimer > 0)
    slowTimer -= dt;

  float aMult = 0.95f;
  if (eBuffActive) aMult *= 0.58f;
  if (ultActive) aMult *= 0.50f;

  if (state == RoperaState::DASHING) {
    dashGraceTimer -= dt;
    if (dashGraceTimer <= 0) {
      state = RoperaState::NORMAL;
      dashGraceTimer = 0;
      velocity = Vector2Scale(velocity, 0.45f);
    }
    velocity = Vector2Scale(velocity, 0.94f); 
    Vector2 np = Vector2Add(position, Vector2Scale(velocity, dt));
    position = Arena::GetClampedPos(np, radius);
    
    static float ghostTimer = 0;
    ghostTimer += dt;
    if (ghostTimer >= 0.06f) {
      ghostTimer = 0;
      Graphics::VFXSystem::GetInstance().SpawnGhost(position, {0, 0, (float)radius * 2, (float)radius * 2}, 0.20f, Fade(GetHUDColor(), 0.15f), (facing.x > 0), 1.0f, {radius, radius}, ResourceManager::roperaDash);
    }

    if (GetRandomValue(0, 100) < 60) {
      Graphics::SpawnDashTrail(position);
    }
    return;
  }

  switch (state) {
  case RoperaState::NORMAL: {
    if (stunTimer > 0) break;

    float spd = 440.0f;
    if (moveSpeedBuffTimer > 0) spd = 640.0f;
    if (slowTimer > 0) spd *= 0.5f;
    if (isCharging) spd *= 0.20f;

    Vector2 move = {0, 0};
    if (IsKeyDown(KEY_A)) move.x -= 1;
    if (IsKeyDown(KEY_D)) move.x += 1;
    if (IsKeyDown(KEY_W)) move.y -= 1;
    if (IsKeyDown(KEY_S)) move.y += 1;

    if (Vector2Length(move) > 0) {
      move = Vector2Normalize(move);
      Vector2 np = Vector2Add(position, Vector2Scale(move, spd * dt));
      position = Arena::GetClampedPos(np, radius);
      if (moveSpeedBuffTimer > 0 && GetRandomValue(0,100) < 15) {
          Graphics::SpawnSpeedStreamer(position, Vector2Scale(move, spd));
      }
    }

    Vector2 aim = Vector2Subtract(targetAim, position);
    if (Vector2Length(aim) > 0) facing = Vector2Normalize(aim);

    if (IsKeyPressed(controls.dash) && CanDash()) {
      Vector2 bdir = (Vector2Length(move) > 0) ? Vector2Normalize(move) : facing;
      velocity = Vector2Scale(bdir, 880.0f);
      state = RoperaState::DASHING;
      dashGraceTimer = 0.45f;
      UseDash();
      Graphics::VFXSystem::GetInstance().SpawnGhost(position, {0, 0, (float)radius * 2, (float)radius * 2}, 0.35f, Fade(GetHUDColor(), 0.5f), (facing.x > 0), 1.0f, {radius, radius}, ResourceManager::roperaDash);
    }

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
      holdTimer += dt;
      isCharging = true;

      if (hasPerfectDodgeBuff && isCharging) {
          hasPerfectDodgeBuff = false;
          isPerfectCounter = true;
          state = RoperaState::HEAVY_ATTACK;
          attackPhase = AttackPhase::STARTUP;
          attackPhaseTimer = 0.05f;
          heavyHasHit = false;
          comboTimer = 0;
          isCharging = false;
          holdTimer = 0;
          velocity = Vector2Scale(facing, 2000.0f);
          // VFX: Holy counter al activarse el contraataque perfecto
          Graphics::SpawnHolyCounterVFX(position);
      }

      if (holdTimer >= 0.35f && isCharging) {
        bool canHeavy = true;
        if (!ultActive) {
          if (CanDash()) UseDash(); else canHeavy = false;
        } else {
          if (ultHeavyCharges > 0) ultHeavyCharges--; else canHeavy = false;
        }

        if (canHeavy) {
          velocity = Vector2Scale(facing, 1800.0f);
          state = RoperaState::HEAVY_ATTACK;
          attackPhase = AttackPhase::STARTUP;
          attackPhaseTimer = 0.10f * aMult;
          heavyHasHit = false;
          lungeHitIds.clear();
          hasLockedTarget = false;
          ultHeavySeqIndex = (ultHeavySeqIndex % 4) + 1;
          isCharging = false; 
          holdTimer = 0;
        }
      }
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
      if (isCharging) {
        state = RoperaState::ATTACKING;
        hasHit = false;
        attackPhase = AttackPhase::STARTUP;
        attackPhaseTimer = combo[comboStep].startup * aMult;
        comboTimer = 1.4f;
      }
      holdTimer = 0;
      isCharging = false;
    }

    if (IsKeyPressed(controls.boomerang) && qCooldown <= 0 && energy >= 20.0f && !ultActive) {
      energy -= 20.0f;
      qCooldown = qMaxCooldown;
      qActive = true;
      qSlashIndex = 0;
      qSlashActiveTimer = 0.18f * aMult;
      qSlashGapTimer = 0.0f;
      qHasHit = false;
      state = RoperaState::CASTING_Q;
    }

    if (IsKeyPressed(controls.berserker) && eCooldown <= 0 && energy >= 30.0f) {
      energy -= 30.0f;
      eCooldown = eMaxCooldown;
      eBuffTimer = 6.0f;
      eBuffActive = true;
    }

    if (IsKeyPressed(controls.ultimate) && ultCooldown <= 0 && !ultActive && hp < maxHp * 0.61f && energy >= 50.0f) {
      energy -= 50.0f;
      ultActive = true;
      ultTimer = 8.0f;
      ultCooldown = ultMaxCooldown;
      ultHeavyCharges = 4;
      InitSwords();
    }
    break;
  }

  case RoperaState::ATTACKING: {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) attackBufferTimer = BUFFER_WINDOW;


    attackPhaseTimer -= dt;
    if (hitCooldownTimer > 0) hitCooldownTimer -= dt;

    if (attackPhaseTimer <= 0) {
      switch (attackPhase) {
      case AttackPhase::STARTUP:
        attackPhase = AttackPhase::ATTACK_ACTIVE;
        attackPhaseTimer = combo[comboStep].active * aMult;
        hitCooldownTimer = 0;
        attackId++;
        if (comboStep < 2) velocity = Vector2Add(velocity, Vector2Scale(facing, 60.0f));
        break;
      case AttackPhase::ATTACK_ACTIVE:
        attackPhase = AttackPhase::RECOVERY;
        attackPhaseTimer = combo[comboStep].recovery * aMult;
        break;
      case AttackPhase::RECOVERY: {
        int stepLimit = ultActive ? 2 : 3;
        comboStep = (comboStep + 1) % stepLimit;
        if (attackBufferTimer > 0.0f) {
          attackBufferTimer = 0.0f;
          attackPhase = AttackPhase::STARTUP;
          attackPhaseTimer = combo[comboStep].startup * aMult;
          comboTimer = 1.4f;
          hasHit = false;
          Vector2 aim2 = Vector2Subtract(targetAim, position);
          if (Vector2Length(aim2) > 0) facing = Vector2Normalize(aim2);
        } else {
          state = RoperaState::NORMAL;
          attackPhase = AttackPhase::NONE;
          hasHit = false;
        }
        break;
      }
      default: break;
      }
    }

    if (IsKeyPressed(controls.dash) && CanDash()) {
      velocity = Vector2Scale(facing, 880.0f);
      state = RoperaState::DASHING;
      dashGraceTimer = 0.45f;
      UseDash();
      attackPhase = AttackPhase::NONE;
    }
    break;
  }

  case RoperaState::HEAVY_ATTACK: {
    attackPhaseTimer -= dt;
    if (attackPhase == AttackPhase::STARTUP) {
      Vector2 np = Vector2Add(position, Vector2Scale(velocity, dt));
      position = Arena::GetClampedPos(np, radius);
      velocity = Vector2Scale(velocity, 0.85f);
    } else if (attackPhase == AttackPhase::ATTACK_ACTIVE) {
      float activeSpd = ultActive ? 1600.0f : 120.0f;
      Vector2 np = Vector2Add(position, Vector2Scale(facing, activeSpd * dt));
      position = Arena::GetClampedPos(np, radius);
      static float lungeGhost = 0;
      lungeGhost += dt;
      if (lungeGhost > 0.015f) {
          lungeGhost = 0;
          Color gCol = ultActive ? Color{255, 180, 50, 255} : Color{100, 255, 220, 255};
          
          // Fix Sprite Ghost: Usar el frame actual en lugar de un rectangulo arbitrario
          Texture2D* currentSheet = &ResourceManager::ropera8Attack;
          int totalFrames = 12;
          int dirIdx = GetDirectionIndex(facing);
          int frame = 8; // Usar el frame de estocada (el final del set 3)
          float fw = (float)currentSheet->width / totalFrames;
          float fh = (float)currentSheet->height / 8;
          Rectangle ghostSrc = { (float)frame * fw, (float)dirIdx * fh, fw, fh };
          
          Graphics::VFXSystem::GetInstance().SpawnGhost(position, ghostSrc, 0.35f, Fade(gCol, 0.5f), (facing.x > 0), 2.5f, {fw*1.25f, fh*1.0f}, *currentSheet);
      }
    }

    if (attackPhaseTimer <= 0) {
      switch (attackPhase) {
      case AttackPhase::STARTUP:
        attackPhase = AttackPhase::ATTACK_ACTIVE;
        attackPhaseTimer = 0.14f;
        lungeHitIds.clear();
        attackId++;
        break;
      case AttackPhase::ATTACK_ACTIVE:
        attackPhase = AttackPhase::RECOVERY;
        attackPhaseTimer = (ultActive ? 0.22f : 0.35f); 
        break;
      case AttackPhase::RECOVERY:
        if (ultActive && ultHeavyCharges > 0) {
            ultHeavyCharges--;
            state = RoperaState::HEAVY_ATTACK;
            attackPhase = AttackPhase::STARTUP;
            attackPhaseTimer = 0.08f; 
            lungeHitIds.clear();
            attackId++;
            if (hasLockedTarget) {
                Vector2 diff = Vector2Subtract(lockedHeavyTargetPos, position);
                float distToBoss = Vector2Length(diff);
                if (distToBoss > 0) facing = Vector2Normalize(diff);
                velocity = Vector2Scale(facing, fminf(distToBoss * 12.0f, 2200.0f)); // Mas rapido y preciso
            } else {
                velocity = Vector2Scale(facing, 1600.0f);
            }
        } else {
            state = RoperaState::NORMAL;
            attackPhase = AttackPhase::NONE;
            heavyHasHit = false;
            lungeHitIds.clear();
            hasLockedTarget = false;
            isPerfectCounter = false;
            if (ultActive) ultHeavySeqIndex = 0; 
        }
        break;
      default: break;
      }
    }
    break;
  }

  case RoperaState::CASTING_Q: {
    if (qSlashActiveTimer > 0) {
      qSlashActiveTimer -= dt;
    } else if (qSlashIndex == 0) {
      qSlashGapTimer -= dt;
      if (qSlashGapTimer <= 0) {
        qSlashIndex = 1;
        qSlashActiveTimer = 0.18f * aMult;
        qHasHit = false;
        attackId++;
      }
    } else {
      qActive = false;
      state = RoperaState::NORMAL;
    }
    if (qSlashIndex == 0 && qSlashActiveTimer <= 0 && qSlashGapTimer <= 0) {
      qSlashGapTimer = 0.10f * aMult;
    }
    break;
  }
  default: break;
  } 

  velocity = Vector2Scale(velocity, 0.87f);
  if (state != RoperaState::HEAVY_ATTACK || attackPhase != AttackPhase::STARTUP) {
    Vector2 np = Vector2Add(position, Vector2Scale(velocity, dt));
    position = Arena::GetClampedPos(np, radius);
  }

  if (state != RoperaState::ATTACKING && comboTimer > 0) {
    comboTimer -= dt;
    if (comboTimer <= 0) comboStep = 0;
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
  isPerfectCounter = false;
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
  hasHit = false;
  for (int i = 0; i < 3; i++) {
    swords[i].active = false;
    swords[i].swordState = SwordState::BEHIND;
    swords[i].hasDealt = false;
  }
}

// =====================================================
// HANDLE SKILLS
// =====================================================
void Ropera::HandleSkills(Boss &boss) {
  float dt = GetFrameTime() * g_timeScale;
  UpdateSwords(dt, boss);
}

// =====================================================
// CHECK COLLISIONS
// =====================================================
void Ropera::CheckCollisions(Boss &boss) {
  if (boss.isDead || boss.isDying || boss.IsInvulnerable()) return;

  if (CheckComboCollision(boss)) {
    int step = comboStep;
    float dmg = combo[step].damage;
    if (eBuffActive) dmg += boss.maxHp * eMaxHpBonusFrac;
    if (ultActive) dmg *= 0.65f; // Reducido significativamente para compensar las espadas
    dmg *= rpg.DamageMultiplierPhysical();
    lastDamageType = DamageType::PHYSICAL;
    boss.TakeDamage(dmg, comboStep == 2 ? 8.0f : 4.0f, {0, 0});
    energy = fminf(maxEnergy, energy + 5.0f);
    if (eBuffActive) hp = fminf(maxHp, hp + dmg * eLifestealFrac);
    hitstopTimer = 0.08f;
    velocity = Vector2Add(velocity, Vector2Scale(facing, 120.0f));
    if (comboStep == 2) screenShake = fmaxf(screenShake, 1.8f); 
    if (ultActive) Graphics::SpawnStyledBlood(boss.position, facing);
    if (ultActive) TriggerSwords(boss.position);
    // VFX: Tajo sprite sobre el punto de impacto
    Graphics::SpawnSlashVFX(boss.position, facing, comboStep);
  }

  if (CheckHeavyCollision(boss)) {
    float dmg = 50.0f; 
    if (isPerfectCounter) {
        energy = fminf(maxEnergy, energy + 30.0f);
        isPerfectCounter = false;
        Graphics::SpawnHolyCounterVFX(position);
        Graphics::SpawnHolyImpactVFX(boss.position);
        dmg *= 1.5f;
    }
    if (eBuffActive) dmg += boss.maxHp * eMaxHpBonusFrac;
    dmg *= rpg.DamageMultiplierPhysical();
    lastDamageType = DamageType::PHYSICAL;
    boss.TakeDamage(dmg, 80.0f, {0, 0});
    energy = fminf(maxEnergy, energy + 10.0f);
    if (eBuffActive) hp = fminf(maxHp, hp + dmg * eLifestealFrac);
    hitstopTimer = 0.18f;
    velocity = Vector2Add(velocity, Vector2Scale(facing, 385.0f)); 
    if (ultActive) {
      TriggerSwords(boss.position);
      hasLockedTarget = true;
      lockedHeavyTargetPos = boss.position;
      boss.velocity = {0, 0};
      Graphics::SpawnStyledBlood(boss.position, facing); 
    }
    // VFX: Slash pesado sobre el punto de impacto
    Graphics::SpawnHeavySlashVFX(boss.position, facing, ultActive);
    Color impactCol = ultActive ? Color{255, 140, 0, 255} : Color{0, 255, 200, 255};
    Graphics::SpawnImpactBurst(boss.position, facing, WHITE, impactCol, 12, 8);
    Graphics::SpawnSonicBoom(boss.position, 280.0f);
    screenShake = fmaxf(screenShake, 3.5f); 
  }

  if (qActive && state == RoperaState::CASTING_Q && qSlashActiveTimer > 0) {
    if (CheckQCollision(boss, qSlashIndex)) {
      float dmg = 24.0f * rpg.DamageMultiplierPhysical(); 
      lastDamageType = DamageType::PHYSICAL;
      boss.TakeDamage(dmg, 20.0f, {0, 0});
      energy = fminf(maxEnergy, energy + 8.0f);
      qHasHit = true;
      if (eBuffActive) hp = fminf(maxHp, hp + dmg * eLifestealFrac);
      hitstopTimer = 0.05f;
      velocity = Vector2Add(velocity, Vector2Scale(facing, 350.0f));
      if (!ultActive) moveSpeedBuffTimer = 3.5f;
    }
  }
}

// =====================================================
// CHECK COMBO COLLISION
// =====================================================
bool Ropera::CheckComboCollision(Boss &boss) {
  if (state != RoperaState::ATTACKING || attackPhase != AttackPhase::ATTACK_ACTIVE) return false;
  float attackMult = (eBuffActive ? 0.58f : (ultActive ? 0.50f : 1.0f));
  float activeTotal = combo[comboStep].active * attackMult;
  float progress = 0.20f + CombatUtils::GetProgress(attackPhaseTimer, activeTotal) * 4.5f;
  if (progress > 1.0f) progress = 1.0f;
  float maxRange = combo[comboStep].range * (ultActive ? 0.75f : 1.0f);
  float angleWidth = combo[comboStep].angleWidth;
  if (hitCooldownTimer > 0) return false;

  bool hitDetected = false;
  if (comboStep == 0 || comboStep == 2) {
      hitDetected = CombatUtils::CheckProgressiveThrust(position, facing, boss.position, boss.radius, maxRange, angleWidth / 2.0f, progress);
  } else if (comboStep == 1) {
      hitDetected = CombatUtils::CheckProgressiveSweep(position, facing, boss.position, boss.radius, maxRange, -angleWidth / 2.0f, angleWidth, 1.0f, progress);
  }

  if (hitDetected) {
      if (comboStep == 2) {
          if (!hasHit) { hasHit = true; return true; }
      } else {
          hitCooldownTimer = combo[comboStep].hitCooldown;
          hasHit = true;
          return true;
      }
  }
  return false;
}

// =====================================================
// CHECK HEAVY COLLISION
// =====================================================
bool Ropera::CheckHeavyCollision(Boss &boss) {
  if (state != RoperaState::HEAVY_ATTACK || attackPhase != AttackPhase::ATTACK_ACTIVE) return false;
  for (int id : lungeHitIds) { if ((size_t)&boss == (size_t)id) return false; }
  float progress = CombatUtils::GetProgress(attackPhaseTimer, 0.14f) * 6.0f;
  if (progress > 1.0f) progress = 1.0f;
  float ultScale = ultActive ? 0.75f : 1.0f;
  float maxRange = 210.0f * ultScale;
  float angleWidth = ultActive ? 35.0f : 12.0f;
  if (CombatUtils::CheckProgressiveThrust(position, facing, boss.position, boss.radius, maxRange, angleWidth/2.0f, progress)) {
    lungeHitIds.push_back((int)(size_t)&boss); return true;
  }
  return false;
}

// =====================================================
// CHECK Q COLLISION
// =====================================================
bool Ropera::CheckQCollision(Boss &boss, int slashIdx) {
  if (qHasHit) return false;
  float offsetDeg = (slashIdx == 0) ? -45.0f : 45.0f;
  float sweepDir = (slashIdx == 0) ? 1.0f : -1.0f;
  float progress = CombatUtils::GetProgress(qSlashActiveTimer, 0.18f * (eBuffActive ? 0.58f : 1.0f)) * 2.2f;
  if (progress > 1.0f) progress = 1.0f;
  return CombatUtils::CheckProgressiveSweep(position, facing, boss.position, boss.radius, 155.0f, offsetDeg, 90.0f, sweepDir, progress);
}

// =====================================================
// GET ABILITIES
// =====================================================
std::vector<AbilityInfo> Ropera::GetAbilities() const {
  std::vector<AbilityInfo> abs;
  float currentDashCD = (dashCooldown1 > 0 && dashCooldown2 > 0) ? fminf(dashCooldown1, dashCooldown2) : ((dashCooldown1 > 0) ? dashCooldown1 : dashCooldown2);
  abs.push_back({TextFormat("DASH [%d]", dashCharges), currentDashCD, dashMaxCD, 0.0f, CanDash(), {80, 200, 220, 255}});
  abs.push_back({"Q Tajos", qCooldown, qMaxCooldown, 20.0f, qCooldown <= 0 && energy >= 20.0f && !ultActive, {0, 220, 180, 255}});
  abs.push_back({"E Furia", eCooldown, eMaxCooldown, 30.0f, eCooldown <= 0 && energy >= 30.0f, {60, 255, 180, 255}});
  abs.push_back({"R Garras", ultCooldown, ultMaxCooldown, 50.0f, ultCooldown <= 0 && !ultActive && hp / maxHp < 0.60f && energy >= 50.0f, {255, 120, 0, 255}});
  return abs;
}

// =====================================================
// GET SPECIAL STATUS
// =====================================================
std::string Ropera::GetSpecialStatus() const {
  if (ultActive) return TextFormat("[ GARRAS %.1fs ]", ultTimer);
  if (eBuffActive) return TextFormat("[ FURIA %.1fs ]", eBuffTimer);
  if (moveSpeedBuffTimer > 0) return "[ VEL+ ]";
  return "";
}

// =====================================================
// DRAW
// =====================================================
void Ropera::Draw() {
  float t = (float)g_gameTime;
  float ap = 0.3f + 0.15f * sinf(t * 3.2f);
  Color ac = ultActive ? Color{255, 120, 0, 255} : (eBuffActive ? Color{0, 255, 150, 255} : (moveSpeedBuffTimer > 0 ? Color{80, 255, 220, 255} : WHITE));
  DrawCircleGradient((int)position.x, (int)position.y - 20, radius * (ultActive ? 3.8f : 1.9f), Fade(ac, ap * (ultActive ? 0.9f : 0.5f)), Fade(ac, 0));

  Texture2D* currentSheet = &ResourceManager::ropera8Idle;
  int totalFrames = 6; 
  if (state == RoperaState::ATTACKING || state == RoperaState::HEAVY_ATTACK || state == RoperaState::CASTING_Q) { currentSheet = &ResourceManager::ropera8Attack; totalFrames = 12; }
  else if (Vector2Length(velocity) > 10.0f) { currentSheet = &ResourceManager::ropera8Run; totalFrames = 8; }

  if (currentSheet->id != 0) {
    int dirIdx = GetDirectionIndex(facing);
    int frame = 0;
    if (state == RoperaState::ATTACKING) {
        float aMult = (eBuffActive ? 0.58f : (ultActive ? 0.50f : 1.0f));
        float recovery = combo[comboStep].recovery * aMult;

        if (attackPhase == AttackPhase::STARTUP) {
            frame = comboStep * 4 + 0;
        } else if (attackPhase == AttackPhase::ATTACK_ACTIVE) {
            frame = comboStep * 4 + 1;
        } else { // RECOVERY
            float recProgress = (recovery - attackPhaseTimer) / recovery;
            frame = comboStep * 4 + (recProgress > 0.5f ? 3 : 2);
        }
    } else if (state == RoperaState::HEAVY_ATTACK) {
        float total = 0.14f + (ultActive ? 0.22f : 0.35f) + 0.14f; 
        float elapsed = total - attackPhaseTimer;
        frame = 8 + (int)((elapsed / total) * 4) % 4; 
    } else {
        frame = (int)(t / 0.12f) % totalFrames;
    }
    float frameW = (float)currentSheet->width / totalFrames;
    float frameH = (float)currentSheet->height / 8;
    Rectangle src = { (float)frame * frameW, (float)dirIdx * frameH, frameW, frameH };
    Rectangle dest = { position.x, position.y - 20, radius * 5.0f, radius * 5.0f };
    Vector2 origin = { dest.width / 2.0f, dest.height / 1.2f };
    DrawTexturePro(*currentSheet, src, dest, origin, 0.0f, (hitFlashTimer > 0 ? WHITE : ac));
  } else {
    DrawCircleV({position.x, position.y - 20}, radius, ac);
  }

  if ((state == RoperaState::ATTACKING || state == RoperaState::HEAVY_ATTACK) && attackPhase != AttackPhase::NONE) {
    float attackMult = (eBuffActive ? 0.58f : (ultActive ? 0.50f : 1.0f));
    float maxRange = (state == RoperaState::HEAVY_ATTACK) ? (210.0f * (ultActive ? 0.75f : 1.0f)) : combo[comboStep].range * (ultActive ? 0.75f : 1.0f);
    float angW = (state == RoperaState::HEAVY_ATTACK) ? (ultActive ? 35.0f : 12.0f) : combo[comboStep].angleWidth;
    float progress = CombatUtils::GetProgress(attackPhaseTimer, (state == RoperaState::HEAVY_ATTACK ? 0.14f : combo[comboStep].active * attackMult));
    if (progress > 1.0f) progress = 1.0f;
    float drawRange = maxRange * progress;
    float facingAngle = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
    float sa = facingAngle - angW/2.0f;
    Color sc = (state == RoperaState::HEAVY_ATTACK) ? RED : (ultActive ? Color{255, 160, 0, 255} : Color{0, 255, 200, 255});
    rlPushMatrix(); rlTranslatef(position.x, position.y - 20, 0); rlScalef(1.0f, 0.5f, 1.0f);
    if (state == RoperaState::ATTACKING) {
      if (comboStep == 0 || comboStep == 2) {
          float alpha = (comboStep == 2) ? 0.7f : 0.6f;
          DrawCircleSector({0, 0}, drawRange, sa, sa + angW, 24, Fade(sc, alpha));
      } else if (comboStep == 1) {
          DrawCircleSector({0, 0}, maxRange, sa, sa + angW * progress, 24, Fade(sc, 0.6f));
      }
    } else { DrawCircleSector({0, 0}, drawRange, sa, sa + angW, 16, Fade(sc, 0.6f)); }
    rlPopMatrix();
  }

  if (qActive && state == RoperaState::CASTING_Q && qSlashActiveTimer > 0) {
    float qMult = (eBuffActive ? 0.58f : 1.0f);
    float progress = (qSlashIndex == 0) ? (1.0f - (qSlashActiveTimer / (0.18f * qMult)) * 0.5f) : (0.5f + (1.0f - (qSlashActiveTimer / (0.18f * qMult))) * 0.5f);
    if (ResourceManager::roperaTajoDoble.id != 0) {
      int frames = ResourceManager::roperaTajoFrames;
      int frame = (int)(progress * frames) % frames;
      float fw = (float)ResourceManager::roperaTajoDoble.width / frames;
      float ang = atan2f(facing.y, facing.x) * RAD2DEG + (qSlashIndex == 0 ? -28.0f : 28.0f);
      Rectangle src = { (float)frame * fw, 0, (facing.x < 0 ? -fw : fw), (float)ResourceManager::roperaTajoDoble.height };
      float sc = 2.4f;
      DrawTexturePro(ResourceManager::roperaTajoDoble, src, {position.x, position.y - 20, fw * sc, (float)ResourceManager::roperaTajoDoble.height * sc}, {fw*sc*0.5f, (float)ResourceManager::roperaTajoDoble.height*sc*0.5f}, ang, WHITE);
    }
  }

  if (isCharging && holdTimer > 0) {
    float cp = fminf(holdTimer / 0.35f, 1.0f);
    DrawCircleLines((int)position.x, (int)position.y - 20, radius + 8.0f + 20.0f * (1.0f - cp), Fade(ac, cp));
    if (hasPerfectDodgeBuff) {
      float pulse = 0.8f + 0.2f * sinf(t * 15.0f);
      DrawCircleGradient((int)position.x, (int)position.y - 20, radius * 3.5f, Fade(GOLD, 0.4f * pulse), Fade(GOLD, 0));
    }
  }

  if (state == RoperaState::DASHING) DrawCircleGradient((int)position.x, (int)position.y - 20, radius * 2.5f, Fade({0, 220, 180, 255}, 0.4f), Fade({0, 220, 180, 255}, 0));
  if (moveSpeedBuffTimer > 0) {
    rlPushMatrix(); rlTranslatef(position.x, position.y, 0); rlScalef(1.0f, 0.5f, 1.0f);
    DrawCircleLines(0, 0, radius + 15.0f, Fade({0, 255, 200, 255}, 0.5f + 0.4f * sinf(t * 6.0f)));
    rlPopMatrix();
  }

  if (ultActive) {
    for (int i = 0; i < 3; i++) {
      if (!swords[i].active) continue;
      Vector2 sp = swords[i].position;
      bool firing = (swords[i].swordState == SwordState::FIRING && swords[i].fireDelayTimer <= 0);
      Color sc = swords[i].flashTimer > 0 ? Color{255, 80, 0, 255} : (firing ? Color{255, 200, 60, 255} : Color{220, 230, 255, 255});
      float ang = atan2f(sp.y - position.y, sp.x - position.x) * RAD2DEG;
      DrawRectanglePro({sp.x, sp.y, 32.0f, 5.0f}, {16.0f, 2.5f}, ang, sc);
      DrawRectanglePro({sp.x, sp.y, 5.0f, 14.0f}, {2.5f, 7.0f}, ang + 90.0f, Fade(LIGHTGRAY, 0.8f));
      if (firing) {
        Vector2 tail = Vector2Subtract(sp, Vector2Scale(Vector2Normalize(Vector2Subtract(swords[i].targetPos, sp)), 20.0f));
        DrawLineEx(sp, tail, 3.0f, Fade({255, 200, 60, 255}, 0.5f));
      }
    }
  }
}
