#include "entities.h"
#include "rlgl.h"
#include <cmath>

void Player::Update() {
    float dt = GetFrameTime();
    float speedMult = isBuffed ? 1.3f : 1.0f;

    // --- ACTUALIZAR SPRITESHEET (ANIMACION) ---
    frameTimer += dt;
    if (frameTimer >= frameSpeed) {
        frameTimer = 0;
        currentFrameX = (currentFrameX + 1) % frameCols;
    }

    // --- SINCRONIZAR PROPIEDADES CON HUD ---
    vidaActual = hp;
    vidaMaxima = maxHp;
    estaminaActual = energy;
    estaminaMaxima = maxEnergy;

    // --- COOLDOWNS Y TIMERS PASIVOS ---
    if (dashCooldown > 0) dashCooldown -= dt;
    if (boomerangCooldown > 0) boomerangCooldown -= dt;
    if (ultimateCooldown > 0) ultimateCooldown -= dt;
    
    if (buffTimer > 0) { buffTimer -= dt; isBuffed = true; } 
    else { isBuffed = false; }
    
    berserkerActivo = isBuffed;
    boomerangDisponible = (boomerangCooldown <= 0 && ultCharges == 0);

    if (ultTimer > 0) {
        ultTimer -= dt;
        isUltActive = true;
        if (ultTimer <= 0) { ultimateCooldown = 20.0f; isUltActive = false; }
    } else { isUltActive = false; }

    // Habilidades Globales (Pueden lanzarse en casi cualquier estado)
    if (IsKeyPressed(controls.boomerang)) {
        if (ultCharges > 0) { LaunchBoomerang(ultCharges == 1); ultCharges--; if (ultCharges == 0) isUltPending = true; }
        else if (boomerangCooldown <= 0 && energy >= 35.0f) { LaunchBoomerang(); energy -= 35.0f; boomerangCooldown = 3.0f; }
    }
    if (IsKeyPressed(controls.berserker) && energy >= 70.0f) { buffTimer = 4.0f; energy -= 70.0f; }

    // --- MÁQUINA DE ESTADOS ---
    switch (state) {
        case PlayerState::NORMAL: {
            // Movimiento
            float currentSpeed = isBuffed ? 520.0f : 400.0f;
            Vector2 move = {0, 0};
            if (IsKeyDown(KEY_W)) move.y -= 1;
            if (IsKeyDown(KEY_S)) move.y += 1;
            if (IsKeyDown(KEY_A)) move.x -= 1;
            if (IsKeyDown(KEY_D)) move.x += 1;

            if (Vector2Length(move) > 0) {
                move = Vector2Normalize(move);
                Vector2 nextPos = Vector2Add(position, Vector2Scale(move, currentSpeed * dt));
                float dx = std::abs(nextPos.x - 2000.0f) / 1400.0f;
                float dy = std::abs(nextPos.y - 2000.0f) / 700.0f;
                if (dx + dy <= 1.0f) position = nextPos;
            }

            // Dirección
            Vector2 aimDiff = Vector2Subtract(targetAim, position);
            if (Vector2Length(aimDiff) > 0) facing = Vector2Normalize(aimDiff);

            // Transiciones
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                chargeTimer += dt;
                if (chargeTimer > 0.15f && energy > 0) {
                    state = PlayerState::SPINNING;
                    spinHitCount = 0;
                    spinTimer = 0;
                }
            }
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                if (chargeTimer < 0.15f) {
                    state = PlayerState::ATTACKING;
                    hasHit = false;
                    attackPhase = AttackPhase::STARTUP;
                    attackPhaseTimer = combo[comboStep].startup / speedMult;
                    comboTimer = 0.8f;
                }
                chargeTimer = 0.0f;
            }
            if (IsKeyPressed(controls.dash) && dashCooldown <= 0) {
                state = PlayerState::DASHING;
                canDashAttack = !lastDashWasAttack;
                lastDashWasAttack = false; // Por defecto es normal hasta que se pulse clic
                
                Vector2 dashDir = (Vector2Length(move) > 0) ? move : facing;
                dashStartPos = position;
                velocity = Vector2Add(velocity, Vector2Scale(dashDir, 1200.0f));
                dashCooldown = 0.7f;
            }
            break;
        }

        case PlayerState::ATTACKING: {
            // Dirección (Sigue al mouse)
            Vector2 aimDiff = Vector2Subtract(targetAim, position);
            if (Vector2Length(aimDiff) > 0) facing = Vector2Normalize(aimDiff);

            attackPhaseTimer -= dt;
            if (hitCooldownTimer > 0) hitCooldownTimer -= dt;
            if (attackPhaseTimer <= 0) {
                switch (attackPhase) {
                    case AttackPhase::STARTUP:
                        attackPhase = AttackPhase::ATTACK_ACTIVE;
                        attackPhaseTimer = combo[comboStep].active / speedMult;
                        velocity = Vector2Add(velocity, Vector2Scale(facing, 250.0f));
                        hitCooldownTimer = 0.0f; // Listo para golpear de inmediato
                        attackId++;              // Nueva ventana activa
                        break;
                    case AttackPhase::ATTACK_ACTIVE:
                        attackPhase = AttackPhase::RECOVERY;
                        attackPhaseTimer = combo[comboStep].recovery / speedMult;
                        break;
                    case AttackPhase::RECOVERY:
                        state = PlayerState::NORMAL;
                        attackPhase = AttackPhase::NONE;
                        hasHit = false;
                        comboStep = (comboStep + 1) % 4;
                        break;
                }
            }
            // Cancelar con Dash
            if (IsKeyPressed(controls.dash) && dashCooldown <= 0) {
                state = PlayerState::DASHING;
                dashStartPos = position;
                velocity = Vector2Add(velocity, Vector2Scale(facing, 1200.0f));
                dashCooldown = 0.7f;
                attackPhase = AttackPhase::NONE; // Reset de ataque
            }
            break;
        }

        case PlayerState::DASHING: {
            // Transición a Super Estocada (Sólamente si no se usó en el anterior)
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && canDashAttack && energy >= 15.0f) {
                state = PlayerState::DASH_ATTACK;
                hasDashHit = false;
                energy -= 15.0f;
                dashAttackTimer = 0.3f;
                lastDashWasAttack = true; 
                
                // BOOST DEL 60% AL DASH AL ATACAR (PULIDO V3)
                velocity = Vector2Scale(velocity, 1.60f); 
            }
            // Volver a normal cuando baja la velocidad o acaba el tiempo (Ventana extendida a 0.2)
            if (dashCooldown < 0.2f) state = PlayerState::NORMAL;
            break;
        }

        case PlayerState::DASH_ATTACK: {
            dashAttackTimer -= dt;
            if (dashAttackTimer <= 0) state = PlayerState::NORMAL;
            break;
        }

        case PlayerState::SPINNING: {
            // Movimiento Reducido durante el Giro (40% velocidad)
            float chargeSpeed = (isBuffed ? 520.0f : 400.0f) * 0.4f;
            Vector2 move = {0, 0};
            if (IsKeyDown(KEY_W)) move.y -= 1;
            if (IsKeyDown(KEY_S)) move.y += 1;
            if (IsKeyDown(KEY_A)) move.x -= 1;
            if (IsKeyDown(KEY_D)) move.x += 1;

            if (Vector2Length(move) > 0) {
                move = Vector2Normalize(move);
                Vector2 nextPos = Vector2Add(position, Vector2Scale(move, chargeSpeed * dt));
                float dx = std::abs(nextPos.x - 2000.0f) / 1400.0f;
                float dy = std::abs(nextPos.y - 2000.0f) / 700.0f;
                if (dx + dy <= 1.0f) position = nextPos;
            }

            spinAngle += 1200.0f * dt;
            spinTimer -= dt;
            if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON) || energy <= 0) {
                state = PlayerState::NORMAL;
                chargeTimer = 0;
            }
            // Cancelar con Dash
            if (IsKeyPressed(controls.dash) && dashCooldown <= 0) {
                state = PlayerState::DASHING;
                canDashAttack = !lastDashWasAttack;
                lastDashWasAttack = false;
                dashStartPos = position;
                velocity = Vector2Add(velocity, Vector2Scale(facing, 1200.0f));
                dashCooldown = 0.7f;
            }
            break;
        }
    }

    // --- PROYECTILES Y FÍSICAS COMUNES ---
    for (int i = (int)activeBoomerangs.size() - 1; i >= 0; i--) {
        Projectile& b = activeBoomerangs[i];
        if (!b.active) { activeBoomerangs.erase(activeBoomerangs.begin() + i); continue; }
        for (int j = 7; j > 0; j--) b.trail[j] = b.trail[j - 1];
        b.trail[0] = b.position;
        if (b.trailCount < 8) b.trailCount++;

        if (b.isOrbital) {
            b.orbitAngle += 360.0f * dt;
            b.position.x = position.x + cosf(b.orbitAngle * DEG2RAD) * 120.0f;
            b.position.y = position.y + sinf(b.orbitAngle * DEG2RAD) * 120.0f;
            if (ultTimer <= 0 && ultCharges <= 0 && !isUltPending) b.active = false;
        } else {
            float bSpeed = 1000.0f * dt;
            if (!b.returning) {
                b.position = Vector2Add(b.position, Vector2Scale(b.direction, bSpeed));
                if (Vector2Distance(b.startPos, b.position) >= b.maxDistance) b.returning = true;
            } else {
                Vector2 toPlayer = Vector2Normalize(Vector2Subtract(position, b.position));
                b.position = Vector2Add(b.position, Vector2Scale(toPlayer, bSpeed));
                if (Vector2Distance(position, b.position) < 30.0f) {
                    b.active = false;
                    if (b.isLastUltCharge && isUltPending) {
                        isUltPending = false;
                        ultTimer = 8.0f;
                        for (int j = 0; j < 3; j++) {
                            Projectile orb = { position, position, {0,0}, 0.0f, false, true, 3.0f, true, (float)j * 120.0f, false };
                            activeBoomerangs.push_back(orb);
                        }
                    }
                }
            }
        }
    }

    Vector2 nextPhysPos = Vector2Add(position, Vector2Scale(velocity, dt));
    float dxP = std::abs(nextPhysPos.x - 2000.0f) / 1400.0f;
    float dyP = std::abs(nextPhysPos.y - 2000.0f) / 700.0f;
    if (dxP + dyP <= 1.0f) position = nextPhysPos;
    else velocity = {0, 0};
    
    // --- FRICCIÓN CONDICIONAL (DESPLAZAMIENTO SUPER ESTOCADA) ---
    float friction = (state == PlayerState::DASH_ATTACK) ? 0.93f : 0.85f;
    velocity = Vector2Scale(velocity, friction);

    if (state != PlayerState::ATTACKING && comboTimer > 0) {
        comboTimer -= dt;
        if (comboTimer <= 0) comboStep = 0;
    }
}

void Player::LaunchBoomerang(bool isLast) {
    Projectile b = { position, position, facing, 280.0f, false, true, 1.5f, false, 0.0f, isLast };
    activeBoomerangs.push_back(b);
}

void Player::ActivateUltimate() {
    ultCharges = 3; isUltActive = false; isUltPending = false; ultTimer = 0;
}

bool Player::CheckAttackCollision(Enemy& enemy) {
    if (state != PlayerState::ATTACKING || attackPhase != AttackPhase::ATTACK_ACTIVE) return false;
    if (hitCooldownTimer > 0.0f) return false; // Barrido: esperar entre golpes consecutivos

    // --- DISTANCIA CORREGIDA PARA ESPACIO ISOMÉTRICO ---
    // El visuals aplasta Y x0.5, así que la hitbox debe compensar: multiplicar Y por 2.0
    Vector2 diff = Vector2Subtract(enemy.position, position);
    float isoY = diff.y * 2.0f;           // desaplastar coordenada isométrica
    float isoDist = sqrtf(diff.x * diff.x + isoY * isoY);

    float totalRange = combo[comboStep].range + enemy.radius;
    if (isoDist < totalRange) {
        // --- ÁNGULO EN ESPACIO ISOMÉTRICO CORREGIDO ---
        float angleToEnemy = atan2f(isoY, diff.x) * RAD2DEG;
        float angleFacing  = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
        float angleDiff    = fabsf(fmodf(angleToEnemy - angleFacing + 540.0f, 360.0f) - 180.0f);

        if (angleDiff <= combo[comboStep].angleWidth / 2.0f) {
            hitCooldownTimer = combo[comboStep].hitCooldown; // Armar el CD para el siguiente golpe del barrido
            hasHit = true;
            return true;
        }
    }
    return false;
}

bool Player::CheckDashCollision(Enemy& enemy) {
    if (state != PlayerState::DASH_ATTACK || hasDashHit) return false;

    // --- HITBOX DE LANZA / PUNTA DE FLECHA ---
    Vector2 diff = Vector2Subtract(enemy.position, position);
    float dist = Vector2Length(diff);
    
    // Rango Largo (180) y Estrecho (35 grados)
    float attackRange = 180.0f; 
    float angleWidth = 35.0f;

    if (dist < attackRange + enemy.radius) {
        float angleToEnemy = atan2f(diff.y, diff.x) * RAD2DEG;
        float angleFacing = atan2f(facing.y, facing.x) * RAD2DEG;
        float angleDiff = fabsf(fmodf(angleToEnemy - angleFacing + 540, 360) - 180);
        
        if (angleDiff <= angleWidth / 2) {
            hasDashHit = true; 
            return true;
        }
    }
    return false;
}

void Player::Draw() {
    DrawEllipse((int)position.x, (int)position.y, radius, radius * 0.5f, Fade(BLACK, 0.4f));

    if (state == PlayerState::DASH_ATTACK) {
        // --- VISUAL DE LANZA DORADA (PENETRANTE) ---
        float startAngle = atan2f(facing.y, facing.x) * RAD2DEG - 17.5f; // 35 / 2
        float currentRange = 180.0f;
        
        rlPushMatrix();
            rlTranslatef(position.x, position.y, 0);
            rlScalef(1.0f, 0.5f, 1.0f);
            
            // Núcleo de la lanza
            DrawCircleSector({0, 0}, currentRange, startAngle, startAngle + 35.0f, 16, Fade(GOLD, 0.6f));
            // Bordes brillantes
            DrawCircleSectorLines({0, 0}, currentRange, startAngle, startAngle + 35.0f, 16, WHITE);
            
            // Líneas de velocidad adicionales
            for (int i = 0; i < 3; i++) {
                DrawLineEx({0,0}, {cosf((startAngle + 17.5f) * DEG2RAD) * (currentRange + 20), sinf((startAngle + 17.5f) * DEG2RAD) * (currentRange + 20)}, 2.0f, Fade(WHITE, 0.4f));
            }
        rlPopMatrix();
    }

    if (isUltActive) DrawCircleGradient((int)position.x, (int)position.y - 20, radius * 3, Fade(RED, 0.5f), Fade(RED, 0));
    else if (isBuffed) DrawCircleGradient((int)position.x, (int)position.y - 20, radius * 2, Fade(GOLD, 0.3f), Fade(GOLD, 0));
    
    Color playerColor = isUltActive ? RED : (isBuffed ? YELLOW : color);
    if (ResourceManager::texPlayer.id != 0) {
        // --- SOPORTE SPRITESHEET JUGADOR ---
        float frameWidth = (float)ResourceManager::texPlayer.width / frameCols;
        float frameHeight = (float)ResourceManager::texPlayer.height / frameRows;
        Rectangle sourceRec = {
            currentFrameX * frameWidth,
            currentFrameY * frameHeight,
            frameWidth * (facing.x < 0 ? -1.0f : 1.0f), // Flip sprite si mira a la izq
            frameHeight
        };
        DrawTexturePro(ResourceManager::texPlayer, sourceRec, 
            { position.x, position.y - 20, radius * 2, radius * 2 }, 
            { radius, radius }, 0.0f, WHITE);
    } else {
        DrawCircleV({position.x, position.y - 20}, radius, playerColor);
    }
    
    if (state == PlayerState::ATTACKING) {
        float startAngle = atan2f(facing.y, facing.x) * RAD2DEG - (combo[comboStep].angleWidth / 2);
        float currentTargetRange = combo[comboStep].range;
        Color attackCol = isBuffed ? ORANGE : YELLOW;
        float alpha = (attackPhase == AttackPhase::STARTUP) ? 0.2f : ((attackPhase == AttackPhase::RECOVERY) ? 0.3f : 0.6f);
        if (attackPhase == AttackPhase::STARTUP) currentTargetRange *= 0.5f;

        rlPushMatrix();
            rlTranslatef(position.x, position.y, 0);
            rlScalef(1.0f, 0.5f, 1.0f);
            DrawCircleSector({0, 0}, currentTargetRange, startAngle, startAngle + combo[comboStep].angleWidth, 32, Fade(attackCol, alpha));
            if (attackPhase == AttackPhase::ATTACK_ACTIVE) {
                DrawCircleSectorLines({0, 0}, currentTargetRange, startAngle, startAngle + combo[comboStep].angleWidth, 32, WHITE);
                DrawCircleSectorLines({0, 0}, currentTargetRange + 2, startAngle - 1, startAngle + combo[comboStep].angleWidth + 1, 32, Fade(WHITE, 0.5f));
            }
        rlPopMatrix();
    }

    for (auto& b : activeBoomerangs) {
        float bRad = b.isOrbital ? 17.0f : 21.0f;
        for (int i = 0; i < b.trailCount; i++) {
            float trailScale = 1.0f - ((float)i / b.trailCount);
            DrawCircleV(b.trail[i], bRad * trailScale, Fade(b.isOrbital ? RED : SKYBLUE, 0.5f * trailScale));
        }
        DrawCircleV(b.position, bRad, b.isOrbital ? RED : WHITE);
        DrawCircleLines((int)b.position.x, (int)b.position.y, bRad + 3, SKYBLUE);
    }

    if (state == PlayerState::SPINNING) {
        rlPushMatrix();
            rlTranslatef(position.x, position.y - 20, 0);
            rlRotatef(spinAngle, 0, 0, 1);
            rlScalef(1.0f, 0.5f, 1.0f);
            float spinRange = 125.0f;
            DrawCircleSector({0, 0}, spinRange, 0, 360, 32, Fade(SKYBLUE, 0.2f));
            for (int i = 0; i < 4; i++) {
                DrawCircleSector({0,0}, spinRange + 5, i * 90, i * 90 + 45, 16, Fade(SKYBLUE, 0.4f));
                DrawCircleSectorLines({0,0}, spinRange + 5, i * 90, i * 90 + 45, 16, Fade(WHITE, 0.3f));
            }
        rlPopMatrix();
    }
}