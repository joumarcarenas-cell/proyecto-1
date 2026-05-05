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
#include "include/DirectionUtils.h"
#include "include/graphics/VFXSystem.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>

extern float g_timeScale;
extern double g_gameTime;
extern float screenShake;
// Eliminado a favor de usar CombatUtils::CheckProgressiveSweep / Thrust /
// Radial para cumplir con la arquitectura progresiva universal para todos los
// enemigos.

// ── UpdateAI ─────────────────────────────────────────────────────────
void Enemy::UpdateAI(Player &player) {
  if (isDead || isDying)
    return;
  float dt = GetFrameTime() * g_timeScale;

  if (dashTimer > 0)
    dashTimer -= dt;
  if (jumpTimer > 0)
    jumpTimer -= dt;
  if (evadeCooldown > 0)
    evadeCooldown -= dt;

  // Siempre mirar al jugador excepto durante el 'active frame' del ataque (User
  // requested auto-aim fix)
  bool isAttacking =
      (aiState == AIState::ATTACK_BASIC || aiState == AIState::ATTACK_HEAVY ||
       aiState == AIState::ATTACK_SLAM || aiState == AIState::ATTACK_JUMP ||
       aiState == AIState::ATTACK_ROCKS || aiState == AIState::ATTACK_DASH ||
       aiState == AIState::AVALANCHE_START ||
       aiState == AIState::AVALANCHE_ACTIVE);

  bool canRotate = !isAttacking;
  if (aiState == AIState::ATTACK_BASIC && attackPhaseTimer > 0.08f)
    canRotate = true;
  if (aiState == AIState::ATTACK_HEAVY && attackPhaseTimer > 0.20f)
    canRotate = true;
  if (aiState == AIState::ATTACK_SLAM && attackPhaseTimer > 0.25f)
    canRotate = true;
  if (aiState == AIState::ATTACK_DASH && attackPhaseTimer > 0.15f)
    canRotate = true;
  if (aiState == AIState::ATTACK_JUMP && attackPhaseTimer > 0.22f)
    canRotate = true;

  if (canRotate && aiState != AIState::EVADE) {
    Vector2 diffPlayer = Vector2Subtract(player.position, position);
    if (Vector2Length(diffPlayer) > 0.11f)
      facing = Vector2Normalize(diffPlayer);
  }

  // Velocidad de animacion segun estado
  frameSpeed = (aiState == AIState::IDLE) ? 0.16f : 0.08f;

  // Sistema de animacion por estado y direction (8 columnas x 35 filas)
  frameCols = 8;
  frameRows = 35;
  if (aiState != previousAIState) {
    currentFrameX = 0;
    frameTimer = 0.0f;
    previousAIState = aiState;
  }

  // Mapeo de direccion: 5 direcciones (S=0, SW=1, W=2, NW=3, N=4)
  // Las direcciones Este se generan con flip horizontal en Draw()
  int baseDir = 0;
  if (facing.y > 0.5f) {
    baseDir = (fabsf(facing.x) > 0.5f) ? 1 : 0;
  } else if (facing.y < -0.5f) {
    baseDir = (fabsf(facing.x) > 0.5f) ? 3 : 4;
  } else {
    baseDir = 2;
  } // W / E (flip en Draw)

  int maxFrames = 3;
  if (aiState == AIState::STAGGERED || aiState == AIState::IDLE) {
    currentFrameY = 0 + baseDir;
    maxFrames = 3;
  } else if (aiState == AIState::CHASE || aiState == AIState::EVADE ||
             aiState == AIState::ATTACK_DASH) {
    currentFrameY = 8 + baseDir;
    maxFrames = 7;
  } else if (aiState == AIState::ATTACK_BASIC) {
    int d = (baseDir > 3) ? 3 : baseDir;
    currentFrameY = 13 + d;
    maxFrames = 4;
  } else if (aiState == AIState::ATTACK_HEAVY) {
    int d = (baseDir > 2) ? 2 : baseDir;
    currentFrameY = 21 + d;
    maxFrames = 7;
  } else if (aiState == AIState::ATTACK_SLAM ||
             aiState == AIState::ATTACK_JUMP) {
    currentFrameY = 24 + baseDir;
    maxFrames = 5;
  } else if (aiState == AIState::ATTACK_ROCKS) {
    currentFrameY = 29 + baseDir;
    maxFrames = 6;
  } else {
    currentFrameY = 0 + baseDir;
    maxFrames = 3;
  }

  frameTimer += dt;
  if (frameTimer >= frameSpeed) {
    frameTimer = 0;
    currentFrameX = (currentFrameX + 1) % maxFrames;
  }
  if (hitFlashTimer > 0)
    hitFlashTimer -= dt;

  // Lluvia de Rocas
  if (rocksToSpawn > 0) {
    rockSpawnTimer -= dt;
    if (rockSpawnTimer <= 0.0f && rocksSpawned < 5) {
      rocks[rocksSpawned].position = player.position;
      rocks[rocksSpawned].fallTimer = 1.0f;
      rocks[rocksSpawned].active = true;
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
        screenShake = fmaxf(screenShake, 1.0f); // Reducido (era 1.5)

        Vector2 diff = Vector2Subtract(player.position, rocks[i].position);
        float isoY = diff.y * 2.0f;
        if (sqrtf(diff.x * diff.x + isoY * isoY) < 60.0f + player.radius) {
          player.TakeDamage(
              15.0f,
              {0,
               0}); // Dano de la roca reducido para evitar one-shots en ráfaga
          player.slowTimer = 1.0f; // Ralentiza un poco al golpear
        }
      }
    }
  }

  // Evasion reactiva (Desactivada: Boss Rush sin interrupciones)
  /*
  if (evadeCooldown <= 0.0f &&
      (aiState == AIState::CHASE || aiState == AIState::ORBITING ||
       aiState == AIState::IDLE)) {
    if (player.isCharging || player.attackPhase == AttackPhase::STARTUP) {
      float d = Vector2Distance(player.position, position);
      if (d <= 350.0f && GetRandomValue(1, 100) <= 30) {
        aiState = AIState::EVADE;
        stateTimer = 0.35f;
        Vector2 away = Vector2Subtract(position, player.position);
        evadeDir =
            (GetRandomValue(0, 1) == 0) ? Vector2{-away.y, away.x} : away;
        if (Vector2Length(evadeDir) > 0)
          evadeDir = Vector2Normalize(evadeDir);
        evadeCooldown = 4.0f;
      } else {
        evadeCooldown = 0.5f;
      }
    }
  }
  */

  // ── Actualización de Vuelo de Rocas (Fase 75%) ──
  for (int i = 0; i < 8; i++) {
    if (thrownRocks[i].active) {
      thrownRocks[i].position = Vector2Add(
          thrownRocks[i].position,
          Vector2Scale(thrownRocks[i].direction, thrownRocks[i].speed * dt));
      // Hit detection
      if (CombatUtils::GetIsoDistance(thrownRocks[i].position,
                                      player.position) <
          (35.0f + player.radius)) {
        player.TakeDamage(18.0f,
                          Vector2Scale(thrownRocks[i].direction, 1800.0f));
        thrownRocks[i].active = false;
      }
      // Boundaries (dinámicos según el radio de la arena + margen)
      float boundLimit = Arena::RADIUS * 1.5f;
      Vector2 distFromCenter = Vector2Subtract(
          thrownRocks[i].position, {Arena::CENTER_X, Arena::CENTER_Y});
      if (fabsf(distFromCenter.x) > boundLimit ||
          fabsf(distFromCenter.y) > boundLimit) {
        thrownRocks[i].active = false;
      }
    }
  }

  // ── Procesamiento de Bombas de Desesperación (Fase 10%) ──
  if (desperationTriggered) {
    for (int i = 0; i < 5; i++) {
      if (desperationBombs[i].active) {
        desperationBombs[i].delay -= dt;
        if (desperationBombs[i].delay <= 0) {
          desperationBombs[i].active = false;
          screenShake = fmaxf(screenShake, 1.8f);
          for (int k = 0; k < 15; k++)
            Graphics::VFXSystem::GetInstance().SpawnParticleEx(
                desperationBombs[i].position,
                {(float)GetRandomValue(-400, 400),
                 (float)GetRandomValue(-400, 400)},
                0.5f, RED, 6.0f);

          if (CombatUtils::CheckProgressiveRadial(
                  desperationBombs[i].position, player.position, player.radius,
                  desperationBombs[i].radius, 1.0f)) {
            Vector2 away =
                Vector2Subtract(player.position, desperationBombs[i].position);
            Vector2 knockback = {0, 0};
            if (Vector2Length(away) > 0)
              knockback = Vector2Scale(Vector2Normalize(away), 2000.0f);
            player.TakeDamage(32.0f, knockback);
          }
        }
      }
    }
  }

  // ── Actualización Global de Ondas Sísmicas (Fase 50% y 10%) ──
  if (aiState == AIState::AVALANCHE_ACTIVE ||
      aiState == AIState::DESPERATION_ACTIVE) {
    for (int i = 0; i < waveCount; i++) {
      if (!waves[i].active)
        continue;
      waves[i].radius += waves[i].speed * dt;

      float dist =
          CombatUtils::GetIsoDistance(waves[i].center, player.position);
      if (!waves[i].hasHit &&
          fabsf(dist - waves[i].radius) < (42.0f + player.radius)) {
        player.TakeDamage(30.0f,
                          Vector2Scale(Vector2Normalize(Vector2Subtract(
                                           player.position, waves[i].center)),
                                       2200.0f));
        waves[i].hasHit = true;
      }

      if (waves[i].radius > Arena::RADIUS * 1.5f)
        waves[i].active = false;
    }
  }

  switch (aiState) {
  case AIState::IDLE:
    stateTimer -= dt;
    if (stateTimer <= 0)
      aiState = AIState::CHASE;
    break;

  case AIState::CHASE: {
    Vector2 diff = Vector2Subtract(player.position, position);
    float dist = Vector2Length(diff);
    if (dist > 0)
      facing = Vector2Normalize(diff);

    // ── Triggers de Fases ───────────────────────────────────────────
    if (!desperationTriggered && hp <= maxHp * 0.10f) {
      desperationTriggered = true;
      aiState = AIState::DESPERATION_START;
      stateTimer = 0.0f;
      isBleeding = false; // El 10% SÍ limpia
      bleedTimer = 0.0f;
      bleedTotalDamage = 0.0f;
      staticStacks = 0;
      poiseCurrent = poiseMax;
      return;
    }

    if (!avalancheTriggered && hp <= maxHp * 0.50f) { // Movido al 50%
      avalancheTriggered = true;
      aiState = AIState::AVALANCHE_START;
      stateTimer = 0.0f;
      waveCount = 0;
      poiseCurrent = poiseMax;
      return;
    }

    if (!phase75Triggered && hp <= maxHp * 0.75f) {
      phase75Triggered = true;
      aiState = AIState::CENTER_ROCKS_START;
      stateTimer = 0.0f;
      phaseTimer = 0.0f;
      poiseCurrent = poiseMax;
      return;
    }

    // Persecución constante y agresiva (Sin orbitar)
    float currentChaseSpeed =
        desperationTriggered ? 2800.0f : 2100.0f; // Mas veloz en 10%
    if (dist > radius + player.radius + 30.0f) {
      velocity =
          Vector2Add(velocity, Vector2Scale(facing, currentChaseSpeed * dt));
    }

    // ── Bombardeo Aleatorio durante Persecución Final (Fase 10%) ──
    if (desperationTriggered) {
      desperationBombTimer -= dt;
      if (desperationBombTimer <= 0.0f) {
        desperationBombTimer = 1.0f; // Intervalo rapido

        for (int b = 0; b < 3; b++) { // 3 bombas para mas agresividad
          for (int i = 0; i < 5; i++) {
            if (!desperationBombs[i].active) {
              desperationBombs[i].active = true;
              desperationBombs[i].radius = 240.0f;
              desperationBombs[i].delay = 0.9f;

              // Centradas en el jugador para no salirse del mapa
              Vector2 bombPos = {
                  player.position.x + (float)GetRandomValue(-450, 450),
                  player.position.y + (float)GetRandomValue(-450, 450)};
              desperationBombs[i].position =
                  Arena::GetClampedPos(bombPos, 0.0f);
              break;
            }
          }
        }
      }
    }

    if (attackCooldown <= 0) {
      bool canJump = (jumpTimer <= 0 && dist <= 380.0f && dist > 150.0f);
      bool canSlam = (slamTimer <= 0 && dist <= 350.0f);
      bool canDash = (dashTimer <= 0 && dist > 260.0f && dist <= 900.0f);
      bool canBasic = (dist <= 220.0f);

      if (canDash &&
          GetRandomValue(1, 100) <= 55) { // Prioridad para cerrar distancia
        aiState = AIState::ATTACK_DASH;
        attackPhaseTimer = 0.6f;
        hasHit = false;
        dashTimer = 8.0f;
        attackStep = 0;
        currentDashDist =
            1450.0f; // Velocidad fija y fluida (antes era ditancia dinámica)
      } else if (canJump && GetRandomValue(1, 100) <= 60) {
        aiState = AIState::ATTACK_JUMP;
        attackPhaseTimer = 1.35f; // +0.15s Telegraphing para saltos
        hasHit = false;
        jumpTimer = 12.0f;
        attackStep = 0;
      } else if (canSlam && GetRandomValue(1, 100) <= 60) {
        aiState = AIState::ATTACK_SLAM;
        attackPhaseTimer = 1.25f; // +0.15s Telegraphing para slams
        hasHit = false;
        slamTimer = 7.0f;
        attackStep = 0;
      } else if (canBasic) {
        aiState = AIState::ATTACK_BASIC;
        attackPhaseTimer =
            0.70f; // Slower for better Perfect Dodge reaction (was 0.45)
        hasHit = false;
      }
    } else {
      attackCooldown -= dt * (desperationTriggered ? 1.6f : aggressionLevel);
    }
    break;
  }

  case AIState::ATTACK_BASIC: {
    attackPhaseTimer -= dt;
    if (!hasHit) {
      float prog =
          CombatUtils::GetProgress(attackPhaseTimer, 0.70f); // Slower sweep
      if (prog > 1.0f)
        prog = 1.0f;
      // Barrido de 130 grados: desde facing - 65 hasta facing + 65.
      if (CombatUtils::CheckProgressiveSweep(position, facing, player.position,
                                             player.radius, 260.0f, -65.0f,
                                             130.0f, 1.0f, prog)) {
        player.TakeDamage(22.0f, {0, 0}); // Daño base equilibrado
        hasHit = true;
      }
    }

    if (attackPhaseTimer <= 0) {
      stateTimer = 0.25f; // Recuperación más rápida tras el golpe
      attackStep++;
      if (attackStep < 2) {
        aiState = AIState::IDLE;
        attackCooldown = 0.05f; // Siguiente golpe casi inmediato
      } else {
        attackStep = 0;
        int r = GetRandomValue(1, 100);
        if (r <= 33) {
          aiState = AIState::ATTACK_HEAVY;
          attackPhaseTimer = 1.1f; // Slower heavy startup (was 0.9f)
          hasHit = false;
        } else if (r <= 66) {
          aiState = AIState::ATTACK_ROCKS;
          attackPhaseTimer = 1.0f;
          rocksSpawned = 0;
          rockSpawnTimer = 0.0f;
          screenShake = fmaxf(screenShake, 0.7f); // Reducido (era 1.0)
        } else {
          aiState = AIState::IDLE;
          attackCooldown = 0.45f;
        }
      }
    }
    break;
  }

  case AIState::ATTACK_DASH: {
    if (attackPhaseTimer > 0) {
      attackPhaseTimer -= dt;
      if (attackPhaseTimer <= 0)
        stateTimer =
            0.50f; // Tiempo extendido para embestida más suave (antes 0.35)
    } else {
      stateTimer -= dt;
      velocity = Vector2Scale(facing, currentDashDist);

      // Radio ampliado (140) para compensar la menor velocidad
      if (!hasHit &&
          CombatUtils::CheckProgressiveRadial(position, player.position,
                                              player.radius, 140.0f, 1.0f)) {
        player.TakeDamage(55.0f, {0, 0}); // Daño castigador por fallar esquiva
        hasHit = true;
        screenShake = fmaxf(screenShake, 2.0f); // Reducido y usando fmaxf
        player.stunTimer = 0.5f;
      }
      if (stateTimer <= 0) {
        velocity = {0, 0};
        if (dashCharges < 1) {
          dashCharges++;
          attackPhaseTimer = 0.5f; // Segunda embestida tiene 0.5s de window
          hasHit = false;
          currentDashDist =
              1650.0f; // Segunda embestida levemente mas rapida pero controlada
        } else {
          aiState = AIState::IDLE;
          dashCharges = 0;
          stateTimer =
              0.6f; // Premio al jugador para que ataque tras esquivar 2 veces
          attackCooldown = 1.5f;
        }
      }
    }
    break;
  }

  case AIState::ATTACK_SLAM: {
    if (attackPhaseTimer > 0) {
      attackPhaseTimer -= dt;
      if (attackPhaseTimer < 0.4f)
        screenShake = fmaxf(screenShake, 0.3f); // Reducido (era 0.4)
    } else if (!hasHit) {
      if (CombatUtils::CheckProgressiveRadial(position, player.position,
                                              player.radius, 350.0f, 1.0f)) {
        player.TakeDamage(40.0f, {0, 0}); // + Daño
        player.slowTimer = 1.5f;
        Vector2 sd = Vector2Subtract(player.position, position);
        float sid = Vector2Length({sd.x, sd.y * 2.0f});
        float prox = 1.0f - (sid / 350.0f);
        if (prox < 0)
          prox = 0;
        screenShake = fmaxf(
            screenShake,
            1.2f + 1.8f * prox); // Reducido (era 1.8 + 2.5) y usando fmaxf
      }
      hasHit = true;
      stateTimer = 0.8f;
    } else {
      stateTimer -= dt;
      if (stateTimer <= 0) {
        aiState = AIState::IDLE;
        stateTimer = 0.65f; // +0.15s de respiro
        attackCooldown = 1.3f;
      }
    }
    break;
  }

  case AIState::ATTACK_HEAVY: {
    attackPhaseTimer -= dt;

    if (!hasHit) {
      float prog = CombatUtils::GetProgress(attackPhaseTimer, 1.1f) *
                   0.95f; // Slower hitbox progression (was 0.9, 1.4f)
      if (prog > 1.0f)
        prog = 1.0f;
      // Estocada frontal potente (30 grados, radio 330)
      if (CombatUtils::CheckProgressiveThrust(position, facing, player.position,
                                              player.radius, 330.0f, 15.0f,
                                              prog)) {
        player.TakeDamage(35.0f, {0, 0}); // Corregido el daño absurdamente bajo
        hasHit = true;
        screenShake =
            fmaxf(screenShake, 1.0f); // Reducido (era 1.5) y usando fmaxf
        Vector2 tip = Vector2Add(position, Vector2Scale(facing, 330.0f * prog));
        Graphics::SpawnImpactBurst(tip, facing, ORANGE, {255, 120, 0, 255}, 10,
                                   4);
      }
    }

    if (attackPhaseTimer <= 0) {
      if (!hasHit) {
        // Fallo visual al final del alcance si no dio
        Vector2 tip = Vector2Add(position, Vector2Scale(facing, 330.0f));
        Graphics::SpawnImpactBurst(tip, facing, ORANGE, {255, 120, 0, 255}, 10,
                                   4);
        hasHit = true;
      }
      stateTimer = 0.75f; // +0.15s de respiro
      aiState = AIState::IDLE;
      attackCooldown = 0.95f;
    }
    break;
  }

  case AIState::ATTACK_JUMP: {
    if (attackPhaseTimer > 0) {
      attackPhaseTimer -= dt;
      Vector2 diff = Vector2Subtract(player.position, position);
      if (Vector2Length(diff) > 20.0f && attackPhaseTimer > 0.3f)
        velocity = Vector2Add(
            velocity, Vector2Scale(Vector2Normalize(diff), 800.0f * dt));
    } else if (!hasHit) {
      screenShake = fmaxf(screenShake, 1.8f); // Reducido (era 2.5)
      hasHit = true;
      if (CombatUtils::CheckProgressiveRadial(position, player.position,
                                              player.radius, 300.0f, 1.0f)) {
        Vector2 diff = Vector2Subtract(player.position, position);
        player.TakeDamage(50.0f, Vector2Scale(Vector2Normalize(diff), 2800.0f));
      }
      // Explosion de impacto al aterrizar
      for (int i = 0; i < 30; i++)
        Graphics::VFXSystem::GetInstance().SpawnParticleEx(
            position,
            {(float)GetRandomValue(-500, 500),
             (float)GetRandomValue(-500, 500)},
            0.6f, RED, 5.0f);
    } else {
      stateTimer -= dt;
      velocity = Vector2Scale(velocity, 0.5f);
      if (stateTimer <= 0) {
        aiState = AIState::IDLE;
        attackCooldown = baseAttackCooldown;
      }
    }
    break;
  }

  case AIState::ATTACK_ROCKS: {
    attackPhaseTimer -= dt;
    if (attackPhaseTimer <= 0) {
      rocksToSpawn = 5;
      stateTimer = 0.4f;
      aiState = AIState::CHASE;
      attackCooldown = 0.5f;
    }
    break;
  }

  case AIState::EVADE: {
    stateTimer -= dt;
    velocity = Vector2Scale(evadeDir, 1000.0f); // Aumentado (era 800)
    if (GetRandomValue(1, 100) <= 40)
      Graphics::SpawnDashTrail(position);
    Vector2 d = Vector2Subtract(player.position, position);
    if (Vector2Length(d) > 0)
      facing = Vector2Normalize(d);
    if (stateTimer <= 0) {
      aiState = AIState::CHASE;
      velocity = {0, 0};
      attackCooldown = 0.2f;
    }
    break;
  }

  case AIState::AVALANCHE_START: {
    Vector2 target = {Arena::CENTER_X,
                      Arena::CENTER_Y -
                          Arena::RADIUS * 0.3f}; // Esquina Norte dinámica
    Vector2 diff = Vector2Subtract(target, position);
    float d = Vector2Length(diff);
    if (d < 50.0f) { // Tolerancia aumentada
      aiState = AIState::AVALANCHE_ACTIVE;
      avalancheTimer = 11.0f; // 1s extra para animacion final
      waveSpawnTimer = 0.5f;  // Primer golpe rapido
      facing = {0, 1};        // Mirando al centro del mapa
    } else {
      velocity = Vector2Scale(Vector2Normalize(diff), 800.0f);
      facing = Vector2Normalize(diff);
    }
    break;
  }

  case AIState::AVALANCHE_ACTIVE: {
    velocity = {0, 0};
    avalancheTimer -= dt;
    waveSpawnTimer -= dt;

    if (waveSpawnTimer <= 0 && avalancheTimer > 1.0f) {
      waveSpawnTimer = 1.6f; // Frecuencia mucho mayor
      // Spawn Wave
      if (waveCount < 10) {
        waves[waveCount] = {position, 0.0f, 680.0f, true,
                            false}; // Velocidad letal
        waveCount++;
      }
      screenShake = fmaxf(screenShake, 2.8f);
      // VFX de impacto en el suelo
      for (int i = 0; i < 20; i++)
        Graphics::VFXSystem::GetInstance().SpawnParticleEx(
            position,
            {(float)GetRandomValue(-500, 500),
             (float)GetRandomValue(-500, 500)},
            0.7f, MAROON, 8.0f);
    }

    // Actualizar ondas de choque -- MOVIDO AL BUCLE GLOBAL (arriba)

    if (avalancheTimer <= 0) {
      aiState = AIState::IDLE;
      attackCooldown = 1.5f;
      waveCount = 0; // Limpiar buffer
    }
    break;
  }

  case AIState::STAGGERED: {
    stateTimer -= dt;
    if (stateTimer <= 0) {
      aiState = AIState::IDLE;
      stateTimer = 0.3f;
      for (int i = 0; i < 8; i++)
        Graphics::VFXSystem::GetInstance().SpawnParticleEx(
            position,
            {(float)GetRandomValue(-200, 200),
             (float)GetRandomValue(-200, 200)},
            0.5f, WHITE, 4.0f);
    }
    break;
  }

  case AIState::CENTER_ROCKS_START: {
    Vector2 target = {Arena::CENTER_X, Arena::CENTER_Y};
    Vector2 diff = Vector2Subtract(target, position);
    if (Vector2Length(diff) < 50.0f) {
      aiState = AIState::CENTER_ROCKS_LIFT;
      phaseTimer = 2.0f;
      thrownRockCount = 0;
    } else {
      velocity = Vector2Scale(Vector2Normalize(diff), 1000.0f);
      facing = Vector2Normalize(diff);
    }
    break;
  }

  case AIState::CENTER_ROCKS_LIFT: {
    velocity = {0, 0};
    phaseTimer -= dt;
    if (phaseTimer <= 0) {
      aiState = AIState::CENTER_ROCKS_THROW;
      thrownRockCount = 8;
      rockThrowDelay = 0.2f;
    }
    break;
  }

  case AIState::CENTER_ROCKS_THROW: {
    velocity = {0, 0};

    if (thrownRockCount > 0) {
      rockThrowDelay -= dt;
      if (rockThrowDelay <= 0) {
        // Lógica de sub-ráfaga: dispara 2 rocas con 0.2s entre ellas
        if (burstShotsLeft <= 0) {
          burstShotsLeft = (thrownRockCount >= 2) ? 2 : 1;
        }

        if (burstShotsLeft > 0) {
          subBurstTimer -= dt;
          if (subBurstTimer <= 0) {
            subBurstTimer = 0.20f; // 0.2s entre disparos de la pareja

            int idx = thrownRockCount - 1;
            thrownRocks[idx].active = true;
            thrownRocks[idx].position = position;

            // Apuntar al jugador en el milisegundo exacto del disparo
            Vector2 toPlayer = Vector2Subtract(player.position, position);
            Vector2 aimDir = (Vector2Length(toPlayer) > 0)
                                 ? Vector2Normalize(toPlayer)
                                 : facing;

            thrownRocks[idx].direction = aimDir;
            thrownRocks[idx].speed = 1150.0f; // Un poco más rápido pero recto

            thrownRockCount--;
            burstShotsLeft--;

            Graphics::SpawnImpactBurst(position, aimDir, RED, WHITE, 8, 5.0f);
            screenShake = fmaxf(screenShake, 0.5f);

            if (burstShotsLeft == 0) {
              rockThrowDelay = 0.85f; // Pausa larga tras soltar las dos rocas
              subBurstTimer = 0.0f;   // Reset para la siguiente pareja
            }
          }
        }
      }
    } else {
      aiState = AIState::IDLE;
      attackCooldown = 1.0f;
      burstShotsLeft = 0;
    }
    break;
  }

  case AIState::DESPERATION_START: {
    // Moverse al centro, invulnerable mientras se desplaza
    Vector2 target = {2000.0f, 2000.0f};
    Vector2 diff = Vector2Subtract(target, position);
    if (Vector2Length(diff) < 50.0f) {
      velocity = {0, 0};
      aiState = AIState::DESPERATION_ACTIVE;
      waveSpawnTimer = 0.8f;
      desperationWavesFired = 0;
      desperationBombBurstsDone = 0;
      desperationBombTimer = 999.0f; // Bombas NO empiezan hasta el CHASE
      waveCount = 0;                 // Limpiar ondas anteriores
      facing = {0, 1};
    } else {
      velocity = Vector2Scale(Vector2Normalize(diff), 1300.0f);
      facing = Vector2Normalize(diff);
    }
    break;
  }

  case AIState::DESPERATION_ACTIVE: {
    // Invulnerable durante toda esta fase (cubierto por IsInvulnerable
    // override)
    velocity = {0, 0};
    waveSpawnTimer -= dt;

    // ── Lanzar exactamente 3 ondas sísmicas desde el centro (idénticas al 50%)
    // ──
    if (waveSpawnTimer <= 0.0f && desperationWavesFired < 3) {
      waveSpawnTimer = 1.7f;
      desperationWavesFired++;

      if (waveCount < 10) {
        waves[waveCount] = {position, 0.0f, 850.0f, true, false};
        waveCount++;
      }
      screenShake = fmaxf(screenShake, 4.5f);
      for (int i = 0; i < 30; i++)
        Graphics::VFXSystem::GetInstance().SpawnParticleEx(
            position,
            {(float)GetRandomValue(-700, 700),
             (float)GetRandomValue(-700, 700)},
            1.0f, RED, 10.0f);
    }

    // Transición a CHASE cuando las 3 ondas fueron lanzadas y el último timer
    // expiró
    if (desperationWavesFired >= 3 && waveSpawnTimer <= 0.0f) {
      // OTORGAR RESISTENCIAS permanentes
      desperationResists = true; // 50% reduccion de daño
      isBleeding = false;        // Limpiar sangrado activo
      bleedTimer = 0.0f;
      bleedTotalDamage = 0.0f;
      staticStacks = 0; // Inmune a elementos

      aiState = AIState::CHASE;
      attackCooldown = 0.8f;
      desperationBombTimer = 0.5f; // Bombas empiezan en CHASE
    }
    break;
  }
  }
}

// ── Enemy::Update ────────────────────────────────────────────────────
void Enemy::Update() {
  // Mitigación de daño: 50% una vez que el Golem gana resistencias en la fase
  // final
  if (hp < previousHp && desperationResists) {
    float lost = previousHp - hp;
    hp += lost * 0.5f; // Mitiga el 50%
  }
  previousHp = hp;

  if (hp <= 0 && !isDead && !isDying) {
    isDying = true;
    deathAnimTimer = 1.6f;
    aiState = AIState::IDLE;
    // Detenemos su ataque actual, sangrado y temporizadores
    attackPhaseTimer = 0;
    isBleeding = false;
  }

  if (isDead) {
    respawnTimer -= GetFrameTime() * g_timeScale;
    if (respawnTimer <= 0) {
      isDead = false;
      isDying = false;
      hp = maxHp;
      position = spawnPos;
      velocity = {0, 0};
    }
    return;
  }

  if (isDying) {
    deathAnimTimer -= GetFrameTime() * g_timeScale;
    velocity = {0, 0}; // [NEW] Parada absoluta al empezar animación de muerte
    if (deathAnimTimer <= 0) {
      isDying = false;
      isDead = true;
      respawnTimer = 15.0f;
    }
    return;
  }
  if (recentDamageTimer > 0) {
    recentDamageTimer -= GetFrameTime() * g_timeScale;
    if (recentDamageTimer <= 0)
      recentDamage = 0;
  }

  // Regen de Poise
  if (poiseRegenTimer > 0) {
    poiseRegenTimer -= GetFrameTime() * g_timeScale;
  } else {
    if (poiseCurrent < poiseMax) {
      poiseCurrent += (poiseMax * 0.1f) *
                      (GetFrameTime() * g_timeScale); // 10s back to full
      if (poiseCurrent > poiseMax)
        poiseCurrent = poiseMax;
    }
  }

  Vector2 next = Vector2Add(
      position, Vector2Scale(velocity, GetFrameTime() * g_timeScale));
  position = Arena::GetClampedPos(next, radius);
  velocity = Vector2Scale(velocity, 0.85f);
}

// ── Enemy::Draw ──────────────────────────────────────────────────────
void Enemy::Draw() {
  if (isDead)
    return;

  float jumpH = 0.0f;
  if (aiState == AIState::ATTACK_JUMP && attackPhaseTimer > 0) {
    float progress = fmaxf(0.0f, fminf(1.0f, 1.0f - (attackPhaseTimer / 1.2f)));
    jumpH = sinf(progress * PI) * 450.0f;
  }

  bool telegraphing =
      (attackPhaseTimer > 0 && aiState != AIState::IDLE &&
       aiState != AIState::CHASE && aiState != AIState::STAGGERED &&
       aiState != AIState::EVADE);

  Color tint = WHITE;
  if (hitFlashTimer > 0) {
    tint = WHITE;
    color = WHITE;
  } else if (IsInvulnerable()) {
    // Brillo dorado para indicar invulnerabilidad durante Avalancha
    tint = Color{255, 235, 120, 255};
    color = GOLD;
  } else if (telegraphing) {
    // Quitamos el destello rojo del cuerpo, solo mantenemos el color de la
    // hitbox si se usa
    tint = WHITE;
    color = MAROON;
  } else {
    color = MAROON;
  }

  DrawEllipse((int)position.x, (int)position.y, radius, radius * 0.5f,
              Fade(BLACK, 0.4f));

  // ── Sprite del Golem ─────────────────────────────────────────────
  if (ResourceManager::texEnemy.id != 0) {
    // GIF basado: Identificamos frames por estado
    int totalFrames = ResourceManager::texEnemyFrames;
    int currentFrame = 0;

    if (isDying) {
      float prog = 1.0f - (deathAnimTimer / 1.6f);
      prog = fmaxf(0.0f, fminf(prog, 0.99f));
      // Últimos 7 frames del spritesheet para la animación de muerte
      int startFrame = totalFrames - 7;
      int endFrame = totalFrames - 1;
      currentFrame = startFrame + (int)(prog * (endFrame - startFrame + 1));

      // La Muerte desvanece el color gradualmente al morir
      color = Fade({80, 80, 80, 255}, 1.0f - (prog * 0.5f));
      tint = color;
    } else {
      // Mapeo simple de estados a rangos de frames (estimado)
      float prog = 0.0f;
      int startFrame = 0;
      int endFrame = 0;

      if (aiState == AIState::IDLE) {
        startFrame = 0;
        endFrame = (int)(totalFrames * 0.15f);
        prog = fmodf((float)g_gameTime * 0.4f, 1.0f); // Velocidad reducida
      } else if (aiState == AIState::CHASE || aiState == AIState::EVADE ||
                 aiState == AIState::AVALANCHE_START) {
        startFrame = (int)(totalFrames * 0.16f);
        endFrame = (int)(totalFrames * 0.35f);
        float animSpeed = (aiState == AIState::AVALANCHE_START) ? 1.0f : 0.6f;
        prog = fmodf((float)g_gameTime * animSpeed, 1.0f);
      } else if (aiState == AIState::STAGGERED) {
        startFrame = (int)(totalFrames * 0.36f);
        endFrame = (int)(totalFrames * 0.45f);
        prog = 0.5f; // frame estatico de hit
      } else if (aiState == AIState::AVALANCHE_ACTIVE) {
        // Animación de slam para la avalancha
        startFrame = (int)(totalFrames * 0.46f);
        endFrame = totalFrames - 1;
        // Ciclar el golpe cada vez que se genera una onda
        prog = 1.0f - (waveSpawnTimer / 3.0f);
        if (prog < 0)
          prog = 0;
        if (prog > 0.99f)
          prog = 0.99f;
      } else {
        // Estados de ataque
        startFrame = (int)(totalFrames * 0.46f);
        endFrame = totalFrames - 1;
        // Progreso basado en el timer de fase de ataque
        float maxT = 1.0f;
        if (aiState == AIState::ATTACK_BASIC)
          maxT = 0.6f;
        else if (aiState == AIState::ATTACK_DASH)
          maxT = 0.7f;
        else
          maxT = 1.2f;
        prog = 1.0f - (attackPhaseTimer / maxT);
      }

      if (endFrame < startFrame)
        endFrame = startFrame;
      prog = fmaxf(0.0f, fminf(prog, 0.99f));
      currentFrame = startFrame + (int)(prog * (endFrame - startFrame + 1));
    }

    if (totalFrames > 0)
      currentFrame = currentFrame % totalFrames;

    // [NEW] Dibujado del Sprite del Golem
    Rectangle src = {(float)currentFrame *
                         (ResourceManager::texEnemy.width / totalFrames),
                     0, (float)ResourceManager::texEnemy.width / totalFrames,
                     (float)ResourceManager::texEnemy.height};
    Rectangle dest = {position.x, position.y - 30.0f - jumpH, radius * 4.5f,
                      radius * 4.5f}; // Escalado para el boss
    Vector2 origin = {dest.width / 2.0f,
                      dest.height / 1.1f}; // Ajuste de pivote a los pies

    DrawTexturePro(ResourceManager::texEnemy, src, dest, origin, 0.0f, tint);

  } else {
    // Fallback: Primitive-based rendering for all enemies
    Vector2 snapped = Directions::GetSnappedVector(facing);

    // Anillo de base
    DrawCircleLines((int)position.x, (int)position.y - 30.0f - jumpH,
                    radius + 8, Fade(RED, 0.4f));

    // Puntero 8-way
    Vector2 pointerPos = Vector2Add({position.x, position.y - 30.0f - jumpH},
                                    Vector2Scale(snapped, radius + 15.0f));
    DrawCircleV(pointerPos, 5.0f, WHITE);

    DrawCircleV({position.x, position.y - 30.0f - jumpH}, radius, color);
  }

  // ── Indicadores de telegrafos de ataque ──────────────────────────
  if (aiState == AIState::ATTACK_SLAM && attackPhaseTimer > 0) {
    float prog = 1.0f - (attackPhaseTimer / 1.2f);
    float pulse = 0.5f + 0.4f * sinf((float)g_gameTime * 15.0f) * prog;
    DrawEllipse(position.x, position.y, 350.0f, 350.0f * 0.5f,
                Fade(RED, prog * 0.3f + 0.1f));
    DrawEllipseLines(position.x, position.y, 350.0f, 350.0f * 0.5f,
                     Fade(YELLOW, pulse));
    DrawEllipseLines(position.x, position.y, 350.0f * (1 - prog),
                     350.0f * (1 - prog) * 0.5f, Fade(ORANGE, 0.9f));
    // Brillo adicional en el borde exterior
    DrawEllipseLines(position.x, position.y, 351.0f, 351.0f * 0.5f,
                     Fade(RED, 0.4f));

  } else if (aiState == AIState::ATTACK_HEAVY && attackPhaseTimer > 0) {
    float prog = CombatUtils::GetProgress(attackPhaseTimer, 1.1f) *
                 0.95f; // Slower visual progression (was 0.6f, no multiplier)
    if (prog > 1.0f)
      prog = 1.0f;
    float ang = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
    rlDrawRenderBatchActive();
    rlPushMatrix();
    rlTranslatef(position.x, position.y, 0);
    rlScalef(1, 0.5f, 1);
    DrawCircleSector({0, 0}, 330.0f, ang - 15.0f, ang + 15.0f, 24,
                     Fade(ORANGE, 0.25f));
    DrawCircleSectorLines({0, 0}, 330.0f * prog, ang - 15.0f, ang + 15.0f, 24,
                          Fade(YELLOW, 0.6f + 0.4f * prog));
    DrawCircleSectorLines({0, 0}, 330.0f, ang - 15.0f, ang + 15.0f, 24,
                          Fade(ORANGE, 0.4f));
    rlDrawRenderBatchActive();
    rlPopMatrix();

  } else if (aiState == AIState::ATTACK_BASIC && attackPhaseTimer > 0) {
    float prog = CombatUtils::GetProgress(attackPhaseTimer, 0.55f) *
                 0.95f; // Slower visual progression (was 0.2f, no multiplier)
    if (prog > 1.0f)
      prog = 1.0f;
    float ang = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
    rlDrawRenderBatchActive();
    rlPushMatrix();
    rlTranslatef(position.x, position.y, 0);
    rlScalef(1, 0.5f, 1);
    DrawCircleSector({0, 0}, 260.0f, ang - 65.0f, ang + 65.0f, 32,
                     Fade(RED, 0.2f));
    // Dibuja el barrido con un color más vivo
    DrawCircleSectorLines({0, 0}, 260.0f, ang - 65.0f,
                          ang - 65.0f + (130.0f * prog), 32,
                          Fade(YELLOW, 0.5f + 0.5f * prog));
    DrawCircleSectorLines({0, 0}, 260.0f, ang - 65.0f, ang + 65.0f, 32,
                          Fade(RED, 0.3f));
    rlDrawRenderBatchActive();
    rlPopMatrix();

  } else if (aiState == AIState::ATTACK_DASH && attackPhaseTimer > 0) {
    float prog = 1.0f - (attackPhaseTimer / 0.7f);
    float ang = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
    rlDrawRenderBatchActive();
    rlPushMatrix();
    rlTranslatef(position.x, position.y, 0);
    rlScalef(1, 0.5f, 1);
    DrawRectanglePro({0, -50, currentDashDist, 100}, {0, 50}, ang,
                     Fade(PURPLE, 0.25f));
    DrawRectanglePro({0, -50, currentDashDist * prog, 100}, {0, 50}, ang,
                     Fade(VIOLET, 0.7f));
    // Punto brillante en el origen
    DrawCircleV({0, 0}, 15 * (1.0f - prog), Fade(WHITE, 0.5f));
    rlDrawRenderBatchActive();
    rlPopMatrix();

  } else if (aiState == AIState::ATTACK_JUMP) {
    rlDrawRenderBatchActive();
    rlPushMatrix();
    rlTranslatef(position.x, position.y, 0);
    rlScalef(1, 0.5f, 1);
    if (attackPhaseTimer > 0) {
      float p = 1.0f - (attackPhaseTimer / 1.0f);
      DrawCircleV({0, 0}, 300, Fade(RED, p * 0.4f));
      DrawCircleLines(0, 0, 300, Fade(WHITE, 0.5f));
    } else {
      float wp = 1.0f - (stateTimer / 0.6f);
      DrawCircleLines(0, 0, 300 * wp, Fade(WHITE, 1 - wp));
      DrawCircleLines(0, 0, 300 * wp + 15, Fade(RED, 1 - wp));
    }
    rlDrawRenderBatchActive();
    rlPopMatrix();
  }

  // ── Renderizado de Ondas Sismicas (Avalancha) ─────────────────────
  if (aiState == AIState::AVALANCHE_ACTIVE || waveCount > 0) {
    rlDrawRenderBatchActive();
    rlPushMatrix();
    rlTranslatef(position.x, position.y,
                 0); // O usar el centro de cada wave si es distinto
    rlScalef(1, 0.5f, 1);

    for (int i = 0; i < waveCount; i++) {
      if (!waves[i].active)
        continue;

      Vector2 visualCenter = Vector2Subtract(waves[i].center, position);
      float alpha = 1.0f - (waves[i].radius / 2200.0f);
      if (alpha < 0)
        alpha = 0;

      // Efecto de brillo de la onda (Hitbox resaltada y exacta)
      float collisionWidth = 42.0f; // Un poco más grueso para visibilidad

      // Relleno de la zona de peligro (Ring filling)
      for (float r_off = -collisionWidth; r_off <= collisionWidth;
           r_off += 3.0f) {
        float r_current = waves[i].radius + r_off;
        if (r_current < 0)
          continue;

        float intensity = 1.0f - (fabsf(r_off) / collisionWidth);
        // Color ROJO NEON para que resalte (255, 20, 20)
        Color waveColor = {255, 20, 20, 255};
        if (fabsf(r_off) < 6.0f)
          waveColor = WHITE; // El centro exacto de la hitbox brilla en blanco

        DrawCircleLinesV(visualCenter, r_current,
                         Fade(waveColor, alpha * 0.5f * intensity));
      }

      // Bordes de alta fidelidad
      DrawCircleLinesV(visualCenter, waves[i].radius + collisionWidth,
                       Fade(RED, alpha * 0.8f));
      DrawCircleLinesV(visualCenter, waves[i].radius - collisionWidth,
                       Fade(RED, alpha * 0.8f));

      // Pulso central de la hitbox (Hitbox Checker Visual)
      float pulse = 0.8f + 0.2f * sinf(g_gameTime * 25.0f);
      DrawCircleLinesV(visualCenter, waves[i].radius,
                       Fade(WHITE, alpha * pulse));
      DrawCircleLinesV(visualCenter, waves[i].radius + 2.0f,
                       Fade(YELLOW, alpha * pulse));
    }

    rlDrawRenderBatchActive();
    rlPopMatrix();
  }

  // ── Rocas cayendo ─────────────────────────────────────────────────
  for (int i = 0; i < rocksSpawned; i++) {
    if (!rocks[i].active)
      continue;
    float prog = 1.0f - rocks[i].fallTimer;
    rlDrawRenderBatchActive();
    rlPushMatrix();
    rlTranslatef(rocks[i].position.x, rocks[i].position.y, 0);
    rlScalef(1, 0.5f, 1);
    DrawCircleV({0, 0}, 60 * prog, Fade(BLACK, 0.5f + 0.3f * prog));
    DrawCircleLines(0, 0, 60, Fade(RED, 0.7f));
    rlDrawRenderBatchActive();
    rlPopMatrix();
  }

  // ── Rocas Misiles (Fase 75%) ──────────────────────────────────────
  for (int i = 0; i < 8; i++) {
    if (thrownRocks[i].active || (aiState == AIState::CENTER_ROCKS_LIFT) ||
        (aiState == AIState::CENTER_ROCKS_THROW && i < thrownRockCount)) {
      Vector2 pos = thrownRocks[i].position;
      rlDrawRenderBatchActive();
      rlPushMatrix();
      rlTranslatef(pos.x, pos.y, 0);
      rlScalef(1, 0.5f, 1);

      float drawRadius = 35.0f;
      float liftAlpha = 1.0f;
      if (aiState == AIState::CENTER_ROCKS_LIFT) {
        liftAlpha = 1.0f - (phaseTimer / 2.0f);
        if (liftAlpha > 1.0f)
          liftAlpha = 1.0f;
        pos.y -= liftAlpha * 40.0f; // Levantar
      }

      DrawCircleV({0, 0}, drawRadius * 1.5f,
                  Fade(BLACK, 0.4f * liftAlpha)); // Sombra
      DrawCircleV({0, -20.0f}, drawRadius, Fade(DARKBROWN, liftAlpha));
      DrawCircleLines(0, -20.0f, drawRadius, Fade(ORANGE, liftAlpha * 0.8f));

      // Estela de misil
      if (thrownRocks[i].active) {
        DrawCircleV({-thrownRocks[i].direction.x * 40.0f,
                     -thrownRocks[i].direction.y * 40.0f},
                    15.0f, Fade(ORANGE, 0.6f));
        DrawCircleV({-thrownRocks[i].direction.x * 20.0f,
                     -thrownRocks[i].direction.y * 20.0f},
                    25.0f, Fade(YELLOW, 0.4f));
      }

      rlDrawRenderBatchActive();
      rlPopMatrix();
    }
  }

  // ── Bombardeo de Desesperación (Fase 10%) ─────────────────────────
  for (int i = 0; i < 5; i++) {
    if (!desperationBombs[i].active)
      continue;
    float prog = 1.0f - (desperationBombs[i].delay / 1.2f);
    if (prog < 0)
      prog = 0;
    if (prog > 1)
      prog = 1;

    rlDrawRenderBatchActive();
    rlPushMatrix();
    rlTranslatef(desperationBombs[i].position.x, desperationBombs[i].position.y,
                 0);
    rlScalef(1, 0.5f, 1);

    DrawCircleV({0, 0}, desperationBombs[i].radius,
                Fade(RED, 0.2f + 0.3f * prog));
    DrawCircleLines(0, 0, desperationBombs[i].radius, Fade(RED, 0.6f));

    // Circulo que se encoje
    DrawCircleLines(0, 0, desperationBombs[i].radius * (1.0f - prog),
                    Fade(WHITE, 0.8f));

    // Cruz central
    DrawLine(-15, 0, 15, 0, Fade(YELLOW, 0.9f));
    DrawLine(0, -15, 0, 15, Fade(YELLOW, 0.9f));

    rlDrawRenderBatchActive();
    rlPopMatrix();
  }
}

void Enemy::TakeDamage(float dmg, float poiseDmg, Vector2 pushVel) {
  if (isDead || isDying || IsInvulnerable())
    return;

  if (!desperationResists && aiState != AIState::DESPERATION_ACTIVE &&
      aiState != AIState::AVALANCHE_ACTIVE) {
    if (aiState != AIState::STAGGERED) {
      poiseCurrent -= poiseDmg;
      poiseRegenTimer = 5.0f; // 5 seconds before regen starts

      if (poiseCurrent <= 0) {
        poiseCurrent = poiseMax;
        aiState = AIState::STAGGERED;
        stateTimer = 1.5f;
        attackPhaseTimer = 0.0f;
        // VFX for posture break
        Graphics::SpawnImpactBurst(position, {0, -1}, GOLD, WHITE, 25, 10);
        screenShake = fmaxf(screenShake, 3.0f);
      }
    }
  }

  hp -= dmg;
  velocity = Vector2Add(velocity, pushVel);
  recentDamage += dmg;
  recentDamageTimer = 1.0f;
  hitFlashTimer = 0.15f;
}

void Enemy::ScaleDifficulty(int wave) {
  maxHp *= (1.0f + wave * 0.35f);
  hp = maxHp;
  aggressionLevel *= (1.0f + wave * 0.15f); // Se vuelve más rápido y agresivo
}
