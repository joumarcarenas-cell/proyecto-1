// =====================================================
// Ropera.cpp  v2.0
// - Dash independiente con i-frames reales
// - Combo: Estocada / Tajo90 / Paso-atras+Rafaga-x3
// - Ataque cargado: consume dash, super estocada
// - Q: dos tajos angulo cerrado, buff velocidad si golpea
// - E: +velocidad ataque, lifesteal 20%, +3% maxHp enemigo en basicos
// - R (Garras, <60% hp): espadas detras que se disparan escalonadas al golpear
// =====================================================
#include "entities.h"
#include "rlgl.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

#include "include/graphics/VFXSystem.h"

// ─── Arena helper ────────────────────────────────────
static bool RoArena(Vector2 p, float r) {
    float dx = std::abs(p.x - 2000.0f);
    float dy = std::abs(p.y - 2000.0f);
    return (dx + 2.0f * dy) <= (1400.0f - r * 2.236f);
}

// ─── Hitbox isometrico en arco ────────────────────────
// Devuelve true si enemyPos esta dentro del arco (desde selfPos, mirando facing)
static bool IsoArc(Vector2 self, Vector2 facing, Vector2 enemyPos,
                   float enemyR, float range, float halfAngleDeg) {
    Vector2 d = Vector2Subtract(enemyPos, self);
    float iy  = d.y * 2.0f;
    float dist = sqrtf(d.x*d.x + iy*iy);
    if (dist >= range + enemyR) return false;
    if (halfAngleDeg >= 179.9f) return true;
    float a2e = atan2f(iy, d.x) * RAD2DEG;
    float af  = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
    float diff = fabsf(fmodf(a2e - af + 540.0f, 360.0f) - 180.0f);
    return diff <= halfAngleDeg;
}

// ─── Perp 2D ──────────────────────────────────────────
static Vector2 Perp2D(Vector2 v) { return {-v.y, v.x}; }

// =====================================================
// InitSwords – activa las 3 espadas y las coloca atras
// =====================================================
void Ropera::InitSwords() {
    for (int i = 0; i < 3; i++) {
        swords[i].active       = true;
        swords[i].swordState   = SwordState::BEHIND;
        swords[i].hasDealt     = false;
        swords[i].flashTimer   = 0.0f;
        swords[i].fireDelayTimer = 0.0f;
    }
}

// =====================================================
// TriggerSwords – inicia el vuelo escalonado al golpear
// =====================================================
void Ropera::TriggerSwords(Vector2 enemyPos) {
    swordTargetSnapshot = enemyPos;
    for (int i = 0; i < 3; i++) {
        if (!swords[i].active) continue;
        if (swords[i].swordState != SwordState::BEHIND) continue; // ya disparada
        swords[i].swordState     = SwordState::FIRING;
        swords[i].targetPos      = enemyPos;
        swords[i].hasDealt       = false;
        swords[i].fireDelayTimer = swords[i].fireDelay; // 0, 0.1, 0.2s de delay
    }
}

// =====================================================
// UpdateSwords – actualiza la maquina de la espada
// =====================================================
void Ropera::UpdateSwords(float dt, Enemy& boss) {
    if (!ultActive) return;

    Vector2 perpFacing = Perp2D(facing);
    float   offsets[3] = { 0.0f, -22.0f, 22.0f }; // dispersion lateral

    for (int i = 0; i < 3; i++) {
        if (!swords[i].active) continue;
        if (swords[i].flashTimer > 0) swords[i].flashTimer -= dt;

        switch (swords[i].swordState) {

        case SwordState::BEHIND: {
            // Flotar detras del jugador con pequena dispersion lateral
            Vector2 behind = Vector2Add(position,
                             Vector2Scale(Vector2Negate(facing), swordBehindDist));
            Vector2 target = Vector2Add(behind,
                             Vector2Scale(perpFacing, offsets[i]));
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
                Vector2 behind = Vector2Add(position,
                                 Vector2Scale(Vector2Negate(facing), swordBehindDist));
                swords[i].position.x += (behind.x - swords[i].position.x) * 12.0f * dt;
                swords[i].position.y += (behind.y - swords[i].position.y) * 12.0f * dt;
                break;
            }

            // Mover hacia target
            Vector2 toTarget = Vector2Subtract(swords[i].targetPos, swords[i].position);
            float dist = Vector2Length(toTarget);

            if (dist < 20.0f) {
                // Llegó al objetivo
                if (!swords[i].hasDealt && !boss.isDead) {
                    boss.hp -= swordHitDamage;
                    swords[i].hasDealt  = true;
                    swords[i].flashTimer = 0.20f;
                    // Particulas de impacto
                    for (int k = 0; k < 5; k++) {
                        Graphics::VFXSystem::GetInstance().SpawnParticle(
                            swords[i].position,
                            { (float)GetRandomValue(-200, 200), (float)GetRandomValue(-200, 200) },
                            0.3f, {255, 160, 30, 255}
                        );
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
            Vector2 behind = Vector2Add(position,
                             Vector2Scale(Vector2Negate(facing), swordBehindDist));
            Vector2 toHome = Vector2Subtract(behind, swords[i].position);
            float dist = Vector2Length(toHome);
            if (dist < 8.0f) {
                swords[i].swordState = SwordState::BEHIND;
                swords[i].hasDealt   = false;
            } else {
                Vector2 dir = Vector2Scale(toHome, 1.0f / dist);
                swords[i].position = Vector2Add(swords[i].position,
                                     Vector2Scale(dir, swordReturnSpeed * dt));
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
    float dt = GetFrameTime();

    // ── Cooldowns globales ────────────────────────────
    if (hitFlashTimer > 0) hitFlashTimer -= dt;
    UpdateDash(dt);
    if (qCooldown    > 0) qCooldown    -= dt;
    if (eCooldown    > 0) eCooldown    -= dt;
    if (ultCooldown  > 0) ultCooldown  -= dt;
    if (inputBufferTimer > 0) inputBufferTimer -= dt;

    // ── Buffs ─────────────────────────────────────────
    if (moveSpeedBuffTimer > 0) moveSpeedBuffTimer -= dt;
    if (eBuffTimer > 0) { eBuffTimer -= dt; eBuffActive = true; }
    else                { eBuffActive = false; }

    // ── Ultimate timer ────────────────────────────────
    if (ultActive) {
        ultTimer -= dt;
        if (ultTimer <= 0) { ultActive = false; ultTimer = 0; }
    }

    // ── CC ────────────────────────────────────────────
    if (stunTimer > 0) stunTimer -= dt;
    if (slowTimer > 0) slowTimer -= dt;

    // Multiplicador velocidad de ataque (E buff lo acelera, Ult también)
    float aMult = 1.0f;
    if (eBuffActive) aMult *= 0.58f;
    if (ultActive)   aMult *= 0.50f;

    // ── Estado DASHING: i-frames breves y dash fisico ──────────────
    if (state == RoperaState::DASHING) {
        dashGraceTimer -= dt;
        if (dashGraceTimer <= 0) {
            state = RoperaState::NORMAL;
            dashGraceTimer = 0;
            velocity = Vector2Scale(velocity, 0.35f); // perder impulso al terminar
        }
        // Movimiento fisico del dash (muy poca friccion)
        velocity = Vector2Scale(velocity, 0.98f);
        Vector2 np = Vector2Add(position, Vector2Scale(velocity, dt));
        if (RoArena(np, radius)) position = np;
        
        // Estela de particulas durante el dash
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
        if (stunTimer > 0) break;

        // Velocidad de movimiento
        float spd = 395.0f;
        if (moveSpeedBuffTimer > 0) spd = 590.0f;
        if (slowTimer > 0)          spd *= 0.5f;
        if (isCharging)             spd *= 0.55f;

        // Movimiento WASD
        Vector2 move = {0,0};
        if (IsKeyDown(KEY_W)) move.y -= 1;
        if (IsKeyDown(KEY_S)) move.y += 1;
        if (IsKeyDown(KEY_A)) move.x -= 1;
        if (IsKeyDown(KEY_D)) move.x += 1;
        if (Vector2Length(move) > 0) {
            move = Vector2Normalize(move);
            Vector2 np = Vector2Add(position, Vector2Scale(move, spd * dt));
            if (RoArena(np, radius)) position = np;
        }

        // Facing al mouse
        Vector2 aim = Vector2Subtract(targetAim, position);
        if (Vector2Length(aim) > 0) facing = Vector2Normalize(aim);

        // ── Dash (SPACE) – independiente, i-frames reales ──
        if (IsKeyPressed(controls.dash) && CanDash()) {
            // Direccion: WASD si se mueve, si no facing
            Vector2 bdir = (Vector2Length(move) > 0) ? Vector2Normalize(move) : facing;
            velocity = Vector2Scale(bdir, 1400.0f); // Velocidad alta para el dash fisico
            state = RoperaState::DASHING;
            dashGraceTimer = 0.15f; // Duracion un poco mayor para recorrer distancia
            UseDash();
            // Explosion de particulas al iniciar
            for (int k = 0; k < 8; k++)
                Graphics::VFXSystem::GetInstance().SpawnParticle( position,
                    {(float)GetRandomValue(-200,200),(float)GetRandomValue(-200,200)},
                    0.25f, {0,220,180,255} );
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
                if (CanDash()) UseDash();
                velocity          = Vector2Scale(facing, 1500.0f);
                state             = RoperaState::HEAVY_ATTACK;
                attackPhase       = AttackPhase::STARTUP;
                attackPhaseTimer  = 0.10f * aMult;
                heavyHasHit       = false;
            } else {
                // Click rapido: combo
                state             = RoperaState::ATTACKING;
                hasHit            = false;
                step3BackDone     = false;
                burstHitCount     = 0;
                burstSubActive    = false;
                burstSubTimer     = 0;
                attackPhase       = AttackPhase::STARTUP;
                attackPhaseTimer  = combo[comboStep].startup * aMult;
                comboTimer        = 1.4f;
            }
            holdTimer  = 0;
            isCharging = false;
        }

        // ── Habilidad Q (bloqueada en Ult) ──
        if (IsKeyPressed(controls.boomerang) && qCooldown <= 0 &&
            energy >= 20.0f && !ultActive) {
            energy      -= 20.0f;
            qCooldown    = qMaxCooldown;
            qActive      = true;
            qSlashIndex  = 0;
            qSlashActiveTimer = 0.18f * aMult; // primera ventana activa
            qSlashGapTimer    = 0.0f;
            qHasHit      = false;
            state        = RoperaState::CASTING_Q;
        }

        // ── Habilidad E ──
        if (IsKeyPressed(controls.berserker) && eCooldown <= 0 && energy >= 30.0f) {
            energy      -= 30.0f;
            eCooldown    = eMaxCooldown;
            eBuffTimer   = 6.0f;
            eBuffActive  = true;
        }

        // ── Ultimate R: solo si hp < 60% ──
        if (IsKeyPressed(controls.ultimate) && ultCooldown <= 0 &&
            !ultActive && hp / maxHp < 0.60f && energy >= 50.0f) {
            energy      -= 50.0f;
            ultActive    = true;
            ultTimer     = 8.0f;
            ultCooldown  = ultMaxCooldown;
            InitSwords();
        }
        break;
    }

    // ─── ATTACKING (Combo 3 hits) ─────────────────────
    case RoperaState::ATTACKING: {
        // Facing continuo al mouse
        Vector2 aim = Vector2Subtract(targetAim, position);
        if (Vector2Length(aim) > 0) facing = Vector2Normalize(aim);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            inputBufferTimer = 0.25f;
        }

        // ── Hit 3: paso atrás al entrar en STARTUP ──
        if (comboStep == 2 && !step3BackDone && attackPhase == AttackPhase::STARTUP) {
            velocity = Vector2Add(velocity, Vector2Scale(Vector2Negate(facing), 217.0f));
            step3BackDone = true;
        }

        attackPhaseTimer -= dt;
        if (hitCooldownTimer > 0) hitCooldownTimer -= dt;

        // ── Hit 3 burst sub-timer ──
        if (comboStep == 2 && attackPhase == AttackPhase::ATTACK_ACTIVE && burstSubActive) {
            burstSubTimer -= dt;
        }

        if (attackPhaseTimer <= 0) {
            switch (attackPhase) {
            case AttackPhase::STARTUP:
                attackPhase      = AttackPhase::ATTACK_ACTIVE;
                attackPhaseTimer = combo[comboStep].active * aMult;
                hitCooldownTimer = 0;
                attackId++;
                if (comboStep == 2) { burstSubActive = true; burstSubTimer = 0; }
                break;
            case AttackPhase::ATTACK_ACTIVE:
                attackPhase      = AttackPhase::RECOVERY;
                attackPhaseTimer = combo[comboStep].recovery * aMult;
                burstSubActive   = false;
                break;
            case AttackPhase::RECOVERY:
                comboStep      = (comboStep + 1) % 3;
                if (inputBufferTimer > 0.0f) {
                    inputBufferTimer = 0.0f;
                    attackPhase      = AttackPhase::STARTUP;
                    attackPhaseTimer = combo[comboStep].startup * aMult;
                    comboTimer       = 1.4f;
                    hasHit           = false;
                    burstHitCount    = 0;
                    burstSubActive   = false;
                    step3BackDone    = false;
                } else {
                    state          = RoperaState::NORMAL;
                    attackPhase    = AttackPhase::NONE;
                    hasHit         = false;
                    burstHitCount  = 0;
                    burstSubActive = false;
                    step3BackDone  = false;
                }
                break;
            default: break;
            }
        }

        // ── Dash cancela el combo ──
        if (IsKeyPressed(controls.dash) && CanDash()) {
            Vector2 aim2 = Vector2Subtract(targetAim, position);
            Vector2 bdir = (Vector2Length(aim2) > 0) ? Vector2Normalize(aim2) : facing;
            velocity = Vector2Scale(bdir, 1400.0f);
            state = RoperaState::DASHING;
            dashGraceTimer = 0.15f;
            UseDash();
            attackPhase = AttackPhase::NONE;
            for (int k = 0; k < 8; k++)
                Graphics::VFXSystem::GetInstance().SpawnParticle( position,
                    {(float)GetRandomValue(-200,200),(float)GetRandomValue(-200,200)},
                    0.25f, {0,220,180,255} );
        }
        break;
    }

    // ─── HEAVY_ATTACK (Super Estocada) ────────────────
    case RoperaState::HEAVY_ATTACK: {
        attackPhaseTimer -= dt;

        // Propulsion frontal durante startup
        if (attackPhase == AttackPhase::STARTUP) {
            Vector2 np = Vector2Add(position, Vector2Scale(velocity, dt));
            if (RoArena(np, radius)) position = np;
            velocity = Vector2Scale(velocity, 0.78f);
        }

        if (attackPhaseTimer <= 0) {
            switch (attackPhase) {
            case AttackPhase::STARTUP:
                attackPhase      = AttackPhase::ATTACK_ACTIVE;
                attackPhaseTimer = 0.18f;
                velocity         = {0,0};
                attackId++;
                break;
            case AttackPhase::ATTACK_ACTIVE:
                attackPhase      = AttackPhase::RECOVERY;
                attackPhaseTimer = 0.40f;
                break;
            case AttackPhase::RECOVERY:
                state       = RoperaState::NORMAL;
                attackPhase = AttackPhase::NONE;
                heavyHasHit = false;
                break;
            default: break;
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
                qSlashIndex       = 1;
                qSlashActiveTimer = 0.18f * aMult;
                qHasHit           = false;
                attackId++;
            }
        } else {
            // Fin de la habilidad Q
            qActive = false;
            state   = RoperaState::NORMAL;
        }

        // Iniciar gap si el primer tajo acaba de terminar
        if (qSlashIndex == 0 && qSlashActiveTimer <= 0 && qSlashGapTimer <= 0) {
            qSlashGapTimer = 0.10f * aMult;
        }

        // Facing al mouse durante Q
        Vector2 aim = Vector2Subtract(targetAim, position);
        if (Vector2Length(aim) > 0) facing = Vector2Normalize(aim);
        break;
    }

    case RoperaState::DASHING: break; // ya manejado arriba
    case RoperaState::CHARGING_HEAVY: break;
    case RoperaState::ULT_ACTIVE: state = RoperaState::NORMAL; break;
    } // end switch

    // ── Friccion ─────────────────────────────────────
    velocity = Vector2Scale(velocity, 0.87f);

    // ── Fisica (excepto HEAVY startup) ───────────────
    if (state != RoperaState::HEAVY_ATTACK || attackPhase != AttackPhase::STARTUP) {
        Vector2 np = Vector2Add(position, Vector2Scale(velocity, dt));
        if (RoArena(np, radius)) position = np;
        else velocity = {0,0};
    }

    // ── Animación del Sprite (LISTO PARA NUEVO SPRITE) ────────
    // currentFrameY = ... define aquí la fila de la animación actual
    // currentFrameX = (currentFrameX + 1) % MAX_FRAMES
    frameTimer += dt;
    /*
    if (frameTimer >= anim.spd) {
        frameTimer = 0;
        currentFrameX = (currentFrameX + 1) % MAX_FRAMES;
    }
    */

    // ── Combo timeout ─────────────────────────────────
    if (state != RoperaState::ATTACKING && comboTimer > 0) {
        comboTimer -= dt;
        if (comboTimer <= 0) comboStep = 0;
    }
}


// =====================================================
// RESET
// =====================================================
void Ropera::Reset(Vector2 pos) {
    position          = pos;
    hp                = maxHp;
    energy            = 100.0f;
    velocity          = {0,0};
    state             = RoperaState::NORMAL;
    attackPhase       = AttackPhase::NONE;
    comboStep         = 0; comboTimer = 0;
    hasHit            = false; heavyHasHit = false;
    dashCharges       = maxDashCharges; dashCooldown1 = 0.0f; dashCooldown2 = 0.0f; 
    dashGraceTimer = 0;
    qCooldown         = 0; eCooldown = 0; ultCooldown = 0;
    eBuffTimer        = 0; eBuffActive = false;
    moveSpeedBuffTimer = 0;
    ultActive         = false; ultTimer = 0;
    qActive           = false;
    stunTimer         = 0; slowTimer = 0;
    holdTimer         = 0; isCharging = false;
    step3BackDone     = false;
    burstHitCount     = 0; burstSubActive = false;
    for (int i = 0; i < 3; i++) {
        swords[i].active     = false;
        swords[i].swordState = SwordState::BEHIND;
        swords[i].hasDealt   = false;
    }
}

// =====================================================
// HANDLE SKILLS (update de espadas, llamado cada frame)
// =====================================================
void Ropera::HandleSkills(Enemy& boss) {
    float dt = GetFrameTime();
    UpdateSwords(dt, boss);
}

// =====================================================
// CHECK COLLISIONS
// =====================================================
void Ropera::CheckCollisions(Enemy& enemy) {
    if (enemy.isDead) return;

    float aMult = 1.0f;
    if (eBuffActive) aMult *= 0.58f;
    if (ultActive)   aMult *= 0.50f;

    // ─── Combo ────────────────────────────────────────
    if (CheckComboCollision(enemy)) {
        int step = comboStep;
        float dmg = combo[step].damage;
        if (eBuffActive) dmg += enemy.maxHp * eMaxHpBonusFrac;
        if (ultActive)   dmg *= 0.85f; // ult tradeoff

        enemy.hp -= dmg;
        energy = fminf(maxEnergy, energy + 5.0f);

        if (eBuffActive) hp = fminf(maxHp, hp + dmg * eLifestealFrac);

        // Hitstop y empuje (reducido 50%)
        hitstopTimer = 0.08f;
        velocity = Vector2Add(velocity, Vector2Scale(facing, 280.0f));

        // Solo el ultimo ataque de la secuencia empuja al boss 
        if (comboStep == 2) {
            enemy.velocity = Vector2Add(enemy.velocity, Vector2Scale(facing, 700.0f));
            screenShake = 2.5f;
        }

        // Disparar espadas escalonadas
        if (ultActive) TriggerSwords(enemy.position);
    }

    // ─── Heavy ────────────────────────────────────────
    if (CheckHeavyCollision(enemy)) {
        float dmg = 54.0f;
        if (eBuffActive) dmg += enemy.maxHp * eMaxHpBonusFrac;
        enemy.hp -= dmg;
        energy = fminf(maxEnergy, energy + 10.0f);
        if (eBuffActive) hp = fminf(maxHp, hp + dmg * eLifestealFrac);
        
        // Hitstop y empuje
        hitstopTimer = 0.12f;
        velocity = Vector2Add(velocity, Vector2Scale(facing, 385.0f)); // reducido 50%
        
        if (ultActive) TriggerSwords(enemy.position);
    }

    // ─── Q (dos tajos) ────────────────────────────────
    if (qActive && state == RoperaState::CASTING_Q && qSlashActiveTimer > 0) {
        if (CheckQCollision(enemy, qSlashIndex)) {
            float dmg = 20.0f;
            enemy.hp -= dmg;
            energy = fminf(maxEnergy, energy + 8.0f);
            qHasHit = true;
            if (eBuffActive) hp = fminf(maxHp, hp + dmg * eLifestealFrac);
            
            // Hitstop más ligero para los tajos rápidos
            hitstopTimer = 0.05f;
            velocity = Vector2Add(velocity, Vector2Scale(facing, 350.0f));
            
            // Buff de velocidad al acertar
            if (!ultActive) moveSpeedBuffTimer = 3.5f;
        }
    }
}

// =====================================================
// CHECK COMBO COLLISION
// =====================================================
bool Ropera::CheckComboCollision(Enemy& enemy) {
    if (state != RoperaState::ATTACKING || attackPhase != AttackPhase::ATTACK_ACTIVE) return false;

    // Hit 3: hasta 3 sub-hits con su propio timer (0.12s entre c/u)
    if (comboStep == 2) {
        if (burstHitCount >= 3) return false;
        if (!burstSubActive)    return false;
        // El timer de sub-hit actua como hitCooldown interno
        if (burstSubTimer > 0)  return false;
        // Rango ult reducido
        // Rango ult: 68% del rango base (antes 50%)
        float r = combo[2].range * (ultActive ? 0.68f : 1.0f);
        if (IsoArc(position, facing, enemy.position, enemy.radius, r, combo[2].angleWidth/2.0f)) {
            burstHitCount++;
            burstSubTimer = 0.12f; // proximo sub-hit en 0.12s
            return true;
        }
        return false;
    }

    // Hits 1 y 2 — rango ult: 68%
    if (hitCooldownTimer > 0) return false;
    float r = combo[comboStep].range * (ultActive ? 0.68f : 1.0f);
    float ha = combo[comboStep].angleWidth / 2.0f;
    if (IsoArc(position, facing, enemy.position, enemy.radius, r, ha)) {
        hitCooldownTimer = combo[comboStep].hitCooldown;
        hasHit = true;
        return true;
    }
    return false;
}

// =====================================================
// CHECK HEAVY COLLISION
// =====================================================
bool Ropera::CheckHeavyCollision(Enemy& enemy) {
    if (state != RoperaState::HEAVY_ATTACK || attackPhase != AttackPhase::ATTACK_ACTIVE) return false;
    if (heavyHasHit) return false;
    // Rango pesado ~18% mayor que Reaper (200 -> 236)
    if (IsoArc(position, facing, enemy.position, enemy.radius, 236.0f, 36.0f)) {
        heavyHasHit = true;
        return true;
    }
    return false;
}

// =====================================================
// CHECK Q COLLISION
// =====================================================
bool Ropera::CheckQCollision(Enemy& enemy, int slashIdx) {
    if (qHasHit) return false;
    // Tajo 1: offset -28deg; Tajo 2: offset +28deg (angulo cerrado <180)
    float offsetDeg = (slashIdx == 0) ? -28.0f : 28.0f;
    float baseAng   = atan2f(facing.y, facing.x) * RAD2DEG + offsetDeg;
    float rad       = baseAng * DEG2RAD;
    Vector2 sf = { cosf(rad), sinf(rad) };
    // Rango Q tambien con bonus 18%: 118 * 1.18 = 139px
    return IsoArc(position, sf, enemy.position, enemy.radius, 139.0f, 52.0f);
}

// =====================================================
// GET ABILITIES
// =====================================================
std::vector<AbilityInfo> Ropera::GetAbilities() const {
    std::vector<AbilityInfo> abs;
    float currentDashCD = (dashCooldown1 > 0 && dashCooldown2 > 0) ? fminf(dashCooldown1, dashCooldown2) : ((dashCooldown1 > 0) ? dashCooldown1 : dashCooldown2);
    abs.push_back({ TextFormat("DASH [%d]", dashCharges), currentDashCD, dashMaxCD,    0.0f,
                    CanDash(), {80, 200, 220, 255} });
    abs.push_back({ "Q Tajos", qCooldown,    qMaxCooldown, 20.0f,
                    qCooldown <= 0 && energy >= 20.0f && !ultActive, {0, 220, 180, 255} });
    abs.push_back({ "E Furia", eCooldown,    eMaxCooldown, 30.0f,
                    eCooldown <= 0 && energy >= 30.0f, {60, 255, 180, 255} });
    abs.push_back({ "R Garras",ultCooldown, ultMaxCooldown, 50.0f,
                    ultCooldown <= 0 && !ultActive && hp/maxHp < 0.60f && energy >= 50.0f,
                    {255, 120, 0, 255} });
    return abs;
}

// =====================================================
// GET SPECIAL STATUS
// =====================================================
std::string Ropera::GetSpecialStatus() const {
    if (ultActive)             return TextFormat("[ GARRAS %.1fs ]", ultTimer);
    if (eBuffActive)           return TextFormat("[ FURIA %.1fs ]", eBuffTimer);
    if (moveSpeedBuffTimer > 0) return "[ VEL+ ]";
    return "";
}

// =====================================================
// DRAW
// =====================================================
void Ropera::Draw() {
    float t = (float)GetTime();
    // ── Visual Combo ────────────────────────────────
    DrawEllipse((int)position.x, (int)position.y, radius, radius*0.5f, Fade(BLACK, 0.4f));

    // ── Aura ─────────────────────────────────────────
    float ap = 0.3f + 0.15f * sinf(t * 3.2f);
    Color ac = ultActive   ? Color{255,120,0,255}
             : eBuffActive ? Color{0,255,150,255}
             : moveSpeedBuffTimer > 0 ? Color{80,255,220,255}
             :               Color{0,200,160,255};
    DrawCircleGradient((int)position.x, (int)position.y-20,
                       radius*(ultActive ? 3.8f : 1.9f),
                       Fade(ac, ap*(ultActive?0.9f:0.5f)), Fade(ac,0));

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
            activeTex = ResourceManager::roperaTajoDoble;
        } else if (state == RoperaState::HEAVY_ATTACK) {
            activeTex = ResourceManager::roperaHeavy;
        } else if (state == RoperaState::ATTACKING) {
            if (comboStep == 0 || comboStep == 1) activeTex = ResourceManager::roperaAttack1;
            else activeTex = ResourceManager::roperaAttack3;
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
        Image* animSource = nullptr;

        if (state == RoperaState::CASTING_Q) {
            numFrames = ResourceManager::roperaTajoFrames;
            animSource = &ResourceManager::roperaTajoDobleIm;
        } else if (hp <= 0) {
            // Death is still PNG
            if (activeTex.height > 0) numFrames = activeTex.width / activeTex.height;
        } else if (hitFlashTimer > 0) {
            // Hit is still PNG
            if (activeTex.height > 0) numFrames = activeTex.width / activeTex.height;
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
        
        if (numFrames <= 0) numFrames = 1;

        // 3. Calcular qué frame mostrar
        int currentFrame = 0;
        
        if (state == RoperaState::NORMAL && animSource != nullptr) {
            // Loop de animacion por tiempo para Idle/Run
            float animSpeed = (activeTex.id == ResourceManager::roperaRun.id) ? 0.08f : 0.12f;
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
                if (attackPhase == AttackPhase::STARTUP) elapsed = st - attackPhaseTimer;
                else if (attackPhase == AttackPhase::ATTACK_ACTIVE) elapsed = st + (ac - attackPhaseTimer);
                else if (attackPhase == AttackPhase::RECOVERY) elapsed = st + ac + (rc - attackPhaseTimer);
                
                progress = elapsed / tot;
            } else if (state == RoperaState::HEAVY_ATTACK) {
                float tot = 0.10f + 0.18f + 0.40f;
                float elapsed = 0.0f;
                if (attackPhase == AttackPhase::STARTUP) elapsed = 0.10f - attackPhaseTimer;
                else if (attackPhase == AttackPhase::ATTACK_ACTIVE) elapsed = 0.10f + (0.18f - attackPhaseTimer);
                else if (attackPhase == AttackPhase::RECOVERY) elapsed = 0.10f + 0.18f + (0.40f - attackPhaseTimer);
                progress = elapsed / tot;
            } else if (state == RoperaState::DASHING) {
                progress = 1.0f - (dashGraceTimer / 0.15f);
            } else if (state == RoperaState::CASTING_Q) {
                if (qSlashIndex == 0) progress = 1.0f - (qSlashActiveTimer / 0.18f) * 0.5f;
                else progress = 0.5f + (1.0f - (qSlashActiveTimer / 0.18f)) * 0.5f;
            }
            
            progress = std::clamp(progress, 0.0f, 0.99f);
            currentFrame = (int)(progress * numFrames);
        }

        // --- ACTUALIZACION TEXTURA DESDE GIF ---
        if (animSource != nullptr && numFrames > 0) {
            unsigned int nextFrameDataOffset = animSource->width * animSource->height * 4 * currentFrame;
            UpdateTexture(activeTex, ((unsigned char *)animSource->data) + nextFrameDataOffset);
            currentFrame = 0; 
            numFrames = 1;    
        }

        // 4. Dibujar textura
        if (activeTex.id != 0) {
            float frameW = (float)activeTex.width / numFrames;
            float frameH = (float)activeTex.height;
            
            // Voltear horizontalmente segun el facing (asumiendo que miran a la derecha por defecto)
            float flipMult = (facing.x < 0) ? -1.0f : 1.0f;
            
            Rectangle src = { (float)currentFrame * frameW, 0.0f, frameW * flipMult, frameH };
            
            // Origen centrado en la base
            float scale = 1.5f; // Ajusta esto segun el tamano deseado de tu personaje
            Rectangle dest = { position.x, position.y, frameW * scale, frameH * scale };
            Vector2 orig = { (frameW * scale) / 2.0f, (frameH * scale) - 10.0f }; // -10px de offset para encajar pies en centro
            
            Color tint = WHITE;
            if (hitFlashTimer > 0) tint = {255, 100, 100, 255};
            else if (ultActive)    tint = {255, 220, 100, 255};
            else if (eBuffActive)  tint = {180, 255, 200, 255};
            
            DrawTexturePro(activeTex, src, dest, orig, 0.0f, tint);
            
        } else {
            // Fallback primitivo si falla carga
            Color tint = WHITE;
            if (hitFlashTimer > 0) tint = {255, 100, 100, 255};
            DrawCircleV({position.x, position.y - 20}, radius, tint);
        }
    }


    // ── Visual Combo ──────────────────────────────────
    if (state == RoperaState::ATTACKING) {
        int step = comboStep;
        float range = combo[step].range * (ultActive ? 0.68f : 1.0f);
        float ha    = combo[step].angleWidth / 2.0f;
        float sa    = atan2f(facing.y*2.0f, facing.x)*RAD2DEG - ha;
        float alpha = (attackPhase==AttackPhase::STARTUP)  ? 0.15f
                    : (attackPhase==AttackPhase::RECOVERY) ? 0.20f : 0.65f;
        if (attackPhase==AttackPhase::STARTUP) range *= 0.5f;
        Color sc = ultActive ? Color{255,160,0,255} : Color{0,220,180,255};
        rlPushMatrix();
            rlTranslatef(position.x, position.y, 0);
            rlScalef(1.0f, 0.5f, 1.0f);
            DrawCircleSector({0,0}, range, sa, sa+combo[step].angleWidth, 24, Fade(sc,alpha));
            if (attackPhase==AttackPhase::ATTACK_ACTIVE)
                DrawCircleSectorLines({0,0}, range, sa, sa+combo[step].angleWidth, 24, WHITE);
        rlPopMatrix();
    }

    // ── Visual Heavy ──────────────────────────────────
    if (state == RoperaState::HEAVY_ATTACK) {
        float sa  = atan2f(facing.y*2.0f, facing.x)*RAD2DEG;
        float alp = (attackPhase==AttackPhase::ATTACK_ACTIVE) ? 0.78f : 0.25f;
        rlPushMatrix();
            rlTranslatef(position.x, position.y, 0);
            rlScalef(1.0f, 0.5f, 1.0f);
            DrawCircleSector({0,0}, 236.0f, sa-36.0f, sa+36.0f, 24, Fade({0,255,200,255},alp));
            if (attackPhase==AttackPhase::ATTACK_ACTIVE) {
                DrawCircleSectorLines({0,0}, 236.0f, sa-36.0f, sa+36.0f, 24, WHITE);
                DrawCircleSectorLines({0,0}, 242.0f, sa-38.0f, sa+38.0f, 24, Fade(WHITE,0.3f));
            }
        rlPopMatrix();
    }

    // ── Visual Q ──────────────────────────────────────
    if (qActive && state == RoperaState::CASTING_Q && qSlashActiveTimer > 0) {
        float off = (qSlashIndex == 0) ? -28.0f : 28.0f;
        float ba  = atan2f(facing.y*2.0f, facing.x)*RAD2DEG + off;
        rlPushMatrix();
            rlTranslatef(position.x, position.y, 0);
            rlScalef(1.0f, 0.5f, 1.0f);
            DrawCircleSector({0,0}, 139.0f, ba-52.0f, ba+52.0f, 16, Fade({100,255,200,255},0.55f));
            DrawCircleSectorLines({0,0}, 139.0f, ba-52.0f, ba+52.0f, 16, Fade(WHITE,0.90f));
        rlPopMatrix();
    }

    // ── Visual Carga ──────────────────────────────────
    if (isCharging && holdTimer > 0) {
        float cp = fminf(holdTimer/0.35f, 1.0f);
        DrawCircleLines((int)position.x,(int)position.y-20,
                        radius+8.0f+20.0f*(1.0f-cp), Fade({0,220,180,255},cp));
        if (cp >= 1.0f) {
            Color rc = CanDash() ? Color{0,255,180,255} : Color{255,200,0,255};
            if (sinf(t*30.0f)>0) DrawCircleLines((int)position.x,(int)position.y-20,radius+14,rc);
        }
    }

    // ── Visual Dash ───────────────────────────────────
    if (state == RoperaState::DASHING) {
        DrawCircleGradient((int)position.x,(int)position.y-20,radius*4.0f,
                           Fade({0,220,180,255},0.75f), Fade({0,220,180,255},0));
    }

    // ── Buff velocidad anillo ─────────────────────────
    if (moveSpeedBuffTimer > 0) {
        float pulse = 0.5f + 0.4f*sinf(t*6.0f);
        rlPushMatrix();
            rlTranslatef(position.x, position.y, 0);
            rlScalef(1.0f, 0.5f, 1.0f);
            DrawCircleLines(0, 0, radius+15.0f, Fade({0,255,200,255},pulse));
        rlPopMatrix();
    }

    // ── Espadas Voladoras ─────────────────────────────
    if (ultActive) {
        for (int i = 0; i < 3; i++) {
            if (!swords[i].active) continue;
            Vector2 sp = swords[i].position;

            bool firing = (swords[i].swordState == SwordState::FIRING &&
                           swords[i].fireDelayTimer <= 0);
            bool flash  = (swords[i].flashTimer > 0);

            Color sc_col = flash    ? Color{255, 80, 0, 255}
                         : firing   ? Color{255,200,60,255}
                         :            Color{220,230,255,255};

            // Dibuja espada como rectangulo rotado + punta
            float angle = atan2f(sp.y - position.y, sp.x - position.x) * RAD2DEG;
            // Blade
            DrawRectanglePro({sp.x, sp.y, 32.0f, 5.0f}, {16.0f, 2.5f}, angle, sc_col);
            // Guard
            DrawRectanglePro({sp.x, sp.y, 5.0f, 14.0f}, {2.5f, 7.0f},
                             angle + 90.0f, Fade(LIGHTGRAY, 0.8f));
            // Glow
            if (flash) {
                DrawCircleGradient((int)sp.x,(int)sp.y, 22.0f,
                                   Fade({255,120,0,255},0.75f),
                                   Fade({255,60,0,255},0));
            }

            // Estela durante el vuelo
            if (firing) {
                Vector2 tail = Vector2Subtract(sp, Vector2Scale(
                    Vector2Normalize(Vector2Subtract(swords[i].targetPos, sp)), 20.0f));
                DrawLineEx(sp, tail, 3.0f, Fade({255,200,60,255},0.5f));
            }
        }
    }
}
