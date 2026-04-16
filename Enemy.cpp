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
#include "include/graphics/VFXSystem.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>

extern float g_timeScale;
extern double g_gameTime;
extern float screenShake;
// Eliminado a favor de usar CombatUtils::CheckProgressiveSweep / Thrust / Radial
// para cumplir con la arquitectura progresiva universal para todos los enemigos.

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

  // Siempre mirar al jugador excepto durante el 'active frame' del ataque (User requested auto-aim fix)
  bool isAttacking = (aiState == AIState::ATTACK_BASIC || aiState == AIState::ATTACK_HEAVY || 
                      aiState == AIState::ATTACK_SLAM || aiState == AIState::ATTACK_JUMP || 
                      aiState == AIState::ATTACK_ROCKS || aiState == AIState::ATTACK_DASH ||
                      aiState == AIState::AVALANCHE_START || aiState == AIState::AVALANCHE_ACTIVE);

  bool canRotate = !isAttacking;
  if (aiState == AIState::ATTACK_BASIC && attackPhaseTimer > 0.08f) canRotate = true;
  if (aiState == AIState::ATTACK_HEAVY && attackPhaseTimer > 0.20f) canRotate = true;
  if (aiState == AIState::ATTACK_SLAM && attackPhaseTimer > 0.25f) canRotate = true;
  if (aiState == AIState::ATTACK_DASH && attackPhaseTimer > 0.15f) canRotate = true;
  if (aiState == AIState::ATTACK_JUMP && attackPhaseTimer > 0.22f) canRotate = true;

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
  } else if (aiState == AIState::CHASE || aiState == AIState::ORBITING ||
             aiState == AIState::EVADE || aiState == AIState::ATTACK_DASH) {
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
        if (!player.IsImmune() &&
            sqrtf(diff.x * diff.x + isoY * isoY) < 60.0f + player.radius) {
          player.hp -= 24.2f;      // Dano de la roca (+20% total)
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

  switch (aiState) {
  case AIState::IDLE:
    stateTimer -= dt;
    if (stateTimer <= 0)
      aiState = AIState::CHASE;
    break;

  case AIState::CHASE:
  case AIState::ORBITING: {
    Vector2 diff = Vector2Subtract(player.position, position);
    float dist = Vector2Length(diff);
    if (dist > 0)
      facing = Vector2Normalize(diff);
    
    // ── Trigger Avalancha (25% HP) ──────────────────────────────────
    if (!avalancheTriggered && hp <= maxHp * 0.25f) {
        avalancheTriggered = true;
        aiState = AIState::AVALANCHE_START;
        stateTimer = 0.0f;
        waveCount = 0; // Reset wave buffer
        return;
    }

    if (dist > 350.0f)
      aiState = AIState::CHASE; // Se acerca mas antes de orbitar
    else if (dist > 150.0f) {
      if (aiState != AIState::ORBITING) {
        aiState = AIState::ORBITING;
        orbitDir = GetRandomValue(0, 1) ? 1 : -1;
        orbitAngle = atan2f(diff.y, diff.x);
      }
    } else {
      aiState = AIState::CHASE;
    }

    if (aiState == AIState::CHASE) {
      if (dist > 180.0f)
        velocity = Vector2Add(velocity, Vector2Scale(facing, 1650.0f * dt)); // Velocidad de persecución aumentada (era 1400)
    } else {
      orbitAngle += orbitDir * 1.5f * dt;
      Vector2 tgt = {player.position.x - cosf(orbitAngle) * 350.0f,
                     player.position.y - sinf(orbitAngle) * 350.0f};
      Vector2 md = Vector2Normalize(Vector2Subtract(tgt, position));
      velocity = Vector2Add(velocity, Vector2Scale(md, 1800.0f * dt)); // Velocidad de orbitación aumentada (era 1500)
    }

    if (attackCooldown <= 0.0f) {
      bool canJump = (jumpTimer <= 0 && dist <= 200.0f);
      bool canSlam = (slamTimer <= 0 && dist <= 300.0f);
      bool canDash = (dashTimer <= 0 && dist > 260.0f && dist <= 750.0f);
      bool canBasic = (dist <= 260.0f);

      if (canJump) {
        aiState = AIState::ATTACK_JUMP;
        attackPhaseTimer = 1.5f; // Slower jump startup
        hasHit = false;
        jumpTimer = 15.0f;
        attackStep = 0;
      } else if (canSlam && GetRandomValue(1, 100) <= 60) {
        aiState = AIState::ATTACK_SLAM;
        attackPhaseTimer = 1.3f; // Slower slam startup
        hasHit = false;
        slamTimer = 8.0f;
        attackStep = 0;
      } else if (canDash && GetRandomValue(1, 100) <= 45) { // Mas agresividad con la embestida (45%)
        aiState = AIState::ATTACK_DASH;
        attackPhaseTimer = 0.7f;
        hasHit = false;
        dashTimer = 9.0f;
        attackStep = 0; // Mas cd de embestida
        currentDashDist = Clamp(dist, 180.0f, 700.0f);
      } else if (canBasic) {
        aiState = AIState::ATTACK_BASIC;
        attackPhaseTimer = 0.55f; // Slower basic startup (was 0.35f)
        hasHit = false;
      }
    } else {
      attackCooldown -= dt * aggressionLevel;
    }
    break;
  }

  case AIState::ATTACK_BASIC: {
    attackPhaseTimer -= dt;
    if (!hasHit && !player.IsImmune()) {
      float prog = CombatUtils::GetProgress(attackPhaseTimer, 0.55f) * 0.95f; // Slower hitbox progression (was 0.35, 1.4f)
      if (prog > 1.0f) prog = 1.0f;
      // Barrido de 130 grados: desde facing - 65 hasta facing + 65.
      if (CombatUtils::CheckProgressiveSweep(position, facing, player.position, player.radius, 260.0f, -65.0f, 130.0f, 1.0f, prog)) {
        player.hp -= 18.15f; // +20% total
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
      // El re-apuntado ahora se gestiona de forma global en la cabecera de UpdateAI
      // para mayor consistencia en todos los ataques.

      if (attackPhaseTimer <= 0)
        stateTimer = 0.35f; // Duracion original para dar tiempo a esquivar
    } else {
      stateTimer -= dt;
      velocity = Vector2Scale(facing, currentDashDist / 0.32f); 
      // La embestida usa Radial para comprobar daño mientras corre
      if (!hasHit && CombatUtils::CheckProgressiveRadial(position, player.position, player.radius, 100.0f, 1.0f)) {
        player.hp -= 43.56f;
        hasHit = true;
        screenShake = fmaxf(screenShake, 2.0f); // Reducido (era 3.0) y usando fmaxf
        player.stunTimer = 0.5f;
      }
      if (stateTimer <= 0) {
        velocity = {0, 0};
        if (dashCharges < 1) {
          dashCharges++;
          attackPhaseTimer = 0.5f; // Segunda embestida tiene 0.5s de window
          hasHit = false;
          currentDashDist *= 1.6f; // La segunda embestida es mas larga
        } else {
          aiState = AIState::IDLE;
          dashCharges = 0;
          stateTimer = 1.0f;
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
      if (!player.IsImmune() && CombatUtils::CheckProgressiveRadial(position, player.position, player.radius, 350.0f, 1.0f)) {
        player.hp -= 36.3f; // +20% total
        player.slowTimer = 1.5f;
        Vector2 sd = Vector2Subtract(player.position, position);
        float sid = Vector2Length({sd.x, sd.y * 2.0f});
        float prox = 1.0f - (sid / 350.0f);
        if (prox < 0) prox = 0;
        screenShake = fmaxf(screenShake, 1.2f + 1.8f * prox); // Reducido (era 1.8 + 2.5) y usando fmaxf
      }
      hasHit = true;
      stateTimer = 0.8f;
    } else {
      stateTimer -= dt;
      if (stateTimer <= 0) {
        aiState = AIState::IDLE;
        stateTimer = 0.5f;
        attackCooldown = 1.2f;
      }
    }
    break;
  }

  case AIState::ATTACK_HEAVY: {
    attackPhaseTimer -= dt;
    
    if (!hasHit && !player.IsImmune()) {
      float prog = CombatUtils::GetProgress(attackPhaseTimer, 1.1f) * 0.95f; // Slower hitbox progression (was 0.9, 1.4f)
      if (prog > 1.0f) prog = 1.0f;
      // Estocada frontal potente (30 grados, radio 330)
      if (CombatUtils::CheckProgressiveThrust(position, facing, player.position, player.radius, 330.0f, 15.0f, prog)) {
        player.hp -= 12.1f; // +20% total
        hasHit = true;
        screenShake = fmaxf(screenShake, 1.0f); // Reducido (era 1.5) y usando fmaxf
        Vector2 tip = Vector2Add(position, Vector2Scale(facing, 330.0f * prog));
        Graphics::SpawnImpactBurst(tip, facing, ORANGE, {255, 120, 0, 255}, 10, 4);
      }
    }
    
    if (attackPhaseTimer <= 0) {
      if (!hasHit) {
        // Fallo visual al final del alcance si no dio
        Vector2 tip = Vector2Add(position, Vector2Scale(facing, 330.0f));
        Graphics::SpawnImpactBurst(tip, facing, ORANGE, {255, 120, 0, 255}, 10, 4);
        hasHit = true;
      }
      stateTimer = 0.6f;
      aiState = AIState::IDLE;
      attackCooldown = 0.8f;
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
      if (!player.IsImmune() && CombatUtils::CheckProgressiveRadial(position, player.position, player.radius, 300.0f, 1.0f)) {
        player.hp -= 42.35f; // +20% total
        Vector2 diff = Vector2Subtract(player.position, position);
        player.velocity = Vector2Add(
            player.velocity, Vector2Scale(Vector2Normalize(diff), 2800.0f));
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
    Vector2 target = {2000.0f, 1380.0f}; // Esquina Norte (ajustado para estar dentro de la arena)
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
        waveSpawnTimer = 3.0f; // Un poco más de tiempo para el loop de animación (antes 2.4s)
        // Spawn Wave
        if (waveCount < 10) {
            waves[waveCount] = {position, 0.0f, 400.0f, true, false}; // Velocidad reducida (antes 480)
            waveCount++;
        }
        screenShake = fmaxf(screenShake, 2.8f); 
        // VFX de impacto en el suelo
        for(int i=0; i<20; i++)
          Graphics::VFXSystem::GetInstance().SpawnParticleEx(position, {(float)GetRandomValue(-500, 500), (float)GetRandomValue(-500, 500)}, 0.7f, MAROON, 8.0f);
    }

    // Actualizar ondas de choque
    for (int i = 0; i < waveCount; i++) {
        if (!waves[i].active) continue;
        waves[i].radius += waves[i].speed * dt;
        
        // Colisión con jugador
        float dist = CombatUtils::GetIsoDistance(waves[i].center, player.position);
        // Margen exacto: el jugador es golpeado si su radio toca la zona de peligro (38px de ancho efectivo)
        if (!waves[i].hasHit && !player.IsImmune() && fabsf(dist - waves[i].radius) < (38.0f + player.radius)) {
            player.hp -= 30.0f; // Dano aumentado a 30 por ola
            player.velocity = Vector2Add(player.velocity, Vector2Scale(Vector2Normalize(Vector2Subtract(player.position, waves[i].center)), 2200.0f));
            waves[i].hasHit = true;
        }

        if (waves[i].radius > 2400.0f) waves[i].active = false;
    }

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
  }
}

// ── Enemy::Update ────────────────────────────────────────────────────
void Enemy::Update() {
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
    velocity = Vector2Scale(velocity, 0.90f); // Se frena al morir
    if (deathAnimTimer <= 0) {
      isDying = false;
      isDead = true;
      respawnTimer = 15.0f;
    }
  }
  if (recentDamageTimer > 0) {
    recentDamageTimer -= GetFrameTime() * g_timeScale;
    if (recentDamageTimer <= 0)
      recentDamage = 0;
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
  if (aiState == AIState::ATTACK_JUMP && attackPhaseTimer > 0)
    jumpH = sinf((1.0f - attackPhaseTimer) * PI) * 450.0f;

  bool telegraphing =
      (attackPhaseTimer > 0 && aiState != AIState::IDLE &&
       aiState != AIState::CHASE && aiState != AIState::ORBITING &&
       aiState != AIState::STAGGERED && aiState != AIState::EVADE);

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
      } else if (aiState == AIState::CHASE || aiState == AIState::ORBITING ||
                 aiState == AIState::EVADE || aiState == AIState::AVALANCHE_START) {
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
        if (prog < 0) prog = 0;
        if (prog > 0.99f) prog = 0.99f;
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

    // Actualizar textura con el frame actual del GIF
    if (totalFrames > 0) {
      currentFrame = currentFrame % totalFrames;
      unsigned int frameSize = ResourceManager::texEnemyIm.width *
                               ResourceManager::texEnemyIm.height * 4;
      UpdateTexture(ResourceManager::texEnemy,
                    ((unsigned char *)ResourceManager::texEnemyIm.data) +
                        (frameSize * currentFrame));
    }

    // Flip horizontal si el Golem mira hacia la DERECHA
    float drawSrcW = (facing.x > 0.1f) ? -(float)ResourceManager::texEnemy.width
                                       : (float)ResourceManager::texEnemy.width;
    Rectangle src = {0, 0, drawSrcW, (float)ResourceManager::texEnemy.height};

    // Ajuste de escala y centro (Pivot)
    float scale = 2.4f; // Tamaño aumentado
    float finalW = (float)ResourceManager::texEnemy.width * scale;
    float finalH = (float)ResourceManager::texEnemy.height * scale;

    Rectangle dest = {position.x, position.y - jumpH, finalW, finalH};
    // Pivot: X al centro, Y casi al final para que los pies toquen la hitbox
    Vector2 orig = {finalW * 0.5f, finalH * 0.92f};

    DrawTexturePro(ResourceManager::texEnemy, src, dest, orig, 0.0f, tint);
  } else {
    // Fallback hasta que llegue el nuevo sprite
    DrawCircleV({position.x, position.y - 30.0f - jumpH}, radius, color);
    DrawLineV({position.x, position.y},
              {position.x + facing.x * radius * 1.4f,
               position.y + facing.y * radius * 0.7f},
              Fade(WHITE, 0.6f));
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
    DrawEllipseLines(position.x, position.y, 351.0f, 351.0f * 0.5f, Fade(RED, 0.4f));

  } else if (aiState == AIState::ATTACK_HEAVY && attackPhaseTimer > 0) {
    float prog = CombatUtils::GetProgress(attackPhaseTimer, 1.1f) * 0.95f; // Slower visual progression (was 0.6f, no multiplier)
    if (prog > 1.0f) prog = 1.0f;
    float ang = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
    rlDrawRenderBatchActive();
    rlPushMatrix();
    rlTranslatef(position.x, position.y, 0);
    rlScalef(1, 0.5f, 1);
    DrawCircleSector({0,0}, 330.0f, ang - 15.0f, ang + 15.0f, 24, Fade(ORANGE, 0.25f));
    DrawCircleSectorLines({0,0}, 330.0f * prog, ang - 15.0f, ang + 15.0f, 24, Fade(YELLOW, 0.6f + 0.4f * prog));
    DrawCircleSectorLines({0,0}, 330.0f, ang - 15.0f, ang + 15.0f, 24, Fade(ORANGE, 0.4f));
    rlDrawRenderBatchActive();
    rlPopMatrix();

  } else if (aiState == AIState::ATTACK_BASIC && attackPhaseTimer > 0) {
    float prog = CombatUtils::GetProgress(attackPhaseTimer, 0.55f) * 0.95f; // Slower visual progression (was 0.2f, no multiplier)
    if (prog > 1.0f) prog = 1.0f;
    float ang = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
    rlDrawRenderBatchActive();
    rlPushMatrix();
    rlTranslatef(position.x, position.y, 0);
    rlScalef(1, 0.5f, 1);
    DrawCircleSector({0, 0}, 260.0f, ang - 65.0f, ang + 65.0f, 32,
                     Fade(RED, 0.2f));
    // Dibuja el barrido con un color más vivo
    DrawCircleSectorLines({0, 0}, 260.0f, ang - 65.0f, ang - 65.0f + (130.0f * prog), 32,
                          Fade(YELLOW, 0.5f + 0.5f * prog));
    DrawCircleSectorLines({0, 0}, 260.0f, ang - 65.0f, ang + 65.0f, 32, Fade(RED, 0.3f));
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
    DrawCircleV({0,0}, 15 * (1.0f - prog), Fade(WHITE, 0.5f));
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
    rlTranslatef(position.x, position.y, 0); // O usar el centro de cada wave si es distinto
    rlScalef(1, 0.5f, 1);
    
    for (int i = 0; i < waveCount; i++) {
        if (!waves[i].active) continue;
        
        Vector2 visualCenter = Vector2Subtract(waves[i].center, position);
        float alpha = 1.0f - (waves[i].radius / 2200.0f);
        if (alpha < 0) alpha = 0;
        
        // Efecto de brillo de la onda (Hitbox resaltada y exacta)
        float collisionWidth = 38.0f;
        
        // Relleno de la zona de peligro (Ring filling)
        // Usamos múltiples círculos para simular un anillo sólido y claro
        for (float r_off = -collisionWidth; r_off <= collisionWidth; r_off += 4.0f) {
            float r_current = waves[i].radius + r_off;
            if (r_current < 0) continue;
            
            // Color varía según si es el centro de la onda o los bordes
            float intensity = 1.0f - (fabsf(r_off) / collisionWidth);
            Color waveColor = (fabsf(r_off) < 5.0f) ? YELLOW : RED;
            DrawCircleLinesV(visualCenter, r_current, Fade(waveColor, alpha * 0.4f * intensity));
        }

        // Borde exterior e interior nítidos
        DrawCircleLinesV(visualCenter, waves[i].radius + collisionWidth, Fade(WHITE, alpha * 0.7f));
        DrawCircleLinesV(visualCenter, waves[i].radius - collisionWidth, Fade(WHITE, alpha * 0.7f));

        // Pulso de advertencia en el radio exacto
        float pulse = 0.8f + 0.2f * sinf(g_gameTime * 20.0f);
        DrawCircleLinesV(visualCenter, waves[i].radius, Fade(YELLOW, alpha * pulse));
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
}

void Enemy::ScaleDifficulty(int wave) {
  maxHp *= (1.0f + wave * 0.35f);
  hp = maxHp;
  aggressionLevel *= (1.0f + wave * 0.15f); // Se vuelve más rápido y agresivo
}
