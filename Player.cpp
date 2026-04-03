#include "entities.h"
#include "rlgl.h"
#include <cmath>

void Player::Update() {
    float dt = GetFrameTime();

    // Sistema de Combo Rápido (Basado en Fases)
    float speedMult = isBuffed ? 1.3f : 1.0f;

    if (dashCooldown > 0) dashCooldown -= dt;
    
    if (boomerangCooldown > 0) boomerangCooldown -= dt;
    if (ultimateCooldown > 0) ultimateCooldown -= dt;
    
    // Lógica de Buff (Berserker)
    if (buffTimer > 0) {
        buffTimer -= dt;
        isBuffed = true;
    } else {
        isBuffed = false;
    }
    
    // Lógica de Definitiva (8 segundos)
    if (ultTimer > 0) {
        ultTimer -= dt;
        isUltActive = true;
        if (ultTimer <= 0) {
            ultimateCooldown = 20.0f; // Cooldown empieza AL ACABAR el efecto
            isUltActive = false;
        }
    } else {
        isUltActive = false;
    }

    // Movimiento básico (Afectado por buff +30%)
    float currentSpeed = isBuffed ? 520.0f : 400.0f; 
    Vector2 move = {0, 0};
    if (IsKeyDown(KEY_W)) move.y -= 1;
    if (IsKeyDown(KEY_S)) move.y += 1;
    if (IsKeyDown(KEY_A)) move.x -= 1;
    if (IsKeyDown(KEY_D)) move.x += 1;

    if (Vector2Length(move) > 0 && !isAttacking) { // Bloquear movimiento manual durante ataques
        move = Vector2Normalize(move);
        Vector2 nextPos = Vector2Add(position, Vector2Scale(move, currentSpeed * dt));
        
        float dx = std::abs(nextPos.x - 2000.0f) / 1400.0f;
        float dy = std::abs(nextPos.y - 2000.0f) / 700.0f;
        if (dx + dy <= 1.0f) {
            position = nextPos;
        }
    }

    // Dirección y Mouse
    Vector2 aimDiff = Vector2Subtract(targetAim, position);
    if (Vector2Length(aimDiff) > 0) facing = Vector2Normalize(aimDiff);
    
    // --- LÓGICA DE ATAQUE (CLICK vs MANTANER) ---
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        if (!isSpinning && !isAttacking) {
            chargeTimer += dt;
            if (chargeTimer > 0.15f && energy > 0) { // Reducido a 0.15s para mejor respuesta
                isSpinning = true;
                spinHitCount = 0;
                spinTimer = 0; // Primer hit inmediato
            }
        }
    }
    
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        if (isSpinning) {
            isSpinning = false;
        } else if (chargeTimer < 0.15f && !isAttacking) {
            // --- GATILLO DE SUPER ESTOCADA (DASH + CLICK) ---
            if (dashCooldown > 0.5f && energy >= 15.0f && !isDashAttacking) {
                isDashAttacking = true;
                hasDashHit = false;
                energy -= 15.0f;
                dashAttackTimer = 0.3f; // Duración del rastro visual
            } else {
                // Acción de CLICK rápido (Iniciar Secuencia de Combo)
                isAttacking = true;
                hasHit = false;
                attackPhase = AttackPhase::STARTUP;
                attackPhaseTimer = combo[comboStep].startup / speedMult;
                comboTimer = 0.8f;
            }
        }
        chargeTimer = 0.0f;
    }

    if (isSpinning) {
        spinAngle += 1200.0f * dt; 
        spinTimer -= dt;
        if (energy <= 0) isSpinning = false;
    }

    // Habilidades de Energía
    if (IsKeyPressed(controls.boomerang)) {
        if (ultCharges > 0) {
            LaunchBoomerang(ultCharges == 1);
            ultCharges--;
            if (ultCharges == 0) isUltPending = true;
        } else if (boomerangCooldown <= 0 && energy >= 35.0f) {
            LaunchBoomerang();
            energy -= 35.0f;
            boomerangCooldown = 3.0f; 
        }
    }
    if (IsKeyPressed(controls.berserker) && energy >= 70.0f) {
        buffTimer = 4.0f;
        energy -= 70.0f;
    }
    if (IsKeyPressed(controls.ultimate) && energy >= 100.0f) {
        ActivateUltimate();
        energy = 0;
    }

    // Lógica de Bumeranes Activos (Múltiples)
    for (int i = (int)activeBoomerangs.size() - 1; i >= 0; i--) {
        Projectile& b = activeBoomerangs[i];
        if (!b.active) {
            activeBoomerangs.erase(activeBoomerangs.begin() + i);
            continue;
        }

        if (b.isOrbital) {
            // Órbita Elíptica Isométrica
            float timeAngle = (float)GetTime() * 4.0f + b.orbitAngle;
            b.position.x = position.x + cosf(timeAngle) * 160.0f;
            b.position.y = position.y + sinf(timeAngle) * 80.0f;
            
            if (ultTimer <= 0 && ultCharges <= 0 && !isUltPending) b.active = false;
        } else {
            // Lógica normal de Ida y Vuelta
            float bSpeed = 450.0f * dt; 
            if (!b.returning) {
                b.position = Vector2Add(b.position, Vector2Scale(b.direction, bSpeed));
                if (Vector2Distance(b.startPos, b.position) >= b.maxDistance) 
                    b.returning = true;
            } else {
                Vector2 toPlayer = Vector2Normalize(Vector2Subtract(position, b.position));
                b.position = Vector2Add(b.position, Vector2Scale(toPlayer, bSpeed));
                if (Vector2Distance(position, b.position) < 30.0f) {
                    b.active = false;
                    // SI ES EL ÚLTIMO DE LA DEFINITIVA, ACTIVAR ÓRBITA AQUÍ
                    if (b.isLastUltCharge && isUltPending) {
                        isUltPending = false;
                        ultTimer = 8.0f;
                        for (int i = 0; i < 3; i++) {
                            // Struct: {pos, start, dir, maxDist, returning, active, damage, isOrbital, angle, isLast}
                            Projectile orb = { position, position, {0,0}, 0.0f, false, true, 3.0f, true, (float)i * 2.094f, false };
                            activeBoomerangs.push_back(orb);
                        }
                    }
                }
            }
        }
    }

    // Dash
    if (IsKeyPressed(controls.dash) && dashCooldown <= 0) {
        Vector2 dashDir = (Vector2Length(move) > 0) ? move : facing;
        dashStartPos = position; // Guardar inicio para la estocada
        velocity = Vector2Add(velocity, Vector2Scale(dashDir, 1200.0f));
        dashCooldown = 0.7f; 
    }

    if (isDashAttacking) {
        dashAttackTimer -= dt;
        if (dashAttackTimer <= 0) isDashAttacking = false;
    }

    // Físicas y Fricción
    Vector2 nextPhysPos = Vector2Add(position, Vector2Scale(velocity, dt));
    
    // --- RESTRICCIÓN DE ARENA (SISTEMA DE SEGURIDAD PARA DASH/PHYSICS) ---
    float dxP = std::abs(nextPhysPos.x - 2000.0f) / 1400.0f;
    float dyP = std::abs(nextPhysPos.y - 2000.0f) / 700.0f;
    if (dxP + dyP <= 1.0f) {
        position = nextPhysPos;
    } else {
        // Si el dash nos saca, frenar en seco y quedar pegado al borde interno
        velocity = {0, 0};
    }

    velocity = Vector2Scale(velocity, 0.85f);

    if (isAttacking) {
        attackPhaseTimer -= dt;
        if (attackPhaseTimer <= 0) {
            switch (attackPhase) {
                case AttackPhase::STARTUP:
                    attackPhase = AttackPhase::ATTACK_ACTIVE;
                    attackPhaseTimer = combo[comboStep].active / speedMult;
                    
                    // IMPULSO HACIA ADELANTE (Lunge) - Pequeño salto al atacar
                    velocity = Vector2Add(velocity, Vector2Scale(facing, 250.0f)); 
                    break;
                case AttackPhase::ATTACK_ACTIVE:
                    attackPhase = AttackPhase::RECOVERY;
                    attackPhaseTimer = combo[comboStep].recovery / speedMult;
                    break;
                case AttackPhase::RECOVERY:
                    isAttacking = false;
                    attackPhase = AttackPhase::NONE;
                    comboStep = (comboStep + 1) % 4;
                    break;
            }
        }
    } else if (comboTimer > 0) {
        comboTimer -= dt;
        if (comboTimer <= 0) comboStep = 0;
    }
}

void Player::LaunchBoomerang(bool isLast) {
    // {pos, start, dir, maxDist, returning, active, damage, isOrbital, angle, isLast}
    Projectile b = { position, position, facing, 280.0f, false, true, 1.5f, false, 0.0f, isLast }; // Rango reducido a 280
    activeBoomerangs.push_back(b);
}

void Player::ActivateUltimate() {
    ultCharges = 3; 
    isUltActive = false;
    isUltPending = false;
    ultTimer = 0;
    // El cooldown ahora se activa en el Update al terminar ultTimer
}

bool Player::CheckAttackCollision(Enemy& enemy) {
    if (attackPhase != AttackPhase::ATTACK_ACTIVE) return false;

    Vector2 diff = Vector2Subtract(enemy.position, position);
    float dist = Vector2Length(diff);
    if (dist < combo[comboStep].range + enemy.radius) {
        float angleToEnemy = atan2f(diff.y, diff.x) * RAD2DEG;
        float angleFacing = atan2f(facing.y, facing.x) * RAD2DEG;
        float angleDiff = fabsf(fmodf(angleToEnemy - angleFacing + 540, 360) - 180);
        return angleDiff <= combo[comboStep].angleWidth / 2;
    }
    return false;
}

bool Player::CheckDashCollision(Enemy& enemy) {
    if (!isDashAttacking || hasDashHit) return false;

    // Colisión de cápsula (segmento entre dashStartPos y position actual)
    Vector2 closest = Vector2Clamp(enemy.position, dashStartPos, position);
    float dist = Vector2Distance(enemy.position, closest);
    
    // Ancho de la estocada un poco más grueso que el final del combo (ej: 40)
    if (dist < 40.0f + enemy.radius) {
        hasDashHit = true;
        return true;
    }
    return false;
}

void Player::Draw() {
    DrawEllipse((int)position.x, (int)position.y, radius, radius * 0.5f, Fade(BLACK, 0.4f));

    // --- RASTRO DE SUPER ESTOCADA ---
    if (isDashAttacking) {
        float thickness = 40.0f;
        DrawLineEx(dashStartPos, position, thickness, Fade(GOLD, dashAttackTimer * 2.0f));
        DrawCircleV(dashStartPos, thickness / 2, Fade(GOLD, dashAttackTimer * 2.0f));
        DrawCircleV(position, thickness / 2, Fade(GOLD, dashAttackTimer * 2.0f));
    }
    // Aura de Buff / Ulti
    if (isUltActive) {
        DrawCircleGradient((int)position.x, (int)position.y - 20, radius * 3, Fade(RED, 0.5f), Fade(RED, 0));
    } else if (isBuffed) {
        DrawCircleGradient((int)position.x, (int)position.y - 20, radius * 2, Fade(GOLD, 0.3f), Fade(GOLD, 0));
    }
    
    // Dibujo con Sprite o Círculo
    if (hasTexture) {
        Rectangle source = { 0, 0, (float)texture.width, (float)texture.height };
        Rectangle dest = { position.x, position.y - 20, radius * 2, radius * 2 };
        Vector2 origin = { radius, radius };
        DrawTexturePro(texture, source, dest, origin, 0.0f, WHITE);
    } else {
        DrawCircleV({position.x, position.y - 20}, radius, isUltActive ? RED : (isBuffed ? YELLOW : color));
    }
    
    DrawHealthBar(40, 5);

    // Barra de Energía (Mana)
    DrawRectangle((int)position.x - 20, (int)position.y - radius - 60, 40, 4, DARKGRAY);
    DrawRectangle((int)position.x - 20, (int)position.y - radius - 60, (int)(40 * (energy / maxEnergy)), 4, SKYBLUE);

    if (isAttacking) {
        float startAngle = atan2f(facing.y, facing.x) * RAD2DEG - (combo[comboStep].angleWidth / 2);
        float currentTargetRange = combo[comboStep].range;
        Color attackCol = isBuffed ? ORANGE : YELLOW;
        float alpha = 0.6f;

        // Visual Diferenciado por Fase
        if (attackPhase == AttackPhase::STARTUP) {
            alpha = 0.2f; // Solo un indicador del área
            currentTargetRange *= 0.5f; // Crece desde el centro
        } else if (attackPhase == AttackPhase::RECOVERY) {
            alpha = 0.3f; // Se desvanece
        }

        rlPushMatrix();
            rlTranslatef(position.x, position.y, 0);
            rlScalef(1.0f, 0.5f, 1.0f);
            
            // Core del ataque
            DrawCircleSector({0, 0}, currentTargetRange, startAngle, startAngle + combo[comboStep].angleWidth, 32, Fade(attackCol, alpha));
            
            // Borde definido durante la fase activa
            if (attackPhase == AttackPhase::ATTACK_ACTIVE) {
                DrawCircleSectorLines({0, 0}, currentTargetRange, startAngle, startAngle + combo[comboStep].angleWidth, 32, Fade(WHITE, 0.8f));
            }
        rlPopMatrix();
    }

    for (auto& b : activeBoomerangs) {
        float bRad = b.isOrbital ? 17.0f : 21.0f; // Reducido ~15%
        DrawCircleV(b.position, bRad, b.isOrbital ? RED : WHITE);
        DrawCircleLines((int)b.position.x, (int)b.position.y, bRad + 3, SKYBLUE);
    }

    // Efecto de Giro (Spin)
    if (isSpinning) {
        rlPushMatrix();
            rlTranslatef(position.x, position.y - 20, 0);
            rlRotatef(spinAngle, 0, 0, 1);
            rlScalef(1.0f, 0.5f, 1.0f);
            
            float spinRange = 125.0f; // Estandarizado a 125 (Reducido 15%)
            DrawCircleSector({0, 0}, spinRange, 0, 360, 32, Fade(SKYBLUE, 0.2f));
            for (int i = 0; i < 4; i++) {
                DrawCircleSector({0,0}, spinRange + 5, i * 90, i * 90 + 45, 16, Fade(SKYBLUE, 0.4f));
                DrawCircleSectorLines({0,0}, spinRange + 5, i * 90, i * 90 + 45, 16, Fade(WHITE, 0.3f));
            }
        rlPopMatrix();
    }
}