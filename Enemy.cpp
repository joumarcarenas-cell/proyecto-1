// =====================================================================
// Enemy.cpp  -  Implementacion del Golem (Boss)
// =====================================================================
// Contiene:
//   Enemy::CheckAttackCollision
//   Enemy::UpdateAI         <- maquina de estados completa del boss
//   Enemy::Update           <- fisica + respawn
//   Enemy::Draw             <- primitivas + telegrafos de ataque + rocas
// =====================================================================

#include "entities.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "include/graphics/VFXSystem.h"

#include <algorithm>
#include <cmath>

// screenShake esta definida en main.cpp y compartida via extern
extern float screenShake;

// ── CheckAttackCollision ─────────────────────────────────────────────
bool Enemy::CheckAttackCollision(Player& player, float range,
                                  float angle, float damage) {
    if (player.IsImmune()) return false;
    Vector2 diff    = Vector2Subtract(player.position, position);
    float   isoY    = diff.y * 2.0f;
    float   isoDist = sqrtf(diff.x * diff.x + isoY * isoY);
    if (isoDist < range + player.radius) {
        float ap = atan2f(isoY, diff.x) * RAD2DEG;
        float af = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
        float ad = fabsf(fmodf(ap - af + 540.0f, 360.0f) - 180.0f);
        if (ad <= angle * 0.5f) {
            player.hp -= damage;
            return true;
        }
    }
    return false;
}

// ── UpdateAI ─────────────────────────────────────────────────────────
void Enemy::UpdateAI(Player& player) {
    if (isDead) return;
    float dt = GetFrameTime();

    if (dashTimer    > 0) dashTimer    -= dt;
    if (slamTimer    > 0) slamTimer    -= dt;
    if (jumpTimer    > 0) jumpTimer    -= dt;
    if (evadeCooldown > 0) evadeCooldown -= dt;

    // Siempre mirar al jugador (excepto durante evasion lateral)
    if (aiState != AIState::EVADE) {
        Vector2 diffPlayer = Vector2Subtract(player.position, position);
        if (Vector2Length(diffPlayer) > 0.1f)
            facing = Vector2Normalize(diffPlayer);
    }

    // Velocidad de animacion segun estado
    frameSpeed = (aiState == AIState::IDLE) ? 0.16f : 0.08f;

    // Sistema de animacion por estado y direction (8 columnas x 35 filas)
    frameCols = 8; frameRows = 35;
    if (aiState != previousAIState) {
        currentFrameX = 0; frameTimer = 0.0f;
        previousAIState = aiState;
    }

    // Mapeo de direccion: 5 direcciones (S=0, SW=1, W=2, NW=3, N=4)
    // Las direcciones Este se generan con flip horizontal en Draw()
    int baseDir = 0;
    if      (facing.y >  0.5f) { baseDir = (fabsf(facing.x) > 0.5f) ? 1 : 0; }
    else if (facing.y < -0.5f) { baseDir = (fabsf(facing.x) > 0.5f) ? 3 : 4; }
    else                       { baseDir = 2; } // W / E (flip en Draw)

    int maxFrames = 3;
    if (aiState == AIState::STAGGERED || aiState == AIState::IDLE) {
        currentFrameY = 0 + baseDir;   maxFrames = 3;
    }
    else if (aiState == AIState::CHASE   || aiState == AIState::ORBITING ||
             aiState == AIState::EVADE   || aiState == AIState::ATTACK_DASH) {
        currentFrameY = 8 + baseDir;   maxFrames = 7;
    }
    else if (aiState == AIState::ATTACK_BASIC) {
        int d = (baseDir > 3) ? 3 : baseDir;
        currentFrameY = 13 + d;        maxFrames = 4;
    }
    else if (aiState == AIState::ATTACK_HEAVY) {
        int d = (baseDir > 2) ? 2 : baseDir;
        currentFrameY = 21 + d;        maxFrames = 7;
    }
    else if (aiState == AIState::ATTACK_SLAM || aiState == AIState::ATTACK_JUMP) {
        currentFrameY = 24 + baseDir;  maxFrames = 5;
    }
    else if (aiState == AIState::ATTACK_ROCKS) {
        currentFrameY = 29 + baseDir;  maxFrames = 6;
    }
    else {
        currentFrameY = 0 + baseDir;   maxFrames = 3;
    }

    frameTimer += dt;
    if (frameTimer >= frameSpeed) {
        frameTimer = 0;
        currentFrameX = (currentFrameX + 1) % maxFrames;
    }
    if (hitFlashTimer > 0) hitFlashTimer -= dt;

    // Lluvia de Rocas
    if (rocksToSpawn > 0) {
        rockSpawnTimer -= dt;
        if (rockSpawnTimer <= 0.0f && rocksSpawned < 5) {
            rocks[rocksSpawned].position  = player.position;
            rocks[rocksSpawned].fallTimer = 1.0f;
            rocks[rocksSpawned].active    = true;
            rocksSpawned++;
            rocksToSpawn--;
            rockSpawnTimer = 0.4f;
        }
    }

    // Update caida y colision de rocas
    for (int i = 0; i < rocksSpawned; i++) {
        if (rocks[i].active) {
            rocks[i].fallTimer -= dt;
            if (rocks[i].fallTimer <= 0) {
                rocks[i].active = false;
                screenShake = fmaxf(screenShake, 1.5f);
                
                Vector2 diff = Vector2Subtract(player.position, rocks[i].position);
                float isoY = diff.y * 2.0f;
                if (!player.IsImmune() && sqrtf(diff.x*diff.x + isoY*isoY) < 60.0f + player.radius) {
                    player.hp -= 20.0f; // Dano de la roca
                    player.slowTimer = 1.0f; // Ralentiza un poco al golpear
                }
            }
        }
    }

    // Evasion reactiva
    if (evadeCooldown <= 0.0f &&
        (aiState == AIState::CHASE || aiState == AIState::ORBITING ||
         aiState == AIState::IDLE))
    {
        if (player.isCharging || player.attackPhase == AttackPhase::STARTUP) {
            float d = Vector2Distance(player.position, position);
            if (d <= 350.0f && GetRandomValue(1, 100) <= 30) {
                aiState = AIState::EVADE; stateTimer = 0.35f;
                Vector2 away = Vector2Subtract(position, player.position);
                evadeDir = (GetRandomValue(0, 1) == 0)
                             ? Vector2{-away.y, away.x}
                             : away;
                if (Vector2Length(evadeDir) > 0) evadeDir = Vector2Normalize(evadeDir);
                evadeCooldown = 4.0f;
            } else { evadeCooldown = 0.5f; }
        }
    }

    switch (aiState) {
    case AIState::IDLE:
        stateTimer -= dt;
        if (stateTimer <= 0) aiState = AIState::CHASE;
        break;

    case AIState::CHASE:
    case AIState::ORBITING: {
        Vector2 diff = Vector2Subtract(player.position, position);
        float   dist = Vector2Length(diff);
        if (dist > 0) facing = Vector2Normalize(diff);

        if      (dist > 350.0f) aiState = AIState::CHASE; // Se acerca mas antes de orbitar
        else if (dist > 150.0f) {
            if (aiState != AIState::ORBITING) {
                aiState = AIState::ORBITING;
                orbitDir   = GetRandomValue(0, 1) ? 1 : -1;
                orbitAngle = atan2f(diff.y, diff.x);
            }
        } else { aiState = AIState::CHASE; }

        if (aiState == AIState::CHASE) {
            if (dist > 180.0f) velocity = Vector2Add(velocity, Vector2Scale(facing, 1400.0f * dt));
        } else {
            orbitAngle += orbitDir * 1.5f * dt;
            Vector2 tgt = { player.position.x - cosf(orbitAngle) * 350.0f,
                            player.position.y - sinf(orbitAngle) * 350.0f };
            Vector2 md  = Vector2Normalize(Vector2Subtract(tgt, position));
            velocity = Vector2Add(velocity, Vector2Scale(md, 1500.0f * dt));
        }

        if (attackCooldown <= 0.0f) {
            bool canJump  = (jumpTimer <= 0 && dist <= 200.0f);
            bool canSlam  = (slamTimer <= 0 && dist <= 300.0f);
            bool canDash  = (dashTimer <= 0 && dist > 260.0f && dist <= 750.0f);
            bool canBasic = (dist <= 260.0f);

            if (canJump) {
                aiState = AIState::ATTACK_JUMP; attackPhaseTimer = 1.0f;
                hasHit = false; jumpTimer = 15.0f; attackStep = 0;
            } else if (canSlam && GetRandomValue(1, 100) <= 60) {
                aiState = AIState::ATTACK_SLAM; attackPhaseTimer = 0.9f;
                hasHit = false; slamTimer = 8.0f; attackStep = 0;
            } else if (canDash && GetRandomValue(1, 100) <= 25) { // Menos spam de embestida (25%)
                aiState = AIState::ATTACK_DASH; attackPhaseTimer = 0.7f;
                hasHit = false; dashTimer = 9.0f; attackStep = 0; // Mas cd de embestida
                currentDashDist = Clamp(dist, 180.0f, 700.0f);
            } else if (canBasic) {
                aiState = AIState::ATTACK_BASIC; attackPhaseTimer = 0.3f;
                hasHit = false;
            }
        } else { attackCooldown -= dt * aggressionLevel; }
        break;
    }

    case AIState::ATTACK_BASIC: {
        attackPhaseTimer -= dt;
        if (attackPhaseTimer <= 0 && !hasHit) {
            CheckAttackCollision(player, 260.0f, 130.0f, 15.0f); // Hitbox mejorado: 260 rango, 130 grados
            hasHit = true; stateTimer = 0.3f; attackStep++;
            if (attackStep < 2) { aiState = AIState::IDLE; attackCooldown = 0.1f; }
            else {
                attackStep = 0;
                int r = GetRandomValue(1, 100);
                if (r <= 33) {
                    aiState = AIState::ATTACK_HEAVY; attackPhaseTimer = 0.6f;
                    hasHit = false;
                } else if (r <= 66) {
                    aiState = AIState::ATTACK_ROCKS; attackPhaseTimer = 1.0f;
                    rocksSpawned = 0; rockSpawnTimer = 0.0f;
                    screenShake = fmaxf(screenShake, 1.0f);
                } else { aiState = AIState::IDLE; attackCooldown = 0.45f; }
            }
        }
        break;
    }

    case AIState::ATTACK_DASH: {
        if (attackPhaseTimer > 0) {
            attackPhaseTimer -= dt;
            Vector2 diff = Vector2Subtract(player.position, position);
            if (Vector2Length(diff) > 0) facing = Vector2Normalize(diff);
            if (attackPhaseTimer <= 0) stateTimer = 0.35f;
        } else {
            stateTimer -= dt;
            velocity = Vector2Scale(facing, currentDashDist / 0.35f);
            if (!hasHit && CheckAttackCollision(player, 100.0f, 60.0f, 36.0f)) {
                hasHit = true; screenShake = 3.0f; player.stunTimer = 0.5f;
            }
            if (stateTimer <= 0) {
                velocity = {0, 0};
                if (dashCharges < 1) { dashCharges++; attackPhaseTimer = 0.4f; hasHit = false; }
                else { aiState = AIState::IDLE; dashCharges = 0; stateTimer = 1.0f; attackCooldown = 1.5f; }
            }
        }
        break;
    }

    case AIState::ATTACK_SLAM: {
        if (attackPhaseTimer > 0) {
            attackPhaseTimer -= dt;
            if (attackPhaseTimer < 0.4f) screenShake = fmaxf(screenShake, 0.4f);
        } else if (!hasHit) {
            Vector2 sd  = Vector2Subtract(player.position, position);
            float   sid = Vector2Length({sd.x, sd.y * 2.0f});
            if (!player.IsImmune() && sid <= 350.0f + player.radius) {
                player.hp -= 30.0f; player.slowTimer = 1.5f;
                float prox = 1.0f - (sid / 350.0f); if (prox < 0) prox = 0;
                screenShake = 1.8f + 2.5f * prox;
            }
            hasHit = true; stateTimer = 0.8f;
        } else {
            stateTimer -= dt;
            if (stateTimer <= 0) { aiState = AIState::IDLE; stateTimer = 0.5f; attackCooldown = 1.2f; }
        }
        break;
    }

    case AIState::ATTACK_HEAVY: {
        attackPhaseTimer -= dt;
        if (attackPhaseTimer <= 0 && !hasHit) {
            CheckAttackCollision(player, 330.0f, 30.0f, 25.0f);
            Vector2 tip = Vector2Add(position, Vector2Scale(facing, 330.0f));
            Graphics::SpawnImpactBurst(tip, facing, ORANGE, {255,120,0,255}, 10, 4);
            Vector2 td  = Vector2Subtract(player.position, tip);
            float   tid = sqrtf(td.x * td.x + (td.y * 2.0f) * (td.y * 2.0f));
            if (!player.IsImmune() && tid <= 60.0f + player.radius) player.hp -= 10.0f;
            hasHit = true; stateTimer = 0.6f; screenShake = 1.5f;
        }
        if (hasHit) { stateTimer -= dt; if (stateTimer <= 0) { aiState = AIState::IDLE; attackCooldown = 0.8f; } }
        break;
    }

    case AIState::ATTACK_JUMP: {
        if (attackPhaseTimer > 0) {
            attackPhaseTimer -= dt;
            Vector2 diff = Vector2Subtract(player.position, position);
            if (Vector2Length(diff) > 20.0f && attackPhaseTimer > 0.3f)
                velocity = Vector2Add(velocity, Vector2Scale(Vector2Normalize(diff), 800.0f * dt));
        } else if (!hasHit) {
            screenShake = fmaxf(screenShake, 2.5f); hasHit = true;
            Vector2 diff = Vector2Subtract(player.position, position);
            float   iso  = Vector2Length({diff.x, diff.y * 2.0f});
            if (!player.IsImmune() && iso <= 300.0f + player.radius) {
                player.hp -= 35.0f;
                player.velocity = Vector2Add(player.velocity,
                    Vector2Scale(Vector2Normalize(diff), 2800.0f));
            }
            // Explosion de impacto al aterrizar
            for (int i = 0; i < 30; i++)
                Graphics::VFXSystem::GetInstance().SpawnParticleEx(position,
                    { (float)GetRandomValue(-500, 500), (float)GetRandomValue(-500, 500) },
                    0.6f, RED, 5.0f);
        } else {
            stateTimer -= dt; velocity = Vector2Scale(velocity, 0.5f);
            if (stateTimer <= 0) { aiState = AIState::IDLE; attackCooldown = baseAttackCooldown; }
        }
        break;
    }

    case AIState::ATTACK_ROCKS: {
        attackPhaseTimer -= dt;
        if (attackPhaseTimer <= 0) { rocksToSpawn = 5; stateTimer = 0.4f; aiState = AIState::CHASE; attackCooldown = 0.5f; }
        break;
    }

    case AIState::EVADE: {
        stateTimer -= dt;
        velocity = Vector2Scale(evadeDir, 800.0f);
        if (GetRandomValue(1, 100) <= 40)
            Graphics::SpawnDashTrail(position);
        Vector2 d = Vector2Subtract(player.position, position);
        if (Vector2Length(d) > 0) facing = Vector2Normalize(d);
        if (stateTimer <= 0) { aiState = AIState::CHASE; velocity = {0, 0}; attackCooldown = 0.2f; }
        break;
    }

    case AIState::STAGGERED: {
        stateTimer -= dt;
        if (stateTimer <= 0) {
            aiState = AIState::IDLE; stateTimer = 0.3f;
            for (int i = 0; i < 8; i++)
                Graphics::VFXSystem::GetInstance().SpawnParticleEx(position,
                    { (float)GetRandomValue(-200, 200), (float)GetRandomValue(-200, 200) },
                    0.5f, WHITE, 4.0f);
        }
        break;
    }
    }
}

// ── Enemy::Update ────────────────────────────────────────────────────
void Enemy::Update() {
    if (isDead) {
        respawnTimer -= GetFrameTime();
        if (respawnTimer <= 0) { isDead = false; hp = maxHp; position = spawnPos; velocity = {0, 0}; }
        return;
    }
    if (recentDamageTimer > 0) { recentDamageTimer -= GetFrameTime(); if (recentDamageTimer <= 0) recentDamage = 0; }
    Vector2 next = Vector2Add(position, Vector2Scale(velocity, GetFrameTime()));
    float dx = std::abs(next.x - 2000.0f), dy = std::abs(next.y - 2000.0f);
    if ((dx + 2.0f * dy) <= (1400.0f - radius * 2.236f)) position = next;
    velocity = Vector2Scale(velocity, 0.85f);
}

// ── Enemy::Draw ──────────────────────────────────────────────────────
void Enemy::Draw() {
    if (isDead) return;

    float jumpH = 0.0f;
    if (aiState == AIState::ATTACK_JUMP && attackPhaseTimer > 0)
        jumpH = sinf((1.0f - attackPhaseTimer) * PI) * 450.0f;

    bool telegraphing = (attackPhaseTimer > 0 &&
        aiState != AIState::IDLE   && aiState != AIState::CHASE &&
        aiState != AIState::ORBITING && aiState != AIState::STAGGERED &&
        aiState != AIState::EVADE);

    Color tint = WHITE;
    if (hitFlashTimer > 0) { tint = WHITE; color = WHITE; }
    else if (telegraphing) {
        float f = fmaxf(0, sinf(attackPhaseTimer * 30.0f));
        tint.g = (unsigned char)(255 * (1 - f));
        tint.b = (unsigned char)(255 * (1 - f));
        color = { (unsigned char)(130 + 125 * f), 0, 0, 255 };
    } else { color = MAROON; }

    DrawEllipse((int)position.x, (int)position.y, radius, radius * 0.5f, Fade(BLACK, 0.4f));

    // ── Sprite del Golem ─────────────────────────────────────────────
    if (ResourceManager::texEnemy.id != 0) {
        // Sprite sheet: 128x128 px por frame, 8 columnas x 35 filas
        float fw = 128.0f, fh = 128.0f;

        // Flip horizontal si el Golem mira hacia la DERECHA
        // (el sprite sheet original dibuja hacia la IZQUIERDA en side-views)
        float drawSrcW = (facing.x > 0.1f) ? -fw : fw;

        Rectangle src  = { (float)currentFrameX * fw, (float)currentFrameY * fh,
                           drawSrcW, fh };
        Rectangle dest = { position.x, position.y - jumpH, 256.0f, 256.0f };
        Vector2   orig = { 128.0f, 256.0f * 0.85f };
        DrawTexturePro(ResourceManager::texEnemy, src, dest, orig, 0.0f, tint);
    } else {
        // Fallback hasta que llegue el nuevo sprite
        DrawCircleV({position.x, position.y - 30.0f - jumpH}, radius, color);
        DrawLineV(
            {position.x, position.y},
            {position.x + facing.x * radius * 1.4f,
             position.y + facing.y * radius * 0.7f},
            Fade(WHITE, 0.6f));
    }

    // ── Indicadores de telegrafos de ataque ──────────────────────────
    if (aiState == AIState::ATTACK_SLAM && attackPhaseTimer > 0) {
        float prog  = 1.0f - (attackPhaseTimer / 1.2f);
        float pulse = 0.5f + 0.4f * sinf((float)GetTime() * 15.0f) * prog;
        DrawEllipse(position.x, position.y, 350.0f, 350.0f * 0.5f, Fade(RED, prog * 0.5f + 0.1f));
        DrawEllipseLines(position.x, position.y, 350.0f, 350.0f * 0.5f, Fade(WHITE, pulse));
        DrawEllipseLines(position.x, position.y, 350.0f * (1 - prog), 350.0f * (1 - prog) * 0.5f, Fade(RED, 0.8f));

    } else if (aiState == AIState::ATTACK_HEAVY && attackPhaseTimer > 0) {
        float prog = 1.0f - (attackPhaseTimer / 0.6f);
        float ang  = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
        rlDrawRenderBatchActive();
        rlPushMatrix(); rlTranslatef(position.x, position.y, 0); rlScalef(1, 0.5f, 1);
        DrawRectanglePro({0,-30,330,60},        {0,30}, ang, Fade(ORANGE, 0.2f));
        DrawRectanglePro({0,-30,330*prog,60},   {0,30}, ang, Fade(ORANGE, 0.5f + 0.5f * prog));
        rlDrawRenderBatchActive();
        rlPopMatrix();

    } else if (aiState == AIState::ATTACK_BASIC && attackPhaseTimer > 0) {
        float prog = 1.0f - (attackPhaseTimer / 0.3f);
        float ang  = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
        rlDrawRenderBatchActive();
        rlPushMatrix(); rlTranslatef(position.x, position.y, 0); rlScalef(1, 0.5f, 1);
        DrawCircleSector({0, 0}, 260.0f, ang - 65.0f, ang + 65.0f, 32, Fade(RED, 0.15f));
        DrawCircleSectorLines({0, 0}, 260.0f, ang - 65.0f, ang + 65.0f, 32, Fade(RED, 0.4f + 0.6f * prog));
        rlDrawRenderBatchActive();
        rlPopMatrix();

    } else if (aiState == AIState::ATTACK_DASH && attackPhaseTimer > 0) {
        float prog = 1.0f - (attackPhaseTimer / 0.7f);
        float ang  = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
        rlDrawRenderBatchActive();
        rlPushMatrix(); rlTranslatef(position.x, position.y, 0); rlScalef(1, 0.5f, 1);
        DrawRectanglePro({0,-50,currentDashDist,100}, {0,50}, ang, Fade(PURPLE, 0.2f));
        DrawRectanglePro({0,-50,currentDashDist*prog,100}, {0,50}, ang, Fade(PURPLE, 0.6f));
        rlDrawRenderBatchActive();
        rlPopMatrix();

    } else if (aiState == AIState::ATTACK_JUMP) {
        rlDrawRenderBatchActive();
        rlPushMatrix(); rlTranslatef(position.x, position.y, 0); rlScalef(1, 0.5f, 1);
        if (attackPhaseTimer > 0) {
            float p = 1.0f - (attackPhaseTimer / 1.0f);
            DrawCircleV({0, 0}, 300, Fade(RED, p * 0.4f));
            DrawCircleLines(0, 0, 300, Fade(WHITE, 0.5f));
        } else {
            float wp = 1.0f - (stateTimer / 0.6f);
            DrawCircleLines(0, 0, 300 * wp,      Fade(WHITE, 1 - wp));
            DrawCircleLines(0, 0, 300 * wp + 15, Fade(RED,   1 - wp));
        }
        rlDrawRenderBatchActive();
        rlPopMatrix();
    }

    // ── Rocas cayendo ─────────────────────────────────────────────────
    for (int i = 0; i < rocksSpawned; i++) {
        if (!rocks[i].active) continue;
        float prog = 1.0f - rocks[i].fallTimer;
        rlDrawRenderBatchActive();
        rlPushMatrix();
        rlTranslatef(rocks[i].position.x, rocks[i].position.y, 0);
        rlScalef(1, 0.5f, 1);
        DrawCircleV({0, 0}, 60 * prog, Fade(BLACK, 0.5f + 0.3f * prog));
        DrawCircleLines(0, 0, 60, Fade(RED, 0.7f));
        rlDrawRenderBatchActive();
        rlPopMatrix();
        DrawCircleV({rocks[i].position.x, rocks[i].position.y - 1000.0f * (1 - prog)},
                    20, DARKGRAY);
    }
}
