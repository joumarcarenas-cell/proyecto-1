#include "include/EliteEnemies.h"
#include "include/graphics/VFXSystem.h"
#include "include/CombatUtils.h"
#include <raymath.h>
#include "rlgl.h"

extern float g_timeScale;
extern float screenShake;

// =====================================================================
// SimpleKnight Implementation
// =====================================================================
SimpleKnight::SimpleKnight(Vector2 pos) {
    spawnPos = pos;
    position = pos;
    maxHp = 400.0f;
    hp = maxHp;
    radius = 21.0f;
    color = { 100, 100, 150, 255 };
}

void SimpleKnight::UpdateAI(Player& player) {
    if (isDead || isDying) return;
    float dt = GetFrameTime() * g_timeScale;

    // Berserker Check
    if (!isBerserker && hp <= maxHp * 0.5f) {
        isBerserker = true;
        color = { 255, 100, 100, 255 }; // Tinte rojo suave
    }

    float moveSpeed = isBerserker ? 500.0f : 380.0f; // Mas agresivos
    float atkSpeedMult = isBerserker ? 2.0f : 1.5f; // Mas rapidos golpeando

    if (attackCooldown > 0) attackCooldown -= dt * atkSpeedMult;

    Vector2 diff = Vector2Subtract(player.position, position);
    float dist = Vector2Length(diff);
    if (dist > 0 && aiState != AIState::ATTACK_COMBO) facing = Vector2Normalize(diff);

    switch (aiState) {
    case AIState::IDLE:
        if (dist < 800.0f) aiState = AIState::CHASE;
        break;

    case AIState::CHASE:
        if (dist > 80.0f) { // Rango de melee aumentado
            velocity = Vector2Add(velocity, Vector2Scale(facing, moveSpeed * 5.0f * dt));
        } else if (attackCooldown <= 0) {
            aiState = AIState::ATTACK_COMBO;
            comboStep = 0;
            stateTimer = 0.4f;
            hasHit = false;
        }
        break;

    case AIState::ATTACK_COMBO:
        stateTimer -= dt * atkSpeedMult;
        if (!hasHit) {
            float prog = CombatUtils::GetProgress(stateTimer, 0.4f) * 1.4f;
            if (prog > 1.0f) prog = 1.0f;
            if (CombatUtils::CheckProgressiveSweep(position, facing, player.position, player.radius, 90.0f, -45.0f, 90.0f, 1.0f, prog)) {
                player.TakeDamage(10.0f, {0, 0});
                Graphics::SpawnImpactBurst(player.position, facing, color, WHITE, 5, 2);
                hasHit = true;
            }
        }
        
        if (stateTimer <= 0) {
            comboStep++;
            if (comboStep < 3) {
                stateTimer = 0.4f; // Siguiente tajo
                hasHit = false;
            } else {
                aiState = AIState::CHASE;
                attackCooldown = 1.5f;
            }
        }
        break;
    case AIState::BERSERKER_TRANSITION:
    default:
        break;
    }
}
void SimpleKnight::Update() {
    if (hp <= 0 && !isDead && !isDying) { isDying = true; deathAnimTimer = 1.0f; }
    if (isDead) return;
    if (isDying) {
        deathAnimTimer -= GetFrameTime() * g_timeScale;
        if (deathAnimTimer <= 0) isDead = true;
        return;
    }

    float dt = GetFrameTime() * g_timeScale;
    position = Arena::GetClampedPos(Vector2Add(position, Vector2Scale(velocity, dt)), radius);
    velocity = Vector2Scale(velocity, 0.85f);
}

void SimpleKnight::Draw() {
    if (isDead) return;
    Color drawColor = isDying ? Fade(color, 0.5f) : color;
    
    // Sombra
    DrawEllipse(position.x, position.y, radius, radius * 0.5f, Fade(BLACK, 0.3f));
    
    // Cuerpo
    DrawCircleV({position.x, position.y - 20}, radius, drawColor);
    
    // Arma / Telegrafo
    if (aiState == AIState::ATTACK_COMBO) {
        float ang = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
        DrawCircleSectorLines({position.x, position.y}, 90.0f, ang - 45, ang + 45, 16, Fade(WHITE, 0.95f)); // Rango visual 90.0
        DrawCircleSector({position.x, position.y}, 90.0f, ang - 45, ang + 45, 16, Fade({255, 100, 100, 255}, 0.5f)); 
    }

    // Barra de vida
    if (!isDead && !isDying) {
        float hpPct = fmaxf(0.0f, hp / maxHp);
        DrawRectangle((int)position.x - 20, (int)position.y - 45, 40, 6, BLACK);
        DrawRectangle((int)position.x - 20, (int)position.y - 45, (int)(40 * hpPct), 6, RED);
    }
}

void SimpleKnight::ScaleDifficulty(int wave) {
    maxHp *= (1.0f + wave * 0.2f);
    hp = maxHp;
}

// =====================================================================
// GreatswordElite Implementation
// =====================================================================
GreatswordElite::GreatswordElite(Vector2 pos) {
    spawnPos = pos;
    position = pos;
    maxHp = 1200.0f;
    hp = maxHp;
    radius = 32.0f;
    color = { 80, 80, 80, 255 }; // Gris oscuro
}

void GreatswordElite::UpdateAI(Player& player) {
    if (isDead || isDying) return;
    float dt = GetFrameTime() * g_timeScale;

    if (!isBerserker && hp <= maxHp * 0.5f) {
        isBerserker = true;
    }

    float cdMult = isBerserker ? 1.2f : 0.8f; // Agresividad reducida
    if (attackCooldown > 0) attackCooldown -= dt * cdMult;

    Vector2 diff = Vector2Subtract(player.position, position);
    float dist = Vector2Length(diff);
    bool isLockState = (aiState == AIState::ATTACK_COMBO || aiState == AIState::TORBELLINO || aiState == AIState::EMBESTIDA);
    if (dist > 0 && !isLockState) facing = Vector2Normalize(diff);

    switch (aiState) {
    case AIState::IDLE:
        aiState = AIState::CHASE;
        break;

    case AIState::CHASE:
        if (dist > 132.0f) { // Solo perseguir si está lejos para no "empujar"
            velocity = Vector2Add(velocity, Vector2Scale(facing, 240.0f * 5.0f * dt));
        }
        
        if (attackCooldown <= 0) {
            float r = GetRandomValue(0, 100);
            if (dist < 150.0f) {
                if (r < 40) {
                    aiState = AIState::TORBELLINO;
                    whirlwindTimer = 2.0f;
                } else {
                    aiState = AIState::ATTACK_COMBO;
                    comboStep = 0;
                    stateTimer = 0.6f; // Un poco más lento que el normal pero golpea mas fuerte
                    hasHit = false;
                }
            } else if (dist > 250.0f && r < 30) {
                aiState = AIState::EMBESTIDA;
                stateTimer = 0.5f; 
                hasHit = false;
            }
        }
        break;

    case AIState::ATTACK_COMBO: {
        stateTimer -= dt * cdMult;
        if (!hasHit) {
            float totalActive = 0.6f;
            float prog = CombatUtils::GetProgress(stateTimer, totalActive) * 1.4f;
            if (prog > 1.0f) prog = 1.0f;
            
            // Lógica de tajos progresivos direccionales:
            float startOff = -70.0f;
            float sweepDeg = 140.0f;
            float sweepDir = 1.0f; // CCW
            
            if (comboStep == 1) {
                startOff = 70.0f;
                sweepDir = -1.0f; // CW (De derecha a izquierda)
            } else if (comboStep == 2) {
                startOff = -100.0f;
                sweepDeg = 200.0f;
                sweepDir = 1.0f;
            }

            if (CombatUtils::CheckProgressiveSweep(position, facing, player.position, player.radius, 145.0f, startOff, sweepDeg, sweepDir, prog)) {
                float dmg = (comboStep == 2) ? 35.0f : 20.0f;
                Vector2 pushDir = facing;
                if (comboStep == 2) pushDir = Vector2Scale(pushDir, 1.5f);
                player.TakeDamage(dmg, Vector2Scale(pushDir, 400.0f));
                
                Graphics::SpawnImpactBurst(player.position, facing, color, WHITE, 8, 4);
                hasHit = true;
                screenShake = fmaxf(screenShake, (comboStep == 2) ? 1.8f : 1.0f);
            }
        }
        
        if (stateTimer <= 0) {
            comboStep++;
            if (comboStep < 3) {
                stateTimer = 0.6f; 
                hasHit = false;
            } else {
                aiState = AIState::CHASE;
                attackCooldown = 2.0f;
            }
        }
        break;
    }

    case AIState::TORBELLINO: {
        whirlwindTimer -= dt;
        if (whirlwindHitCooldown > 0) whirlwindHitCooldown -= dt;
        
        velocity = Vector2Scale(velocity, 0.5f);
        
        // Lógica de "Reloj": 3 rotaciones completas (1080°) en el tiempo total (2.0s)
        float totalWhirlTime = 2.0f;
        float progressArr = CombatUtils::GetProgress(whirlwindTimer, totalWhirlTime) * 1.4f;
        if (progressArr > 1.0f) progressArr = 1.0f;
        float totalDegArr = 1080.0f; // 3 vueltas completas
        
        if (whirlwindHitCooldown <= 0) {
            // CheckProgressiveSweep detectará cuando la "manecilla" del reloj (la espada) pase por el jugador
            if (CombatUtils::CheckProgressiveSweep(position, facing, player.position, player.radius, 130.0f, 0.0f, totalDegArr, 1.0f, progressArr)) {
                player.TakeDamage(18.0f, {0, 0});
                whirlwindHitCooldown = 0.25f; // Evita multihits en el mismo frame de paso
                screenShake = fmaxf(screenShake, 0.6f);
                Graphics::SpawnImpactBurst(player.position, facing, color, WHITE, 5, 2);
            }
        }

        if (whirlwindTimer <= 0) {
            aiState = AIState::CHASE;
            attackCooldown = 3.0f;
        }
        break;
    }
        
    case AIState::EMBESTIDA:
        stateTimer -= dt;
        if (stateTimer <= 0) {
            // Dash hacia el jugador (Distancia reducida)
            if (Vector2Length(velocity) < 500.0f) {
                velocity = Vector2Scale(facing, 1250.0f);
            }
            if (!hasHit && CombatUtils::CheckProgressiveRadial(position, player.position, player.radius, 80.0f, 1.0f)) {
                player.TakeDamage(25.0f, Vector2Scale(facing, 1000.0f));
                hasHit = true;
                aiState = AIState::CHASE;
                attackCooldown = 3.5f; // Mas castigo por fallar/acertar
            }
            if (stateTimer < -0.4f) { // Duracion del dash
                aiState = AIState::CHASE;
                attackCooldown = 3.0f;
            }
            if (Vector2Length(velocity) < 150.0f) aiState = AIState::CHASE;
        }
        break;
    case AIState::BERSERKER_TRANSITION:
    default:
        break;
    }
}
void GreatswordElite::Update() {
   if (hp <= 0 && !isDead && !isDying) { isDying = true; deathAnimTimer = 1.2f; }
    if (isDead) return;
    if (isDying) {
        deathAnimTimer -= GetFrameTime() * g_timeScale;
        if (deathAnimTimer <= 0) isDead = true;
        return;
    }

    float dt = GetFrameTime() * g_timeScale;
    position = Arena::GetClampedPos(Vector2Add(position, Vector2Scale(velocity, dt)), radius);
    velocity = Vector2Scale(velocity, 0.92f);
}

void GreatswordElite::Draw() {
    if (isDead) return;
    
    // Sombra
    DrawEllipse(position.x, position.y, radius, radius * 0.5f, Fade(BLACK, 0.4f));
    
    DrawCircleV({position.x, position.y - 30}, radius, color);
    
    if (aiState == AIState::TORBELLINO) {
        float wave = sinf((float)GetTime() * 20.0f) * 10.0f;
        float currRadius = 120.0f + wave;
        
        // --- Dibujado Progresivo del Torbellino (Reloj/Hoja giratoria) ---
        float totalWhirlTime = 2.0f;
        float progressArr = CombatUtils::GetProgress(whirlwindTimer, totalWhirlTime) * 1.4f;
        if (progressArr > 1.0f) progressArr = 1.0f;
        
        float totalDegArr = 1080.0f; // 3 vueltas completas
        float coveredDeg = totalDegArr * progressArr;
        
        float facingAngle = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
        float startAngle = facingAngle;
        
        rlPushMatrix();
        rlTranslatef(position.x, position.y, 0);
        rlScalef(1.0f, 0.5f, 1.0f);
        
        // Area que ha cubierto el ataque (rastro visual que se desvanece con wave)
        DrawCircleSector({0,0}, currRadius, startAngle, startAngle + coveredDeg,
                         36, Fade(RED, 0.35f));
                         
        // Borde exterior
        DrawCircleSectorLines({0,0}, currRadius, startAngle,
                              startAngle + coveredDeg, 36, Fade(WHITE, 0.4f));
                              
        // La "hoja" del ataque (leading edge, la manecilla)
        float leadingRad = (startAngle + coveredDeg) * DEG2RAD;
        DrawLineEx({0,0}, {cosf(leadingRad)*currRadius, sinf(leadingRad)*currRadius},
                   4.0f, Fade(WHITE, 0.9f));
                   
        // Destellito en la punta
        DrawCircleV({cosf(leadingRad)*currRadius, sinf(leadingRad)*currRadius}, 6.0f, WHITE);
        
        rlPopMatrix();
    }
    
    if (aiState == AIState::EMBESTIDA && stateTimer > 0) {
       DrawLineEx(position, Vector2Add(position, Vector2Scale(facing, 400)), 12.0f, Fade(RED, 0.8f));
    }

    // Barra de vida
    if (!isDead && !isDying) {
        float hpPct = fmaxf(0.0f, hp / maxHp);
        DrawRectangle((int)position.x - 25, (int)position.y - 55, 50, 6, BLACK);
        DrawRectangle((int)position.x - 25, (int)position.y - 55, (int)(50 * hpPct), 6, RED);
    }
}

void GreatswordElite::ScaleDifficulty(int wave) {
    maxHp *= (1.0f + wave * 0.3f);
    hp = maxHp;
}

// =====================================================================
// SimplyArcher Implementation
// =====================================================================
SimplyArcher::SimplyArcher(Vector2 pos) {
    spawnPos = pos;
    position = pos;
    maxHp = 300.0f;
    hp = maxHp;
    radius = 18.0f;
    color = { 100, 180, 100, 255 }; // Verde
}

void SimplyArcher::UpdateAI(Player& player) {
    if (isDead || isDying) return;
    m_cachedPlayer = &player; // Cachear siempre antes de Update()
    float dt = GetFrameTime() * g_timeScale;

    Vector2 diff = Vector2Subtract(player.position, position);
    float dist = Vector2Length(diff);
    bool isFiring = (aiState == AIState::SHOOT_NORMAL || aiState == AIState::SHOOT_CHARGED);
    if (dist > 0 && !isFiring) facing = Vector2Normalize(diff);

    if (shootCooldown > 0) shootCooldown -= dt;

    switch (aiState) {
    case AIState::IDLE:
        aiState = AIState::KEEP_DISTANCE;
        break;

    case AIState::KEEP_DISTANCE:
        if (dist < safeDistance) {
            // Huir
            Vector2 fleeDir = Vector2Scale(facing, -1.0f);
            velocity = Vector2Add(velocity, Vector2Scale(fleeDir, 480.0f * 5.0f * dt));
        } else {
            velocity = Vector2Scale(velocity, 0.9f);
        }

        if (shootCooldown <= 0) {
            if (GetRandomValue(0, 100) < 30) {
                aiState = AIState::SHOOT_CHARGED;
                stateTimer = 1.5f;
            } else {
                aiState = AIState::SHOOT_NORMAL;
                stateTimer = 0.5f;
            }
        }
        break;

    case AIState::SHOOT_NORMAL:
        stateTimer -= dt;
        if (stateTimer <= 0) {
            arrows.push_back({ position, facing, 800.0f, 15.0f, true, false });
            shootCooldown = 1.2f;
            aiState = AIState::KEEP_DISTANCE;
        }
        break;

    case AIState::SHOOT_CHARGED:
        stateTimer -= dt;
        velocity = {0, 0};
        if (stateTimer <= 0) {
            arrows.push_back({ position, facing, 1200.0f, 40.0f, true, true });
            shootCooldown = 2.5f;
            aiState = AIState::KEEP_DISTANCE;
        }
        break;
    }
}

void SimplyArcher::Update() {
    if (hp <= 0 && !isDead && !isDying) { isDying = true; deathAnimTimer = 1.0f; }
    if (isDead) return;
    if (isDying) {
        deathAnimTimer -= GetFrameTime() * g_timeScale;
        if (deathAnimTimer <= 0) isDead = true;
        return;
    }

    float dt = GetFrameTime() * g_timeScale;
    position = Arena::GetClampedPos(Vector2Add(position, Vector2Scale(velocity, dt)), radius);
    velocity = Vector2Scale(velocity, 0.85f);

    // Update Projectiles - usar puntero cacheado para evitar acceso a extern stale
    if (!m_cachedPlayer) return;
    for (auto& a : arrows) {
        if (!a.active) continue;
        a.pos = Vector2Add(a.pos, Vector2Scale(a.dir, a.speed * dt));
        
        // Colision
        if (!a.hasDealtDamage && CombatUtils::CheckProgressiveRadial(a.pos, m_cachedPlayer->position, m_cachedPlayer->radius, 20.0f, 1.0f)) {
            m_cachedPlayer->TakeDamage(a.damage, {0, 0});
            a.hasDealtDamage = true;
            if (!a.isCharged) a.active = false; 
            Graphics::SpawnImpactBurst(a.pos, a.dir, {100, 255, 100, 255}, WHITE, 5, 2);
        }

        // Limite mapa
        if (!Arena::IsInside(a.pos, 5.0f)) a.active = false;
    }
}

void SimplyArcher::Draw() {
    if (isDead) return;
    
    DrawEllipse(position.x, position.y, radius, radius * 0.5f, Fade(BLACK, 0.3f));
    DrawCircleV({position.x, position.y - 20}, radius, color);

    if (aiState == AIState::SHOOT_CHARGED) {
        float prog = 1.0f - (stateTimer / 1.5f);
        DrawCircleLinesV(position, 30.0f * (1.0f - prog), WHITE);
        DrawLineEx(position, Vector2Add(position, Vector2Scale(facing, 600)), 6.0f, Fade(RED, 0.7f));
    }

    for (const auto& a : arrows) {
        if (!a.active) continue;
        float r = a.isCharged ? 10.0f : 5.0f;
        Color c = a.isCharged ? GOLD : WHITE;
        DrawCircleV(a.pos, r, c);
        DrawCircleLinesV(a.pos, r + 2.0f, Fade(RED, 0.5f)); // Hitbox clara
    }

    // Barra de vida
    if (!isDead && !isDying) {
        float hpPct = fmaxf(0.0f, hp / maxHp);
        DrawRectangle((int)position.x - 20, (int)position.y - 45, 40, 6, BLACK);
        DrawRectangle((int)position.x - 20, (int)position.y - 45, (int)(40 * hpPct), 6, RED);
    }
}

void SimplyArcher::ScaleDifficulty(int wave) {
    maxHp *= (1.0f + wave * 0.2f);
    hp = maxHp;
}
