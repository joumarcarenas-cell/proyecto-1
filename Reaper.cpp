// =====================================================
// Reaper.cpp - Implementacion del Personaje Segador
// =====================================================
#include "entities.h"
#include "rlgl.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

#include "include/graphics/VFXSystem.h"

// =====================================================
// Helper: Dentro del diamante isometrico
// =====================================================
static bool InsideArena(Vector2 pos, float radius) {
    float dx = std::abs(pos.x - 2000.0f);
    float dy = std::abs(pos.y - 2000.0f);
    return (dx + 2.0f * dy) <= (1400.0f - radius * 2.236f);
}

// =====================================================
// REAPER - UPDATE
// =====================================================
void Reaper::Update() {
    float dt = GetFrameTime();

    // --- Cooldowns globales ---
    if (hitFlashTimer > 0) hitFlashTimer -= dt;
    if (dashCooldown > 0) dashCooldown -= dt;
    if (qCooldown    > 0) qCooldown    -= dt;
    if (eCooldown    > 0) eCooldown    -= dt;
    if (ultCooldown  > 0) ultCooldown  -= dt;
    if (inputBufferTimer > 0) inputBufferTimer -= dt;

    // --- Buff timer (Fase 3 Ult) ---
    if (buffTimer > 0) { buffTimer -= dt; isBuffed = true; }
    else               { isBuffed = false; }

    float attackMult = (isBuffed ? 0.65f : 1.0f);

    // =====================================================
    // UPDATE: Ground Bursts (Q) – cadena secuencial
    // =====================================================
    if (qActive) {
        qBurstTimer -= dt;
        if (qBurstTimer <= 0.0f && qBurstsSpawned < 5) {
            // Spawner del siguiente estallido
            bool isTip = (qBurstsSpawned == 4);
            float burstRadius = isTip ? 60.0f : 35.0f;
            float burstDmg    = isTip ? 7.2f  : 4.5f;  // dano reducido en 10%

            // Posicion: avanza 100px por estallido a lo largo de qBurstDir
            float dist = (qBurstsSpawned + 1) * 55.0f;  // 55px por paso (reducido 50%)
            GroundBurst& gb = groundBursts[qBurstsSpawned];
            gb.position      = Vector2Add(qBurstOrigin, Vector2Scale(qBurstDir, dist));
            gb.radius        = burstRadius;
            gb.visualRadius  = burstRadius * 0.1f;
            gb.lifetime      = 0.55f;
            gb.maxLifetime   = 0.55f;
            gb.active        = true;
            gb.hasDealtDamage = false;
            gb.isTip         = isTip;
            gb.damage        = burstDmg;

            qBurstsSpawned++;
            qBurstTimer = (qBurstsSpawned < 5) ? 0.1f : 0.0f;
        }
        if (qBurstsSpawned >= 5) qActive = false;
    }

    // Update de cada burst activo
    for (int i = 0; i < 5; i++) groundBursts[i].Update(dt);

    // =====================================================
    // UPDATE: Sombras de la Ultimate
    // =====================================================
    for (int i = 0; i < 2; i++) {
        if (ultShadows[i].active) {
            ultShadows[i].lifetime -= dt;
            ultShadows[i].position = Vector2Add(ultShadows[i].position,
                                                 Vector2Scale(ultShadows[i].velocity, dt));
            if (ultShadows[i].lifetime <= 0) ultShadows[i].active = false;
        }
    }

    // =====================================================
    // MAQUINA DE ESTADOS
    // =====================================================

    // --- INPUT LOCK durante la Ult (fases 1 y 2) ---
    bool inputLocked = (ultSeqPhase == 1 || ultSeqPhase == 2);

    // --- CC Timers ---
    if (stunTimer > 0) stunTimer -= dt;
    if (slowTimer > 0) slowTimer -= dt;

    switch (state) {

    // ─── NORMAL ──────────────────────────────────────────
    case ReaperState::NORMAL: {
        if (!inputLocked && stunTimer <= 0) {
            float currentSpeed = (isBuffed ? 560.0f : 400.0f);
            if (slowTimer > 0) currentSpeed *= 0.5f;

            Vector2 move = {0, 0};
            if (IsKeyDown(KEY_W)) move.y -= 1;
            if (IsKeyDown(KEY_S)) move.y += 1;
            if (IsKeyDown(KEY_A)) move.x -= 1;
            if (IsKeyDown(KEY_D)) move.x += 1;

            // Ralentizar al 50% durante la carga del heavy
            if (isCharging && holdTimer > 0) currentSpeed *= 0.5f;

            if (Vector2Length(move) > 0) {
                move = Vector2Normalize(move);
                Vector2 nextPos = Vector2Add(position, Vector2Scale(move, currentSpeed * dt));
                if (InsideArena(nextPos, radius)) position = nextPos;
            }

            // Facing hacia el mouse
            Vector2 aimDiff = Vector2Subtract(targetAim, position);
            if (Vector2Length(aimDiff) > 0) facing = Vector2Normalize(aimDiff);

            // --- Hold Click: iniciar CHARGING_HEAVY ---
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                holdTimer += dt;
                isCharging = true;
            }

            // --- Release Click ---
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && isCharging) {
                if (holdTimer >= 0.35f) {
                    // Ataque cargado: mini-dash + heavy attack -> impulsado mas lejos (ajuste hacia adelante)
                    velocity      = Vector2Scale(facing, 1500.0f);
                    miniDashTimer = 0.20f;
                    heavyHasHit   = false;
                    state         = ReaperState::CHARGING_HEAVY;
                } else {
                    // Click rapido: combo
                    state            = ReaperState::ATTACKING;
                    hasHit           = false;
                    attackPhase      = AttackPhase::STARTUP;
                    attackPhaseTimer = combo[comboStep].startup * attackMult;
                    comboTimer       = 1.2f;
                }
                holdTimer  = 0.0f;
                isCharging = false;
            }

            // --- Dash (Blink) ---
            if (IsKeyPressed(controls.dash) && dashCooldown <= 0) {
                Vector2 blinkDir = facing;
                {
                    Vector2 move2 = {0,0};
                    if (IsKeyDown(KEY_W)) move2.y -= 1;
                    if (IsKeyDown(KEY_S)) move2.y += 1;
                    if (IsKeyDown(KEY_A)) move2.x -= 1;
                    if (IsKeyDown(KEY_D)) move2.x += 1;
                    if (Vector2Length(move2) > 0) blinkDir = Vector2Normalize(move2);
                }
                Vector2 newPos = Vector2Add(position, Vector2Scale(blinkDir, blinkDistance));
                if (!InsideArena(newPos, radius)) {
                    float safeLen = 0.0f;
                    for (float t = blinkDistance; t > 10.0f; t -= 8.0f) {
                        if (InsideArena(Vector2Add(position, Vector2Scale(blinkDir, t)), radius)) {
                            safeLen = t; break;
                        }
                    }
                    newPos = Vector2Add(position, Vector2Scale(blinkDir, safeLen));
                }
                position     = newPos;
                state        = ReaperState::DASHING;
                dashCooldown = 1.8f;
            }

            // --- Habilidad Q: Ground Bursts secuenciales ---
            if (IsKeyPressed(controls.boomerang) && qCooldown <= 0 && energy >= 25.0f) {
                energy        -= 25.0f;
                qCooldown     = 8.0f;
                StartGroundBurstChain();
            }

            // --- Habilidad E: Orbes Teledirigidos ---
            if (IsKeyPressed(controls.berserker) && eCooldown <= 0 && energy >= 40.0f) {
                state    = ReaperState::CASTING_E;
                energy  -= 40.0f;
                eCooldown = 12.0f;
            }

            // --- Ultimate ---
            if (IsKeyPressed(controls.ultimate) && ultCooldown <= 0 &&
                energy >= 80.0f && ultSeqPhase == 0) {
                energy -= 80.0f;
                // Señal para HandleSkills
                ultSeqPhase = -1;
            }
        }
        break;
    }

    // ─── ATTACKING (Combo de 3 hits) ─────────────────────
    case ReaperState::ATTACKING: {
        Vector2 aimDiff = Vector2Subtract(targetAim, position);
        if (Vector2Length(aimDiff) > 0) facing = Vector2Normalize(aimDiff);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            inputBufferTimer = 0.25f;
        }

        attackPhaseTimer -= dt;
        if (hitCooldownTimer > 0) hitCooldownTimer -= dt;

        if (attackPhaseTimer <= 0) {
            switch (attackPhase) {
                case AttackPhase::STARTUP:
                    attackPhase      = AttackPhase::ATTACK_ACTIVE;
                    attackPhaseTimer = combo[comboStep].active * attackMult;
                    velocity = Vector2Add(velocity, Vector2Scale(facing, 126.0f));
                    hitCooldownTimer = 0.0f;
                    attackId++;
                    break;
                case AttackPhase::ATTACK_ACTIVE:
                    attackPhase      = AttackPhase::RECOVERY;
                    attackPhaseTimer = combo[comboStep].recovery * attackMult;
                    break;
                case AttackPhase::RECOVERY:
                    comboStep = (comboStep + 1) % 3;
                    if (inputBufferTimer > 0.0f) {
                        inputBufferTimer = 0.0f;
                        attackPhase      = AttackPhase::STARTUP;
                        attackPhaseTimer = combo[comboStep].startup * attackMult;
                        comboTimer       = 1.2f;
                        hasHit           = false;
                    } else {
                        state        = ReaperState::NORMAL;
                        attackPhase  = AttackPhase::NONE;
                        hasHit       = false;
                    }
                    break;
                default: break;
            }
        }
        // Cancelar con dash
        if (IsKeyPressed(controls.dash) && dashCooldown <= 0) {
            Vector2 newPos = Vector2Add(position, Vector2Scale(facing, blinkDistance));
            if (!InsideArena(newPos, radius)) {
                float safeLen = 0.0f;
                for (float t = blinkDistance; t > 10.0f; t -= 8.0f) {
                    if (InsideArena(Vector2Add(position, Vector2Scale(facing, t)), radius)) { safeLen = t; break; }
                }
                newPos = Vector2Add(position, Vector2Scale(facing, safeLen));
            }
            position     = newPos;
            state        = ReaperState::DASHING;
            attackPhase  = AttackPhase::NONE;
            dashCooldown = 1.8f;
        }
        break;
    }

    // ─── CHARGING_HEAVY (Mini-Dash + Tajo Frontal) ───────
    case ReaperState::CHARGING_HEAVY: {
        miniDashTimer -= dt;
        if (miniDashTimer > 0) {
            Vector2 nextPos = Vector2Add(position, Vector2Scale(velocity, dt));
            if (InsideArena(nextPos, radius)) position = nextPos;
            velocity = Vector2Scale(velocity, 0.82f);
        } else {
            state            = ReaperState::HEAVY_ATTACK;
            attackPhase      = AttackPhase::STARTUP;
            attackPhaseTimer = 0.08f;
            velocity         = {0, 0};
            heavyHasHit      = false;
        }
        break;
    }

    // ─── HEAVY_ATTACK (Tajo Frontal Cargado) ─────────────
    case ReaperState::HEAVY_ATTACK: {
        attackPhaseTimer -= dt;
        if (attackPhaseTimer <= 0) {
            switch (attackPhase) {
                case AttackPhase::STARTUP:
                    attackPhase      = AttackPhase::ATTACK_ACTIVE;
                    attackPhaseTimer = 0.20f;
                    attackId++;
                    break;
                case AttackPhase::ATTACK_ACTIVE:
                    attackPhase      = AttackPhase::RECOVERY;
                    attackPhaseTimer = 0.45f;
                    break;
                case AttackPhase::RECOVERY:
                    state       = ReaperState::NORMAL;
                    attackPhase = AttackPhase::NONE;
                    break;
                default: break;
            }
        }
        break;
    }

    // ─── DASHING (Blink – i-frames breves) ───────────────
    case ReaperState::DASHING: {
        static float blinkGrace = 0.0f;
        blinkGrace += dt;
        if (blinkGrace >= 0.08f) {
            blinkGrace = 0.0f;
            state      = ReaperState::NORMAL;
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
                ultSeqPhase   = 2;
                attackPhase   = AttackPhase::STARTUP;
                attackPhaseTimer = 0.20f;  // Startup breve cinematico
                ultFinalSlash    = true;
                ultFinalSlashHit = false;
                attackId++;
            }
        } 
        else if (ultSeqPhase == 2) { // Fase de Tajo Final Automático
            attackPhaseTimer -= dt;
            if (attackPhaseTimer <= 0) {
                switch (attackPhase) {
                    case AttackPhase::STARTUP:
                        attackPhase      = AttackPhase::ATTACK_ACTIVE;
                        attackPhaseTimer = 0.25f;
                        break;
                    case AttackPhase::ATTACK_ACTIVE:
                        attackPhase      = AttackPhase::RECOVERY;
                        attackPhaseTimer = 0.50f;
                        break;
                    case AttackPhase::RECOVERY:
                        isTimeStopped  = false;
                        ultSeqPhase    = 3; // Buff
                        buffTimer      = 6.0f;
                        ultCooldown    = 25.0f;
                        state          = ReaperState::ULT_PHASE3;
                        attackPhase    = AttackPhase::NONE;
                        ultFinalSlash  = false;
                        break;
                    default: break;
                }
            }
        }
        break;
    }

    // ─── ULT_PHASE3 (Buff activo – movimiento libre) ─────
    case ReaperState::ULT_PHASE3: {
        if (buffTimer <= 0) {
            state       = ReaperState::NORMAL;
            isBuffed    = false;
            ultSeqPhase = 0;
        }
        // Movimiento libre durante el buff
        {
            float currentSpeed = 560.0f; // Buffed speed
            Vector2 move = {0, 0};
            if (IsKeyDown(KEY_W)) move.y -= 1;
            if (IsKeyDown(KEY_S)) move.y += 1;
            if (IsKeyDown(KEY_A)) move.x -= 1;
            if (IsKeyDown(KEY_D)) move.x += 1;
            if (Vector2Length(move) > 0) {
                move = Vector2Normalize(move);
                Vector2 nextPos = Vector2Add(position, Vector2Scale(move, currentSpeed * dt));
                if (InsideArena(nextPos, radius)) position = nextPos;
            }
            Vector2 aimDiff = Vector2Subtract(targetAim, position);
            if (Vector2Length(aimDiff) > 0) facing = Vector2Normalize(aimDiff);
            // Habilidades basicas disponibles durante el buff
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                state            = ReaperState::ATTACKING;
                hasHit           = false;
                attackPhase      = AttackPhase::STARTUP;
                attackPhaseTimer = combo[comboStep].startup * 0.65f; // attackMult buffed
                comboTimer       = 1.2f;
            }
            if (IsKeyPressed(controls.dash) && dashCooldown <= 0) {
                Vector2 blinkDir = (Vector2Length(move) > 0) ? move : facing;
                Vector2 newPos   = Vector2Add(position, Vector2Scale(blinkDir, blinkDistance));
                if (!InsideArena(newPos, radius)) {
                    float safeLen = 0.0f;
                    for (float t = blinkDistance; t > 10.0f; t -= 8.0f)
                        if (InsideArena(Vector2Add(position, Vector2Scale(blinkDir, t)), radius)) { safeLen = t; break; }
                    newPos = Vector2Add(position, Vector2Scale(blinkDir, safeLen));
                }
                position     = newPos;
                state        = ReaperState::DASHING;
                dashCooldown = 1.8f;
            }
        }
        break;
    }
    } // end switch

    // --- Friccion general (excepto Input Lock) ---
    if (!inputLocked) velocity = Vector2Scale(velocity, 0.88f);

    // --- Fisica base (excepto Blink y Input Lock) ---
    if (state != ReaperState::DASHING && !inputLocked) {
        Vector2 nextPhysPos = Vector2Add(position, Vector2Scale(velocity, dt));
        if (InsideArena(nextPhysPos, radius)) position = nextPhysPos;
        else velocity = {0, 0};
    }

    // --- Combo timer reset ---
    if (state != ReaperState::ATTACKING && comboTimer > 0) {
        comboTimer -= dt;
        if (comboTimer <= 0) comboStep = 0;
    }

    // =====================================================
    // UPDATE: Proyectiles activos (Homing orbs / sombras)
    // =====================================================
    for (int i = (int)activeProjectiles.size() - 1; i >= 0; i--) {
        Projectile& p = activeProjectiles[i];
        if (!p.active) { activeProjectiles.erase(activeProjectiles.begin() + i); continue; }

        // Trail
        for (int j = 7; j > 0; j--) { p.trail[j] = p.trail[j - 1]; }
        p.trail[0] = p.position;
        if (p.trailCount < 8) p.trailCount++;

        if (p.isShadow || p.isHoming) {
            p.position = Vector2Add(p.position, Vector2Scale(p.direction, p.speed * dt));
            if (Vector2Distance(p.startPos, p.position) > p.maxDistance) p.active = false;
        }
    }
}

void Reaper::Reset(Vector2 pos) {
    position = pos;
    hp = maxHp;
    energy = 100.0f;
    velocity = {0,0};
    activeProjectiles.clear();
    state = ReaperState::NORMAL;
    ultSeqPhase = 0;
    ultCooldown = 0.0f;
    buffTimer   = 0.0f;
    isBuffed    = false;
    qCooldown   = 0.0f;
    dashCooldown = 0.0f;
    eCooldown   = 0.0f;
    comboStep = 0;
    prevReaperState = ReaperState::NORMAL;
}

void Reaper::HandleSkills(Enemy& boss) {
    float dt = GetFrameTime();
    // --- Detectar señal de Ult (ultSeqPhase == -1 → ActivateUltimate) ---
    if (ultSeqPhase == -1) {
        ActivateUltimate(boss.position);
        // Efecto de entrada
        for (int si = 0; si < 20; si++) {
            Graphics::VFXSystem::GetInstance().SpawnParticle(position,
                { (float)GetRandomValue(-350,350), (float)GetRandomValue(-350,350) },
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
    for (auto& p : activeProjectiles) {
        if (p.active && p.isHoming && !boss.isDead && boss.isBleeding) {
            Vector2 toEnemy = Vector2Subtract(boss.position, p.position);
            if (Vector2Length(toEnemy) > 1.0f) {
                Vector2 desired = Vector2Normalize(toEnemy);
                p.direction.x += (desired.x - p.direction.x) * p.homingStrength * dt;
                p.direction.y += (desired.y - p.direction.y) * p.homingStrength * dt;
                float len = Vector2Length(p.direction);
                if (len > 0) p.direction = Vector2Scale(p.direction, 1.0f / len);
            }
        }
    }
}

void Reaper::CheckCollisions(Enemy& boss) {
    if (boss.isDead) return;

    // --- Combo (3 hits) ---
    if (CheckComboCollision(boss)) {
        float dmg = combo[comboStep].damage;
        boss.hp -= dmg;
        energy = fminf(maxEnergy, energy + 7.5f);
        
        // Hitstop y empuje (reducido 50%)
        hitstopTimer = 0.08f;
        velocity = Vector2Add(velocity, Vector2Scale(facing, 297.0f));
        
        // Solo el ultimo ataque de la secuencia empuja al boss
        if (comboStep == 2) {
            boss.velocity = Vector2Add(boss.velocity, Vector2Scale(facing, 750.0f));
            screenShake = 2.5f; 
        }
    }

    // --- Heavy Attack (Tajo frontal cargado) ---
    if (CheckHeavyCollision(boss)) {
        float dmg = 49.5f;
        boss.hp -= dmg;
        energy = fminf(maxEnergy, energy + 12.5f);
        
        hitstopTimer = 0.12f;
        velocity = Vector2Add(velocity, Vector2Scale(facing, 420.0f)); // reducido 50%
    }

    // --- Ground Bursts Q: colision hit-once con el boss ---
    for (int i = 0; i < 5; i++) {
        GroundBurst& gb = groundBursts[i];
        if (!gb.active || gb.hasDealtDamage) continue;
        Vector2 diff  = Vector2Subtract(boss.position, gb.position);
        float isoY    = diff.y * 2.0f;
        float isoDist = sqrtf(diff.x * diff.x + isoY * isoY);
        if (isoDist < gb.visualRadius + boss.radius) {
            gb.hasDealtDamage = true;
            boss.hp -= gb.damage;
            boss.ApplyBleed();
        }
    }

    // --- Orbes Homing E: colision con el boss ---
    for (auto& p : activeProjectiles) {
        if (!p.active || !p.isHoming) continue;
        if (CheckCollisionCircles(p.position, 14.0f, boss.position, boss.radius)) {
            p.active = false;
            boss.hp -= p.damage;
        }
    }

    // --- Ultimate Fase 2: Tajo Final AUTOMATICO → DoT Pop ---
    if (CheckUltFinalSlash(boss)) {
        float popDmg = boss.GetRemainingBleedDamage();
        boss.hp    -= popDmg;
        boss.bleedTimer  = 0;
        boss.isBleeding  = false;
        
        if (popDmg > 0) {
            hp = fminf(maxHp, hp + popDmg);
        }
    }
}

std::vector<AbilityInfo> Reaper::GetAbilities() const {
    std::vector<AbilityInfo> abs;
    abs.push_back({ "DASH", dashCooldown, 1.8f, 0.0f, dashCooldown <= 0, {180, 0, 255, 255} });
    abs.push_back({ "Q Sangre", qCooldown, 8.0f, 25.0f, qCooldown <= 0 && energy >= 25.0f, {220, 0, 255, 255} });
    abs.push_back({ "E Orbes", eCooldown, 12.0f, 40.0f, eCooldown <= 0 && energy >= 40.0f, {255, 60, 255, 255} });
    abs.push_back({ "R Ult", ultCooldown, 25.0f, 80.0f, ultCooldown <= 0 && energy >= 80.0f, {0, 200, 255, 255} });
    return abs;
}

std::string Reaper::GetSpecialStatus() const {
    if (state == ReaperState::LOCKED && ultSeqPhase == 1) return "<<< TIEMPO DETENIDO >>>";
    if (state == ReaperState::LOCKED && ultSeqPhase == 2) return "[ TAJO FINAL ]";
    if (qActive) return TextFormat("SANGRE [%d/5]", qBurstsSpawned);
    return "";
}

// =====================================================
// REAPER - START GROUND BURST CHAIN (Habilidad Q)
// =====================================================
void Reaper::StartGroundBurstChain() {
    qActive        = true;
    qBurstsSpawned = 0;
    qBurstTimer    = 0.0f; // El primero aparece inmediatamente
    qBurstOrigin   = position;
    qBurstDir      = facing;
    // Resetear todos
    for (int i = 0; i < 5; i++) groundBursts[i].active = false;
}

// =====================================================
// REAPER - CHECK COMBO COLLISION (3 hits pesados)
// =====================================================
bool Reaper::CheckComboCollision(Enemy& enemy) {
    if (state != ReaperState::ATTACKING || attackPhase != AttackPhase::ATTACK_ACTIVE) return false;
    if (hitCooldownTimer > 0.0f) return false;

    Vector2 diff  = Vector2Subtract(enemy.position, position);
    float isoY    = diff.y * 2.0f;
    float isoDist = sqrtf(diff.x * diff.x + isoY * isoY);
    float totalRange = combo[comboStep].range + enemy.radius;

    if (isoDist < totalRange) {
        float angleToEnemy = atan2f(isoY, diff.x) * RAD2DEG;
        float angleFacing  = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
        float angleDiff    = fabsf(fmodf(angleToEnemy - angleFacing + 540.0f, 360.0f) - 180.0f);
        float halfAngle    = combo[comboStep].angleWidth / 2.0f;

        if (combo[comboStep].angleWidth >= 359.0f || angleDiff <= halfAngle) {
            hitCooldownTimer = combo[comboStep].hitCooldown;
            hasHit = true;
            return true;
        }
    }
    return false;
}

// =====================================================
// REAPER - CHECK HEAVY COLLISION (Tajo frontal)
// =====================================================
bool Reaper::CheckHeavyCollision(Enemy& enemy) {
    if (state != ReaperState::HEAVY_ATTACK || attackPhase != AttackPhase::ATTACK_ACTIVE) return false;
    if (heavyHasHit) return false;

    Vector2 diff  = Vector2Subtract(enemy.position, position);
    float isoY    = diff.y * 2.0f;
    float isoDist = sqrtf(diff.x * diff.x + isoY * isoY);

    if (isoDist < 200.0f + enemy.radius) {
        float angleToEnemy = atan2f(isoY, diff.x) * RAD2DEG;
        float angleFacing  = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
        float angleDiff    = fabsf(fmodf(angleToEnemy - angleFacing + 540.0f, 360.0f) - 180.0f);
        if (angleDiff <= 40.0f) {
            heavyHasHit = true;
            return true;
        }
    }
    return false;
}

// =====================================================
// REAPER - CHECK ULT FINAL SLASH
// =====================================================
bool Reaper::CheckUltFinalSlash(Enemy& enemy) {
    if (state != ReaperState::LOCKED || ultSeqPhase != 2) return false;
    if (attackPhase != AttackPhase::ATTACK_ACTIVE) return false;
    if (ultFinalSlashHit) return false;

    // El tajo final es un barrido de 200° centrado en facing
    Vector2 diff  = Vector2Subtract(enemy.position, position);
    float isoY    = diff.y * 2.0f;
    float isoDist = sqrtf(diff.x * diff.x + isoY * isoY);

    if (isoDist < 253.0f + enemy.radius) {
        float angleToEnemy = atan2f(isoY, diff.x) * RAD2DEG;
        float angleFacing  = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
        float angleDiff    = fabsf(fmodf(angleToEnemy - angleFacing + 540.0f, 360.0f) - 180.0f);
        if (angleDiff <= 100.0f) { // 200° total
            ultFinalSlashHit = true;
            return true;
        }
    }
    return false;
}

// =====================================================
// REAPER - LAUNCH HOMING ORBS (Habilidad E)
// =====================================================
void Reaper::LaunchHomingOrbs(Enemy& boss) {
    float orbSpeed = 650.0f; // Aumentado un poco solamente (antes 500.0f)

    if (boss.isBleeding) {
        // Lanzamiento en cono dirigido si el boss ya sangra (5 orbes en ~56 grados total)
        for (int i = 0; i < 5; i++) {
            float spreadAngle = ((float)i - 2.0f) * 28.0f;
            float baseAngle   = atan2f(facing.y, facing.x) * RAD2DEG + spreadAngle;
            float rad         = baseAngle * DEG2RAD;
            Vector2 dir = { cosf(rad), sinf(rad) };

            Projectile orb = {};
            orb.position       = position;
            orb.startPos       = position;
            orb.direction      = dir;
            orb.maxDistance    = 1500.0f;
            orb.active         = true;
            orb.damage         = 4.5f;
            orb.isHoming       = true;
            orb.homingStrength = 3.5f;
            orb.speed          = orbSpeed;

            activeProjectiles.push_back(orb);
        }
    } else {
        // Si no hay sangrado, se dispersan en un angulo de 180 hacia delante
        const int numOrbs = 5;
        for (int i = 0; i < numOrbs; i++) {
            // De -90 a 90 grados respecto al facing
            float spreadAngle = ((float)i - 2.0f) * 45.0f; 
            float baseAngle   = atan2f(facing.y, facing.x) * RAD2DEG + spreadAngle;
            float rad         = baseAngle * DEG2RAD;
            Vector2 dir = { cosf(rad), sinf(rad) };

            Projectile orb = {};
            orb.position       = position;
            orb.startPos       = position;
            orb.direction      = dir;
            orb.maxDistance    = 1500.0f;
            orb.active         = true;
            orb.damage         = 4.5f;
            orb.isHoming       = true; 
            orb.homingStrength = 3.5f;
            orb.speed          = orbSpeed;

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
    state          = ReaperState::LOCKED;
    isTimeStopped  = true;
    ultSeqTimer    = 1.8f; // Duracion de la fase de sombras
    ultSeqPhase    = 1;
    ultFinalSlash  = false;
    ultFinalSlashHit = false;

    // Limpiar sombras previas
    for (int i = 0; i < 2; i++) ultShadows[i].active = false;

    // Calcular la X: dos sombras que flanquean al jefe y cruzan en X
    Vector2 toEnemy = Vector2Subtract(bossPos, position);
    float   dist    = Vector2Length(toEnemy);
    if (dist < 1.0f) { toEnemy = facing; dist = 1.0f; }
    Vector2 dirFwd  = Vector2Normalize(toEnemy);
    Vector2 dirPerp = { -dirFwd.y, dirFwd.x }; // perpendicular

    float offset = 350.0f; // distancia de separacion en cada eje

    // Sombra 0: esquina superior-izquierda a esquina inferior-derecha (hacia el boss)
    Vector2 corner0 = Vector2Add(Vector2Scale(dirFwd, -offset), Vector2Scale(dirPerp, offset));
    ultShadows[0].position = Vector2Add(bossPos, corner0);
    ultShadows[0].velocity = Vector2Scale(Vector2Normalize(corner0), -2800.0f);
    ultShadows[0].lifetime = 0.35f;
    ultShadows[0].active   = true;

    // Sombra 1: esquina superior-derecha a esquina inferior-izquierda
    Vector2 corner1 = Vector2Add(Vector2Scale(dirFwd, -offset), Vector2Scale(dirPerp, -offset));
    ultShadows[1].position = Vector2Add(bossPos, corner1);
    ultShadows[1].velocity = Vector2Scale(Vector2Normalize(corner1), -2800.0f);
    ultShadows[1].lifetime = 0.35f;
    ultShadows[1].active   = true;

    // Orientar el facing del Reaper hacia el boss para el tajo final
    if (dist > 1.0f)
        facing = dirFwd;
}

// =====================================================
// REAPER - DRAW
// =====================================================
void Reaper::Draw() {
    float t = (float)GetTime();

    // --- Ground Bursts (Q) en el suelo ---
    for (int i = 0; i < 5; i++) groundBursts[i].Draw();

    // --- Sombras de la Ultimate ---
    for (int i = 0; i < 2; i++) {
        if (!ultShadows[i].active) continue;
        float alpha = ultShadows[i].lifetime * 3.0f;
        if (alpha > 1.0f) alpha = 1.0f;
        DrawCircleV(ultShadows[i].position, 24.0f, Fade({0, 200, 255, 255}, alpha));
        DrawCircleLines((int)ultShadows[i].position.x, (int)ultShadows[i].position.y,
                        28.0f, Fade(WHITE, alpha * 0.8f));
        // Estela
        DrawCircleV(Vector2Add(ultShadows[i].position,
                               Vector2Scale(Vector2Normalize(ultShadows[i].velocity), -35.0f)),
                    16.0f, Fade({0, 150, 255, 255}, alpha * 0.4f));
    }

    // --- Sombra isometrica del Reaper ---
    DrawEllipse((int)position.x, (int)position.y, radius, radius * 0.5f, Fade(BLACK, 0.4f));

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
    if (hitFlashTimer > 0)                          reaperColor = WHITE;
    else if (ultSeqPhase == 1 || ultSeqPhase == 2) reaperColor = {0, 200, 255, 255}; // Cyan Ult
    else if (isBuffed)                              reaperColor = {230, 50, 255, 255};
    else                                            reaperColor = {160, 0, 220, 255};

    DrawCircleV({position.x, position.y - 20}, radius, reaperColor);
    DrawCircleLines((int)position.x, (int)position.y - 20, radius - 3, Fade(WHITE, 0.5f));

    // --- Visual del Combo ---
    if (state == ReaperState::ATTACKING) {
        float halfAngle  = combo[comboStep].angleWidth / 2.0f;
        float startAngle = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG - halfAngle;
        float range      = combo[comboStep].range;
        float alpha      = (attackPhase == AttackPhase::STARTUP)   ? 0.15f
                         : (attackPhase == AttackPhase::RECOVERY)  ? 0.25f : 0.55f;
        if (attackPhase == AttackPhase::STARTUP) range *= 0.5f;

        Color comboCol = isBuffed ? Color{255, 100, 0, 255} : Color{200, 0, 255, 255};

        rlPushMatrix();
            rlTranslatef(position.x, position.y, 0);
            rlScalef(1.0f, 0.5f, 1.0f);
            if (combo[comboStep].angleWidth >= 359.0f) {
                DrawCircleV({0,0}, range, Fade(comboCol, alpha));
                if (attackPhase == AttackPhase::ATTACK_ACTIVE)
                    DrawCircleLines(0, 0, range, WHITE);
            } else {
                DrawCircleSector({0,0}, range, startAngle, startAngle + combo[comboStep].angleWidth, 32, Fade(comboCol, alpha));
                if (attackPhase == AttackPhase::ATTACK_ACTIVE)
                    DrawCircleSectorLines({0,0}, range, startAngle, startAngle + combo[comboStep].angleWidth, 32, WHITE);
            }
        rlPopMatrix();
    }

    // --- Visual del Heavy Attack ---
    bool isUltSlash = (state == ReaperState::LOCKED && ultSeqPhase == 2 && ultFinalSlash);
    if (state == ReaperState::HEAVY_ATTACK || isUltSlash) {
        float slashAngle = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
        float halfW      = isUltSlash ? 100.0f : 40.0f; // Ult slash mas ancho
        float slashRange = isUltSlash ? 253.0f : 200.0f;
        float alpha      = (attackPhase == AttackPhase::ATTACK_ACTIVE) ? 0.75f : 0.22f;
        Color slashCol   = isUltSlash ? Color{0, 220, 255, 255} : Color{255, 50, 200, 255};

        rlPushMatrix();
            rlTranslatef(position.x, position.y, 0);
            rlScalef(1.0f, 0.5f, 1.0f);
            DrawCircleSector({0,0}, slashRange, slashAngle - halfW, slashAngle + halfW, 32, Fade(slashCol, alpha));
            if (attackPhase == AttackPhase::ATTACK_ACTIVE) {
                DrawCircleSectorLines({0,0}, slashRange, slashAngle - halfW, slashAngle + halfW, 32, WHITE);
                DrawCircleSectorLines({0,0}, slashRange + 5.0f, slashAngle - halfW - 2.0f, slashAngle + halfW + 2.0f, 32, Fade(WHITE, 0.35f));
            }
        rlPopMatrix();
    }

    // --- Visual de Carga (Hold Click) ---
    if (isCharging && holdTimer > 0) {
        float chargePct = fminf(holdTimer / 0.35f, 1.0f);
        DrawCircleLines((int)position.x, (int)position.y - 20,
                        radius + 8.0f + 20.0f * (1.0f - chargePct),
                        Fade({200, 0, 255, 255}, chargePct));
        DrawCircleGradient((int)position.x, (int)position.y - 20,
                           radius * 2.0f * chargePct,
                           Fade({200, 0, 255, 160}, 0.5f),
                           Fade({200, 0, 255, 255}, 0));
        if (chargePct >= 1.0f) {
            float blink = sinf(t * 30.0f);
            if (blink > 0) DrawCircleLines((int)position.x, (int)position.y - 20, radius + 12, WHITE);
        }
    }

    // --- Visual Blink ---
    if (state == ReaperState::DASHING) {
        DrawCircleGradient((int)position.x, (int)position.y - 20, radius * 4.0f,
                           Fade({200, 0, 255, 255}, 0.8f),
                           Fade({200, 0, 255, 255}, 0));
    }

    // --- Orbes homing activos ---
    for (auto& p : activeProjectiles) {
        if (!p.active) continue;
        float pRad = p.isShadow ? 8.0f : (p.isHoming ? 14.0f : 10.0f);
        Color pCol = p.isShadow ? Fade({0, 200, 255, 255}, 0.6f)
                   : (p.isHoming ? Color{255, 60, 255, 255} : Color{220, 0, 255, 255});

        for (int i = 0; i < p.trailCount; i++) {
            float ts = 1.0f - ((float)i / p.trailCount);
            DrawCircleV(p.trail[i], pRad * ts * 0.7f, Fade(pCol, 0.4f * ts));
        }
        DrawCircleV(p.position, pRad, pCol);
        DrawCircleLines((int)p.position.x, (int)p.position.y, (int)(pRad + 2), Fade(WHITE, 0.6f));
    }
}
