#include "raylib.h"
#include "rlgl.h"
#include "entities.h"
#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>

// --- IMPLEMENTACIÓN DEL RESOURCE MANAGER ---
Texture2D ResourceManager::texVida;
Texture2D ResourceManager::texEnergia;
Texture2D ResourceManager::texBerserker;
Texture2D ResourceManager::texBoomerang;
Texture2D ResourceManager::texUltimate;
Texture2D ResourceManager::texPlayer;
Texture2D ResourceManager::texEnemy;

void ResourceManager::Load() {
    texVida = LoadTexture("assets/vida.png");
    texEnergia = LoadTexture("assets/energia.png");
    texBerserker = LoadTexture("assets/berserker.png");
    texBoomerang = LoadTexture("assets/boomerang.png");
    texUltimate = LoadTexture("assets/ultimate.png");
    
    texPlayer = LoadTexture("assets/player.png");
    texEnemy = LoadTexture("assets/enemy.png");
}

void ResourceManager::Unload() {
    UnloadTexture(texVida);
    UnloadTexture(texEnergia);
    UnloadTexture(texBerserker);
    UnloadTexture(texBoomerang);
    UnloadTexture(texUltimate);
    
    UnloadTexture(texPlayer);
    UnloadTexture(texEnemy);
}

// Implementación de Enemy y AI (Para que compile todo junto)
std::vector<DamageText> damageTexts; // Global para este archivo
float screenShake = 0.0f; // Global para poder usarse en IA de enemigo y jugador

bool IsPlayerImmune(Player& player) {
    // Frames de Inmunidad ajustados: Difíciles de atinar. Solo invulnerable en la ventana de arranque del dash.
    // El Dash dura de 0.7 a 0.2. Inmunidad solo en el pico de vel (0.7 a 0.55 = ~9 frames o 0.15s).
    return (player.state == PlayerState::DASHING && player.dashCooldown > 0.55f);
}

bool Enemy::CheckAttackCollision(Player& player, float range, float angle, float damage) {
    if (IsPlayerImmune(player)) return false; // i-frames esquivan golpes!

    Vector2 diff = Vector2Subtract(player.position, position);
    float isoY = diff.y * 2.0f;
    float isoDist = sqrtf(diff.x * diff.x + isoY * isoY);
    
    if (isoDist < range + player.radius) {
        float angleToPlayer = atan2f(isoY, diff.x) * RAD2DEG;
        float angleFacing = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
        float angleDiff = fabsf(fmodf(angleToPlayer - angleFacing + 540.0f, 360.0f) - 180.0f);
        
        if (angleDiff <= angle / 2.0f) {
            player.hp -= damage;
            return true;
        }
    }
    return false;
}

void Enemy::UpdateAI(Player& player) {
    if (isDead) return;
    float dt = GetFrameTime();
    
    if (dashTimer > 0) dashTimer -= dt;
    if (slamTimer > 0) slamTimer -= dt;
    
    // Actualizar animación del spritesheet
    frameTimer += dt;
    if (frameTimer >= frameSpeed) {
        frameTimer = 0;
        currentFrameX = (currentFrameX + 1) % frameCols;
    }
    
    switch(aiState) {
        case AIState::IDLE:
            stateTimer -= dt;
            if (stateTimer <= 0) aiState = AIState::PURSUE;
            break;
            
        case AIState::PURSUE: {
            Vector2 diff = Vector2Subtract(player.position, position);
            float dist = Vector2Length(diff);
            if (dist > 0) facing = Vector2Normalize(diff);
            
            bool canSlam = (slamTimer <= 0.0f && dist <= 270.0f);
            bool canDash = (dashTimer <= 0.0f && dist > 260.0f && dist <= 750.0f);
            bool canBasic = (dist <= 250.0f);

            if (attackCooldown <= 0.0f && (canSlam || canDash || canBasic)) {
                if (canSlam) {
                    aiState = AIState::ATTACK_SLAM;
                    attackPhaseTimer = 1.2f; // Aviso largo para que sea esquivable
                    hasHit = false;
                    slamTimer = 12.0f;
                    attackStep = 0; 
                } else if (canDash) {
                    aiState = AIState::ATTACK_DASH;
                    attackPhaseTimer = 1.0f; 
                    hasHit = false;
                    dashTimer = 8.0f; 
                    attackStep = 0; 
                    currentDashDist = Clamp(dist, 180.0f, 700.0f); // Rango variable Min: 180, Max: 700
                } else if (canBasic) {
                    aiState = AIState::ATTACK_BASIC;
                    attackPhaseTimer = 0.3f; 
                    hasHit = false;
                }
            } else {
                if (dist > 220.0f) {
                    velocity = Vector2Add(velocity, Vector2Scale(facing, 1100.0f * dt)); 
                }
            }
            if (attackCooldown > 0) attackCooldown -= dt;
            break;
        }
        case AIState::ATTACK_BASIC: {
            attackPhaseTimer -= dt;
            if (attackPhaseTimer <= 0 && !hasHit) {
                if (CheckAttackCollision(player, 220.0f, 60.0f, 15.0f)) { // Hitbox super aumentada (220, 60 grados)
                    // Impacto de enemigo a jugador
                }
                hasHit = true;
                stateTimer = 0.3f; // Recovery rápido
                aiState = AIState::IDLE;
                attackStep++;
                if (attackStep < 2) {
                    attackCooldown = 0.2f; 
                } else {
                    attackCooldown = 0.8f; 
                    attackStep = 0; 
                }
            }
            break;
        }
        case AIState::ATTACK_DASH: {
            if (attackPhaseTimer > 0) {
                attackPhaseTimer -= dt;
                Vector2 diff = Vector2Subtract(player.position, position);
                if (Vector2Length(diff) > 0) facing = Vector2Normalize(diff);
                if (attackPhaseTimer <= 0) {
                    stateTimer = 0.35f; // Tiempo fijo del dash (0.35s de duracion)
                }
            } else {
                stateTimer -= dt;
                
                // Sobrescribir la fricción cada frame para garantizar distancia lineal estricta
                velocity = Vector2Scale(facing, currentDashDist / 0.35f);
                
                if (!hasHit) {
                    if (CheckAttackCollision(player, 100.0f, 60.0f, 36.0f)) { // 10% menos daño (36)
                        hasHit = true;
                        screenShake = 0.8f; // Ambos que sean leves, reducido
                    }
                }
                if (stateTimer <= 0) {
                    aiState = AIState::IDLE;
                    velocity = {0,0}; // Parada en seco logrando asi distancia perfecta
                    stateTimer = 1.0f;
                    attackCooldown = 1.5f;
                }
            }
            break;
        }
        case AIState::ATTACK_SLAM: {
             if (attackPhaseTimer > 0) {
                 attackPhaseTimer -= dt; 
             } else {
                 if (!hasHit) {
                     // Hitbox del Terremoto (Ajustando representación gráfica isométrica a la colisión de verdad)
                     Vector2 slamDiff = Vector2Subtract(player.position, position);
                     float slamIsoDist = Vector2Length({slamDiff.x, slamDiff.y * 2.0f});

                     if (!IsPlayerImmune(player) && slamIsoDist <= 280.0f + player.radius) {
                         player.hp -= 30.0f;
                         hasHit = true;
                         screenShake = 1.3f; // Terremoto ajustado para que sea mas notorio (aumentado un poco)
                     }
                     stateTimer = 0.6f; 
                     hasHit = true; // Solo dar el golpe una vez
                 }
                 stateTimer -= dt;
                 if (stateTimer <= 0) {
                      aiState = AIState::IDLE;
                      stateTimer = 0.5f;
                      attackCooldown = 1.5f;
                 }
             }
             break;
        }
    }
}

void Enemy::Update() {
    if (isDead) {
        respawnTimer -= GetFrameTime();
        if (respawnTimer <= 0.0f) { isDead = false; hp = maxHp; position = spawnPos; velocity = {0, 0}; }
        return;
    }
    Vector2 nextPos = Vector2Add(position, Vector2Scale(velocity, GetFrameTime()));
    
    // Restricción de Arena (Hull de Diamante)
    float dx = std::abs(nextPos.x - 2000.0f) / 1400.0f;
    float dy = std::abs(nextPos.y - 2000.0f) / 700.0f;
    if (dx + dy <= 1.0f) {
        position = nextPos;
    }
    
    velocity = Vector2Scale(velocity, 0.85f);
}

void Enemy::Draw() {
    if (isDead) return;
    DrawEllipse((int)position.x, (int)position.y, radius, radius * 0.5f, Fade(BLACK, 0.4f));
    
    if (ResourceManager::texEnemy.id != 0) {
        float frameWidth = (float)ResourceManager::texEnemy.width / frameCols;
        float frameHeight = (float)ResourceManager::texEnemy.height / frameRows;
        Rectangle sourceRec = {
            currentFrameX * frameWidth,
            currentFrameY * frameHeight,
            frameWidth * (facing.x < 0 ? -1.0f : 1.0f), // Flip sprite si mira a la izq
            frameHeight
        };
        Rectangle destRec = { position.x, position.y - 30, radius * 2, radius * 2 };
        DrawTexturePro(ResourceManager::texEnemy, sourceRec, destRec, { radius, radius }, 0.0f, WHITE);
    } else {
        DrawCircleV({position.x, position.y - 30}, radius, color);
    }
    
    // Indicador visual de ataque de la IA
    if (aiState == AIState::ATTACK_SLAM) {
        if (attackPhaseTimer > 0) {
            float alpha = 1.0f - (attackPhaseTimer / 1.2f); // Hace un progesivo 'Fade in'
            rlPushMatrix();
                rlTranslatef(position.x, position.y, 0);
                rlScalef(1.0f, 0.5f, 1.0f); // Ovalo isometrico
                DrawCircleV({0, 0}, 280.0f, Fade(RED, alpha * 0.4f)); // Terremoto
                DrawCircleLines(0, 0, 280.0f, Fade(WHITE, alpha));
            rlPopMatrix();
        }
    }
    else if (aiState == AIState::ATTACK_BASIC || aiState == AIState::ATTACK_DASH) {
        float alpha = (aiState == AIState::ATTACK_BASIC) ? 0.3f : 0.6f;
        Color warnCol = (aiState == AIState::ATTACK_DASH) ? RED : ORANGE;
        if (attackPhaseTimer > 0) {
            if (aiState == AIState::ATTACK_DASH) {
                float dashDist = currentDashDist;
                rlPushMatrix();
                    rlTranslatef(position.x, position.y, 0);
                    rlScalef(1.0f, 0.5f, 1.0f); 
                    float angleDash = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
                    Rectangle dashRect = { 0, -100.0f, dashDist, 200.0f }; 
                    DrawRectanglePro(dashRect, {0, 100.0f}, angleDash, Fade(RED, 0.5f));
                    Vector2 lineEnd = { dashDist * cosf(angleDash*DEG2RAD), dashDist * sinf(angleDash*DEG2RAD) };
                    DrawLineEx({0,0}, lineEnd, 4.0f, ORANGE);
                rlPopMatrix();
            } else {
                float warningRadius = 220.0f; // Rango de basic
                float warningAngle = 60.0f; 
                
                rlPushMatrix();
                    rlTranslatef(position.x, position.y, 0);
                    rlScalef(1.0f, 0.5f, 1.0f);
                    float startAngle = atan2f(facing.y * 2.0f, facing.x)*RAD2DEG - (warningAngle / 2.0f);
                    DrawCircleSector({0,0}, warningRadius, startAngle, startAngle + warningAngle, 32, Fade(warnCol, alpha));
                rlPopMatrix();
            }
        }
    }
    
    DrawHealthBar(80, 10);
}

void DrawQuad(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, Color c) {
    DrawTriangle(p1, p2, p3, c); DrawTriangle(p1, p3, p2, c);
    DrawTriangle(p1, p3, p4, c); DrawTriangle(p1, p4, p3, c);
}

struct Particle { Vector2 pos; Vector2 vel; float life; Color col; };

int main() {
    InitWindow(1280, 720, "Hades Prototype - Skills & Energy");
    SetTargetFPS(60);

    ResourceManager::Load(); // CARGA DE ASSETS EN GPU

    Player player({2000, 2000});
    player.energy = 100.0f;
    Enemy boss({2300, 2000});
    boss.maxHp = 2000.0f;
    boss.hp = 2000.0f;
    Vector2 barrelSpawns[4] = { {1400.0f, 2000.0f}, {2600.0f, 2000.0f}, {2000.0f, 1600.0f}, {2000.0f, 2400.0f} };
    Barrel barrel(barrelSpawns[GetRandomValue(0, 3)]); // Spawn aleatorio en una de 4 esquinas
    Camera2D camera = { {640, 360}, player.position, 0.0f, 1.0f };
    std::vector<Particle> particles;
    
    float hitLagTimer = 0.0f;
    screenShake = 0.0f; // Global init

    GamePhase currentPhase = GamePhase::RUNNING;
    int* rebindingKey = nullptr;
    std::string rebindingName = "";

    ShowCursor();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        // --- GESTIÓN DE PAUSA Y MENÚS ---
        if (IsKeyPressed(KEY_K)) {
            if (currentPhase == GamePhase::RUNNING) {
                currentPhase = GamePhase::PAUSED;
                ShowCursor();
            } else if (currentPhase == GamePhase::PAUSED || currentPhase == GamePhase::SETTINGS) {
                ShowCursor();
                rebindingKey = nullptr;
            }
        }

        if (currentPhase == GamePhase::RUNNING) {
            if (hitLagTimer > 0) hitLagTimer -= dt;
            if (screenShake > 0) screenShake -= dt;
            camera.target = player.position;
            player.targetAim = GetScreenToWorld2D(GetMousePosition(), camera);

            // Lógica de barril: curación y spawn aleatorio
            bool barrelWasDeadBefore = barrel.isDead;
            barrel.Update(dt);
            
            // Cuando reaparece, rota a posición aleatoria
            if (barrelWasDeadBefore && !barrel.isDead) {
                barrel.position = barrelSpawns[GetRandomValue(0, 3)];
                barrel.hp = barrel.maxHp;
            }

            // Colisión barril: usa el mismo cálculo isométrico que los ataques
            if (!barrel.isDead && player.state == PlayerState::ATTACKING && !player.hasHit) {
                Vector2 diffB = Vector2Subtract(barrel.position, player.position);
                float isoYB = diffB.y * 2.0f;
                float isoDistB = sqrtf(diffB.x * diffB.x + isoYB * isoYB);
                float angleFacingB = atan2f(player.facing.y, player.facing.x) * RAD2DEG;
                float angleToBarrelB = atan2f(isoYB, diffB.x) * RAD2DEG;
                float angleDiffB = fabsf(fmodf(angleToBarrelB - angleFacingB + 540.0f, 360.0f) - 180.0f);
                float hitRange = player.combo[player.comboStep].range;
                float hitAngle = player.combo[player.comboStep].angleWidth; // Corregido: angle -> angleWidth

                if (isoDistB <= hitRange + barrel.radius && angleDiffB <= hitAngle * 0.5f) {
                    float dmg = player.combo[player.comboStep].damage;
                    barrel.hp -= dmg;
                    player.hasHit = true;
                    if (barrel.hp <= 0) {
                        barrel.isDead = true;
                        barrel.respawnTimer = 15.0f;
                        // Curación directa del 50% al destruir el barril (sin objeto externo)
                        float curacion = player.maxHp * 0.5f;
                        player.hp = fminf(player.maxHp, player.hp + curacion);
                        player.vidaActual = player.hp; // Actualizar HUD
                        damageTexts.push_back({ player.position, {0, -100.0f}, 1.8f, 1.8f, (int)curacion, GREEN });
                        for (int i = 0; i < 15; i++)
                            particles.push_back({ barrel.position, { (float)GetRandomValue(-250,250), (float)GetRandomValue(-250,250) }, 0.6f, GREEN });
                    }
                }
            }

            if (hitLagTimer <= 0) {
                player.Update();
                boss.UpdateAI(player);
                boss.Update();
                
                // Arreglo de hitbox física (No atravesar cuerpos)
                if (player.state != PlayerState::DASHING && player.state != PlayerState::DASH_ATTACK && !boss.isDead) {
                    float distEntities = Vector2Distance(player.position, boss.position);
                    float minDist = player.radius + boss.radius;
                    if (distEntities < minDist && distEntities > 0) {
                        Vector2 pushDir = Vector2Normalize(Vector2Subtract(player.position, boss.position));
                        float overlap = minDist - distEntities;
                        player.position = Vector2Add(player.position, Vector2Scale(pushDir, overlap * 0.5f));
                        boss.position = Vector2Subtract(boss.position, Vector2Scale(pushDir, overlap * 0.5f));
                    }
                }
                
                // Muerte del jugador
                if (player.hp <= 0.0f) {
                    currentPhase = GamePhase::GAME_OVER;
                    ShowCursor();
                }
            }
        } else if (currentPhase == GamePhase::REBINDING) {
            int key = GetKeyPressed();
            if (key > 0) {
                *rebindingKey = key;
                currentPhase = GamePhase::SETTINGS;
                rebindingKey = nullptr;
            }
        }

        // Colisión de Giro (Spin) corrigiendo a vista Isometrica
        if (currentPhase == GamePhase::RUNNING && player.state == PlayerState::SPINNING && player.spinTimer <= 0 && !boss.isDead) {
            Vector2 spinDiff = Vector2Subtract(player.position, boss.position);
            float spinIsoDist = Vector2Length({spinDiff.x, spinDiff.y * 2.0f});
            if (spinIsoDist < player.radius + 125.0f) { // Rango Melee Radial Isometrico
                float dmg = 5.0f;
                boss.hp -= dmg; 
                player.spinHitCount++;
                player.spinTimer = 0.15f; 
                
                // Texto flotante de daño
                damageTexts.push_back({ boss.position, { (float)GetRandomValue(-30, 30), -80.0f }, 0.8f, 0.8f, (int)dmg, WHITE });
                
                // Cada 2 hits, consume 10% de energía
                if (player.spinHitCount % 2 == 0) {
                    player.energy -= 10.0f;
                    if (player.energy < 0) player.energy = 0;
                }
                
                // Partículas de impacto
                for (int i = 0; i < 5; i++) {
                    particles.push_back({ boss.position, { (float)GetRandomValue(-150, 150), (float)GetRandomValue(-150, 150) }, 0.5f, SKYBLUE });
                }
            }
        }

        // Colisión Bumeranes Activos (Múltiples y Orbitales)
        for (auto& b : player.activeBoomerangs) {
            if (b.active && !boss.isDead) {
                if (CheckCollisionCircles(b.position, b.isOrbital ? 30 : 25, boss.position, boss.radius)) {
                    float dmg = b.isOrbital ? 3.0f : 1.5f;
                    boss.hp -= dmg; 
                    
                    // Texto flotante
                    damageTexts.push_back({ boss.position, { (float)GetRandomValue(-20, 20), -60.0f }, 0.6f, 0.6f, (int)dmg, SKYBLUE });
                    
                    if (boss.hp <= 0) { boss.isDead = true; boss.respawnTimer = 3.0f; }
                }
            }
        }

        // Combate e Impactos (hasHit gestionado internamente vía hitCooldownTimer)
        if (!boss.isDead && player.state == PlayerState::ATTACKING) {
            if (player.CheckAttackCollision(boss)) {
                float dmg = player.combo[player.comboStep].damage;
                boss.hp -= dmg;
                player.hasHit = true;
                
                // Texto de daño grande
                damageTexts.push_back({ boss.position, { (float)GetRandomValue(-40, 40), -120.0f }, 1.0f, 1.0f, (int)dmg, YELLOW });
                
                // RECARGA ENERGIA (5 golpes para 100% -> 20 por golpe)
                player.energy = fminf(player.maxEnergy, player.energy + 20.0f);
                
                // ROBO DE VIDA (30% si está buffado)
                if (player.isBuffed) player.hp = fminf(player.maxHp, player.hp + (dmg * 0.3f));
                
                // Actualizar las variables del hud de inmediato para feedback instantáneo
                player.vidaActual = player.hp;
                player.estaminaActual = player.energy;

                // FEEDBACK: HIT LAG (Sutil) + SCREEN SHAKE (Mínimo)
                hitLagTimer = 0.02f; 
                screenShake = dmg * 0.02f; 
                
                // Sin empuje de knockback en básicos
                for (int i = 0; i < 20; i++) {
                    float pAngle = atan2f(player.facing.y, player.facing.x) + (float)GetRandomValue(-60, 60) * DEG2RAD;
                    float pSpeed = (float)GetRandomValue(200, 600);
                    particles.push_back({boss.position, {cosf(pAngle) * pSpeed, sinf(pAngle) * pSpeed}, 0.6f, (player.isBuffed ? GOLD : RED)});
                }
            }
        }
        
        // 3. Definitiva (R) - Solo si el enemigo tiene < 75% vida, energía al 100% y CD listo
        if (IsKeyPressed(player.controls.ultimate) && player.ultimateCooldown <= 0 && player.energy >= 100.0f && !boss.isDead && (boss.hp / boss.maxHp) < 0.75f) {
            player.ActivateUltimate();
            player.energy = 0;
            for (int i = 0; i < 20; i++) particles.push_back({player.position, {(float)GetRandomValue(-400,400), (float)GetRandomValue(-400,400)}, 0.8f, RED});
        }

        // --- COLISION DE SUPER ESTOCADA ---
        if (!boss.isDead && player.state == PlayerState::DASH_ATTACK && !player.hasDashHit) {
            if (player.CheckDashCollision(boss)) {
                float dmg = 45.0f; // 10 más que el último golpe
                boss.hp -= dmg;
                player.hasDashHit = true;
                
                damageTexts.push_back({ boss.position, { 0, -150.0f }, 1.2f, 1.2f, (int)dmg, ORANGE });
                
                screenShake = 1.2f;
                boss.velocity = Vector2Scale(player.facing, 800.0f); // Empuje reducido y moderado exclusivo para la Super Estocada
                
                for (int i = 0; i < 15; i++) {
                    particles.push_back({ boss.position, { (float)GetRandomValue(-300, 300), (float)GetRandomValue(-300, 300) }, 0.6f, GOLD });
                }
            }
        }

        // --- COLISIÓN BARRIL: ataques normales ---
        if (!barrel.isDead && player.state == PlayerState::ATTACKING &&
            player.attackPhase == AttackPhase::ATTACK_ACTIVE && player.hitCooldownTimer <= 0.0f) {
            Vector2 diff  = Vector2Subtract(barrel.position, player.position);
            float isoY    = diff.y * 2.0f;
            float isoDist = sqrtf(diff.x * diff.x + isoY * isoY);
            float range   = player.combo[player.comboStep].range + barrel.radius;
            if (isoDist < range) {
                float angleToBarrel = atan2f(isoY, diff.x) * RAD2DEG;
                float angleFacing   = atan2f(player.facing.y * 2.0f, player.facing.x) * RAD2DEG;
                float angleDiff     = fabsf(fmodf(angleToBarrel - angleFacing + 540.0f, 360.0f) - 180.0f);
                if (angleDiff <= player.combo[player.comboStep].angleWidth / 2.0f) {
                    barrel.TakeDamage(player.combo[player.comboStep].damage);
                    if (barrel.isDead) {
                        for (int i = 0; i < 14; i++)
                            particles.push_back({barrel.position,
                                {(float)GetRandomValue(-220,220),(float)GetRandomValue(-320,-60)},
                                0.8f, {139, 90, 43, 255}});
                    }
                }
            }
        }
        // --- COLISIÓN BARRIL: Super Estocada ---
        if (!barrel.isDead && player.state == PlayerState::DASH_ATTACK && !player.hasDashHit) {
            if (Vector2Distance(player.position, barrel.position) < 180.0f + barrel.radius) {
                barrel.TakeDamage(45.0f);
                for (int i = 0; i < 10; i++)
                    particles.push_back({barrel.position,
                        {(float)GetRandomValue(-220,220),(float)GetRandomValue(-320,-60)},
                        0.7f, {139, 90, 43, 255}});
            }
        }

        // Partículas
        for (auto& p : particles) { p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt)); p.life -= dt; }
        particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& p){ return p.life <= 0; }), particles.end());

        // Actualizar Textos
        for (auto& dtText : damageTexts) dtText.Update(dt);
        damageTexts.erase(std::remove_if(damageTexts.begin(), damageTexts.end(), [](const DamageText& text){ return text.life <= 0; }), damageTexts.end());

        BeginDrawing();
            ClearBackground({25, 30, 40, 255}); 
            
            // Aplicar Screen Shake Mínimo
            Camera2D shakeCam = camera;
            if (screenShake > 0) {
                shakeCam.offset.x += (float)GetRandomValue(-1, 1) * screenShake;
                shakeCam.offset.y += (float)GetRandomValue(-1, 1) * screenShake;
            }

            BeginMode2D(shakeCam);
                // Arena Isométrica Mejorada (Diamante 1400x700)
                Vector2 pNorth = { 2000, 2000 - 700 };
                Vector2 pSouth = { 2000, 2000 + 700 };
                Vector2 pEast  = { 2000 + 1400, 2000 };
                Vector2 pWest  = { 2000 - 1400, 2000 };
                
                Color groundColor = { 245, 245, 245, 255 }; // Blanco/Gris muy claro
                Color wallColor   = { 180, 185, 195, 255 }; // Color paredes (mismas que el borde pero rellenas)
                Color borderColor = { 60, 65, 75, 255 };
                
                float wallHeight = 120.0f;
                
                // --- DIBUJO DE PAREDES TRASERAS (Profundidad 2.5D) ---
                // Pared Nor-Oeste
                DrawQuad(pWest, pNorth, {pNorth.x, pNorth.y - wallHeight}, {pWest.x, pWest.y - wallHeight}, wallColor);
                // Pared Nor-Este
                DrawQuad(pNorth, pEast, {pEast.x, pEast.y - wallHeight}, {pNorth.x, pNorth.y - wallHeight}, ColorBrightness(wallColor, -0.1f));

                // Dibujar el relleno del diamante
                DrawTriangle(pWest, pNorth, pEast, groundColor);
                DrawTriangle(pWest, pEast, pSouth, groundColor);
                
                // Dibujar bordes definidos (Suelo)
                DrawLineEx(pNorth, pEast, 5.0f, borderColor);
                DrawLineEx(pEast, pSouth, 5.0f, borderColor);
                DrawLineEx(pSouth, pWest, 5.0f, borderColor);
                DrawLineEx(pWest, pNorth, 5.0f, borderColor);
                
                // Bordes superiores de las paredes
                DrawLineEx({pWest.x, pWest.y - wallHeight}, {pNorth.x, pNorth.y - wallHeight}, 5.0f, borderColor);
                DrawLineEx({pNorth.x, pNorth.y - wallHeight}, {pEast.x, pEast.y - wallHeight}, 5.0f, borderColor);
                DrawLineEx(pWest, {pWest.x, pWest.y - wallHeight}, 5.0f, borderColor);
                DrawLineEx(pNorth, {pNorth.x, pNorth.y - wallHeight}, 5.0f, borderColor);
                DrawLineEx(pEast, {pEast.x, pEast.y - wallHeight}, 5.0f, borderColor);
                
                // Z-sorting por Y: barril, jugador y enemigo dibujados en orden de profundidad
                struct YObj { float y; int id; };
                YObj objs[3] = {
                    { barrel.position.y, 0 },
                    { player.position.y, 1 },
                    { boss.position.y,   2 }
                };
                std::sort(objs, objs + 3, [](const YObj& a, const YObj& b){ return a.y < b.y; });
                for (int i = 0; i < 3; i++) {
                    if (objs[i].id == 0) barrel.Draw();
                    else if (objs[i].id == 1) player.Draw();
                    else boss.Draw();
                }

                for (auto& p : particles) DrawCircleV(p.pos, 3, p.col);
                for (auto& dtText : damageTexts) dtText.Draw();
                
            EndMode2D();

            // --- UI DE JUEGO (HUD ESTÁTICO FIJO A LA PANTALLA) ---
            if (currentPhase == GamePhase::RUNNING || currentPhase == GamePhase::PAUSED) {
                // Paso 4: Lógica de Recorte
                float pctVida = player.vidaActual / player.vidaMaxima;
                float pctEnergia = player.estaminaActual / player.estaminaMaxima;
                if (pctVida < 0.0f) pctVida = 0.0f;
                if (pctEnergia < 0.0f) pctEnergia = 0.0f;
                
                Texture2D tVida = ResourceManager::texVida;
                Texture2D tEner = ResourceManager::texEnergia;
                
                // Paso 5: Parpadeo rojo si vida baja (Feedback Visual)
                Color vidaTint = WHITE;
                bool isVidaBaja = false;
                if (pctVida < 0.25f) {
                    if ((int)(GetTime() * 10) % 2 == 0) {
                        vidaTint = RED;
                        isVidaBaja = true;
                    }
                }
                
                // Paso 6: Organización - Esquina superior izquierda
                float barScale = 2.0f; // Aumentar un poco el tamaño de las barras
                
                Rectangle srcVida = { 0, 0, (float)tVida.width * pctVida, (float)tVida.height };
                Rectangle dstVida = { 20, 20, srcVida.width * barScale, srcVida.height * barScale };
                Rectangle bgVida = { 20, 20, (float)tVida.width * barScale, (float)tVida.height * barScale };
                
                // Rellenar fondo negro y luego relleno del color respectivo
                DrawRectangleRec(bgVida, Fade(BLACK, 0.5f)); 
                DrawRectangleRec(dstVida, isVidaBaja ? RED : GREEN); 
                // Dibujar textura por encima (por si es un marco) coloreada
                DrawTexturePro(tVida, srcVida, dstVida, {0,0}, 0.0f, vidaTint);
                
                Rectangle srcEner = { 0, 0, (float)tEner.width * pctEnergia, (float)tEner.height };
                float barSpacing = (tVida.height > 0) ? (tVida.height * barScale + 10.0f) : 60.0f;
                Rectangle dstEner = { 20, 20 + barSpacing, srcEner.width * barScale, srcEner.height * barScale };
                Rectangle bgEner = { 20, 20 + barSpacing, (float)tEner.width * barScale, (float)tEner.height * barScale };
                
                DrawRectangleRec(bgEner, Fade(BLACK, 0.5f));
                DrawRectangleRec(dstEner, { 255, 165, 0, 255 }); // Amarillo-naranja
                DrawTexturePro(tEner, srcEner, dstEner, {0,0}, 0.0f, WHITE);
                // Paso 6: Organización - Esquina inferior izquierda: Iconos de habilidades
                float screenW = (float)GetScreenWidth();
                float screenH = (float)GetScreenHeight();
                
                Texture2D tBers = ResourceManager::texBerserker;
                Texture2D tBoom = ResourceManager::texBoomerang;
                Texture2D tUlti = ResourceManager::texUltimate;
                
                // Paso 5: Aplicar Feedback Visual y "Tinte"
                // Si está en recarga o no tiene energia: gris oscuro
                Color cBers = (player.estaminaActual >= 70.0f || player.berserkerActivo) ? WHITE : DARKGRAY;
                if (player.berserkerActivo) cBers = ORANGE;
                
                // Boomerang y Ultimate se pintan de Gris si están en cooldown o falta de energía
                Color cBoom = player.boomerangDisponible ? WHITE : DARKGRAY;
                Color cUlti = (player.ultimateCooldown <= 0 && player.estaminaActual >= 100.0f) ? WHITE : DARKGRAY;

                float iconScale = 1.8f;
                float bw = (tBers.width > 0 ? (float)tBers.width : 64.0f) * iconScale;
                float bh = (tBers.height > 0 ? (float)tBers.height : 64.0f) * iconScale;
                
                // Mover al lado izquierdo abajo, ORDEN: Q(Boomerang) -> E(Berserker) -> R(Ultimate)
                Vector2 posBoom = { 20.0f, screenH - bh - 20.0f };
                Vector2 posBers = { posBoom.x + bw + 15.0f, posBoom.y };
                Vector2 posUlti = { posBers.x + bw + 15.0f, posBoom.y };
                
                DrawTextureEx(tBoom, posBoom, 0.0f, iconScale, cBoom);
                DrawTextureEx(tBers, posBers, 0.0f, iconScale, cBers);
                DrawTextureEx(tUlti, posUlti, 0.0f, iconScale, cUlti);

                // Superponer temporizadores de CD o Duraciones
                if (player.boomerangCooldown > 0) {
                    DrawText(TextFormat("%.1f", player.boomerangCooldown), (int)posBoom.x + (int)(bw/2) - 15, (int)posBoom.y + (int)(bh/2) - 10, 20, WHITE);
                }
                
                if (player.berserkerActivo && player.buffTimer > 0) {
                    DrawText(TextFormat("%.1f", player.buffTimer), (int)posBers.x + (int)(bw/2) - 15, (int)posBers.y + (int)(bh/2) - 10, 20, WHITE);
                }
                
                if (player.ultimateCooldown > 0) {
                    DrawText(TextFormat("%.1f", player.ultimateCooldown), (int)posUlti.x + (int)(bw/2) - 15, (int)posUlti.y + (int)(bh/2) - 10, 20, WHITE);
                }
            }

            // --- MENÚ DE PAUSA ---
            if (currentPhase == GamePhase::PAUSED) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
                DrawText("JUEGO EN PAUSA", GetScreenWidth()/2 - 120, 150, 30, RAYWHITE);
                
                Rectangle btnResume = { (float)GetScreenWidth()/2 - 100, 250, 200, 50 };
                Rectangle btnSettings = { (float)GetScreenWidth()/2 - 100, 320, 200, 50 };
                
                if (CheckCollisionPointRec(GetMousePosition(), btnResume)) {
                    DrawRectangleRec(btnResume, GRAY);
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { currentPhase = GamePhase::RUNNING; ShowCursor(); }
                } else DrawRectangleRec(btnResume, DARKGRAY);
                DrawText("REANUDAR", (int)btnResume.x + 45, (int)btnResume.y + 15, 20, WHITE);
                
                if (CheckCollisionPointRec(GetMousePosition(), btnSettings)) {
                    DrawRectangleRec(btnSettings, GRAY);
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) currentPhase = GamePhase::SETTINGS;
                } else DrawRectangleRec(btnSettings, DARKGRAY);
                DrawText("AJUSTES", (int)btnSettings.x + 55, (int)btnSettings.y + 15, 20, WHITE);
            }
            
            // --- MENÚ DE GAME OVER ---
            if (currentPhase == GamePhase::GAME_OVER) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.8f));
                DrawText("HAS MUERTO", GetScreenWidth()/2 - MeasureText("HAS MUERTO", 50)/2, 200, 50, RED);
                
                Rectangle btnRevive = { (float)GetScreenWidth()/2 - 100, 350, 200, 60 };
                
                if (CheckCollisionPointRec(GetMousePosition(), btnRevive)) {
                    DrawRectangleRec(btnRevive, GRAY);
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        // Reset de partida
                        player.hp = player.maxHp;
                        player.position = {2000, 2000};
                        player.energy = 100.0f;
                        player.velocity = {0,0};
                        player.activeBoomerangs.clear();
                        player.state = PlayerState::NORMAL;
                        
                        boss.hp = boss.maxHp;
                        boss.position = boss.spawnPos;
                        boss.velocity = {0,0};
                        boss.aiState = Enemy::AIState::IDLE;
                        boss.stateTimer = 1.0f; 
                        boss.dashTimer = 0.0f;
                        boss.isDead = false;
                        boss.hasHit = false;
                        
                        barrel.position = barrelSpawns[GetRandomValue(0, 3)];
                        barrel.hp = barrel.maxHp;
                        barrel.isDead = false;
                        
                        particles.clear();
                        damageTexts.clear();
                        
                        currentPhase = GamePhase::RUNNING;
                    }
                } else DrawRectangleRec(btnRevive, DARKGRAY);
                DrawText("REVIVIR", (int)btnRevive.x + 50, (int)btnRevive.y + 15, 25, WHITE);
            }

            // --- MENÚ DE AJUSTES ---
            if (currentPhase == GamePhase::SETTINGS || currentPhase == GamePhase::REBINDING) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), BLACK);
                DrawText("AJUSTES Y CONTROLES", 100, 50, 30, GOLD);
                
                // Resolución
                DrawText("RESOLUCION:", 100, 120, 20, GRAY);
                Rectangle res1 = { 300, 110, 120, 40 };
                Rectangle res2 = { 430, 110, 120, 40 };
                if (CheckCollisionPointRec(GetMousePosition(), res1) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) SetWindowSize(1280, 720);
                if (CheckCollisionPointRec(GetMousePosition(), res2) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) SetWindowSize(1600, 900);
                DrawRectangleRec(res1, DARKGRAY); DrawText("720p", 335, 120, 20, WHITE);
                DrawRectangleRec(res2, DARKGRAY); DrawText("900p", 465, 120, 20, WHITE);

                // Rebinds
                DrawText("CONTROLES (Haz click para reasignar):", 100, 200, 20, GRAY);
                auto DrawRebind = [&](const char* label, int* key, int y) {
                    DrawText(label, 120, y, 20, WHITE);
                    Rectangle r = { 300, (float)y - 5, 150, 30 };
                    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
                    DrawRectangleRec(r, hover ? GRAY : DARKGRAY);
                    DrawText(currentPhase == GamePhase::REBINDING && rebindingKey == key ? "..." : TextFormat("K: %i", *key), 310, y, 20, GOLD);
                    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        currentPhase = GamePhase::REBINDING;
                        rebindingKey = key;
                        rebindingName = label;
                    }
                };
                
                DrawRebind("Dash", &player.controls.dash, 250);
                DrawRebind("Bumeran", &player.controls.boomerang, 300);
                DrawRebind("Berserker", &player.controls.berserker, 350);
                DrawRebind("Ultimate", &player.controls.ultimate, 400);

                if (currentPhase == GamePhase::REBINDING) {
                    DrawText(TextFormat("Pulsa una tecla para [%s]", rebindingName.c_str()), 100, 500, 20, SKYBLUE);
                }

                DrawText("PRESIONA K PARA VOLVER", 100, 650, 15, GRAY);
            }
        EndDrawing();
    }
    
    ResourceManager::Unload(); // LIBERAR MEMORIA GPU
    CloseWindow();
    return 0;
}