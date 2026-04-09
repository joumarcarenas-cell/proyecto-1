#include "entities.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

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
  texEnemy = LoadTexture("assets/golem.png");
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
struct Particle {
  Vector2 pos;
  Vector2 vel;
  float life;
  Color col;
};
std::vector<Particle> particles; // Global para acceso en IA y otros sistemas

bool Enemy::CheckAttackCollision(Player &player, float range, float angle,
                                 float damage) {
  if (player.IsImmune())
    return false; // i-frames esquivan golpes!

  Vector2 diff = Vector2Subtract(player.position, position);
  float isoY = diff.y * 2.0f;
  float isoDist = sqrtf(diff.x * diff.x + isoY * isoY);

  if (isoDist < range + player.radius) {
    float angleToPlayer = atan2f(isoY, diff.x) * RAD2DEG;
    float angleFacing = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
    float angleDiff =
        fabsf(fmodf(angleToPlayer - angleFacing + 540.0f, 360.0f) - 180.0f);

    if (angleDiff <= angle / 2.0f) {
      player.hp -= damage;
      return true;
    }
  }
  return false;
}

void Enemy::UpdateAI(Player &player) {
  if (isDead)
    return;
  float dt = GetFrameTime();

  if (dashTimer > 0)
    dashTimer -= dt;
  if (slamTimer > 0)
    slamTimer -= dt;
  if (jumpTimer > 0)
    jumpTimer -= dt;
  if (evadeCooldown > 0)
    evadeCooldown -= dt;

  // --- GESTIÓN DE ANIMACIÓN SEGÚN EL ESTADO Y DIRECCIÓN ---
  frameCols = 12;
  frameRows = 6;

  if (aiState != previousAIState) {
    currentFrameX = 0;
    frameTimer = 0.0f;
    previousAIState = aiState;
  }

  // Lógica de Filas basada en dirección y estado
  if (aiState == AIState::STAGGERED) {
    currentFrameY = 4;
  } else if (fabsf(facing.x) > fabsf(facing.y)) {
    // Movimiento mayormente horizontal (Perfil)
    bool isAttacking =
        (aiState == AIState::ATTACK_BASIC || aiState == AIState::ATTACK_DASH ||
         aiState == AIState::ATTACK_SLAM || aiState == AIState::ATTACK_HEAVY ||
         aiState == AIState::ATTACK_ROCKS || aiState == AIState::ATTACK_JUMP);
    currentFrameY = isAttacking ? 3 : 2;
  } else if (facing.y < 0) {
    // Hacia arriba (Espalda)
    currentFrameY = 1;
  } else {
    // Hacia abajo o IDLE (Frente)
    currentFrameY = 0;
  }

  // Definir maxFrames por fila (según las especificaciones reales del sprite
  // sheet)
  int maxFrames = 12;
  switch (currentFrameY) {
  case 0:
    maxFrames = 5;
    break; // Frente
  case 1:
    maxFrames = 7;
    break; // Espalda
  case 2:
    maxFrames = 12;
    break; // Caminar Lado
  case 3:
    maxFrames = 12;
    break; // Ataque Lado
  case 4:
    maxFrames = 4;
    break; // Herido / Stun
  case 5:
    maxFrames = 12;
    break; // Muerte
  default:
    maxFrames = 12;
    break;
  }

  // Actualizar animación del spritesheet
  frameTimer += dt;
  if (frameTimer >= frameSpeed) {
    frameTimer = 0;
    currentFrameX = (currentFrameX + 1) % maxFrames;
  }

  if (hitFlashTimer > 0)
    hitFlashTimer -= dt;

  // --- Lluvia de Rocas Independiente ---
  if (rocksToSpawn > 0) {
    rockSpawnTimer -= dt;
    if (rockSpawnTimer <= 0.0f) {
      if (rocksSpawned < 5) {
        rocks[rocksSpawned].position = player.position;
        rocks[rocksSpawned].fallTimer = 1.0f; // 1s de aviso visual
        rocks[rocksSpawned].active = true;
        rocksSpawned++;
        rocksToSpawn--;
        rockSpawnTimer = 0.4f; // Una cada 0.4s
      } else {
        rocksToSpawn = 0; // Se llenó el array
      }
    }
  }

  // --- IA Reactiva (Evasion) ---
  if (evadeCooldown <= 0.0f &&
      (aiState == AIState::CHASE || aiState == AIState::ORBITING ||
       aiState == AIState::IDLE)) {
    // AttackPhase::STARTUP o isCharging significan que el jugador levantó el
    // arma
    if (player.isCharging || player.attackPhase == AttackPhase::STARTUP) {
      float distToP = Vector2Distance(player.position, position);
      if (distToP <= 350.0f) {
        if (GetRandomValue(1, 100) <= 30) { // 30% chance
          aiState = AIState::EVADE;
          stateTimer = 0.35f;
          Vector2 away = Vector2Subtract(position, player.position); // Alejar
          if (GetRandomValue(0, 1) == 0) {                           // Lateral
            evadeDir = {-away.y, away.x};
            if (GetRandomValue(0, 1) == 0)
              evadeDir = {away.y, -away.x};
          } else { // Atrás
            evadeDir = away;
          }
          if (Vector2Length(evadeDir) > 0)
            evadeDir = Vector2Normalize(evadeDir);
          evadeCooldown = 4.0f; // Evade successful
        } else {
          evadeCooldown = 0.5f; // Wait to try again
        }
      }
    }
  }

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

    // Decisión de transición
    if (dist > 450.0f) {
      aiState = AIState::CHASE;
    } else if (dist <= 450.0f && dist > 200.0f) {
      if (aiState != AIState::ORBITING) {
        aiState = AIState::ORBITING;
        orbitDir = (GetRandomValue(0, 1) == 0) ? 1 : -1;
        orbitAngle = atan2f(diff.y, diff.x);
      }
    } else {
      aiState = AIState::CHASE; // Cerca, va a atacar
    }

    // Lógica de Movimiento
    if (aiState == AIState::CHASE) {
      if (dist > 180.0f) {
        velocity = Vector2Add(velocity, Vector2Scale(facing, 1400.0f * dt));
      }
    } else if (aiState == AIState::ORBITING) {
      orbitAngle += orbitDir * 1.5f * dt;
      Vector2 targetOrbit = {player.position.x - cosf(orbitAngle) * 350.0f,
                             player.position.y - sinf(orbitAngle) * 350.0f};
      Vector2 moveDir =
          Vector2Normalize(Vector2Subtract(targetOrbit, position));
      velocity = Vector2Add(velocity, Vector2Scale(moveDir, 1500.0f * dt));
    }

    // Selección de ataque (Agresión)
    if (attackCooldown <= 0.0f) {
      bool canJump = (jumpTimer <= 0.0f && dist <= 180.0f);
      bool canSlam = (slamTimer <= 0.0f && dist <= 270.0f);
      bool canDash = (dashTimer <= 0.0f && dist > 260.0f && dist <= 750.0f);
      bool canBasic = (dist <= 250.0f);

      if (canJump || canSlam || canDash || canBasic) {
        if (canJump) {
          aiState = AIState::ATTACK_JUMP;
          attackPhaseTimer = 1.0f; // Tiempo en el aire (1.0s)
          hasHit = false;
          jumpTimer = 15.0f;
          attackStep = 0;
        } else if (canSlam && GetRandomValue(1, 100) <= 60) {
          aiState = AIState::ATTACK_SLAM;
          attackPhaseTimer = 0.9f;
          hasHit = false;
          slamTimer = 8.0f;
          attackStep = 0;
        } else if (canDash && GetRandomValue(1, 100) <= 70) {
          aiState = AIState::ATTACK_DASH;
          attackPhaseTimer = 0.7f;
          hasHit = false;
          dashTimer = 5.0f;
          attackStep = 0;
          currentDashDist = Clamp(dist, 180.0f, 700.0f);
        } else if (canBasic) {
          aiState = AIState::ATTACK_BASIC;
          attackPhaseTimer = 0.3f;
          hasHit = false;
        }
      }
    } else {
      attackCooldown -= dt * aggressionLevel;
    }
    break;
  }
  case AIState::ATTACK_BASIC: {
    attackPhaseTimer -= dt;
    if (attackPhaseTimer <= 0 && !hasHit) {
      if (CheckAttackCollision(player, 220.0f, 60.0f, 15.0f)) {
        // hit
      }
      hasHit = true;
      stateTimer = 0.3f; // Recovery

      attackStep++;
      if (attackStep < 2) {
        aiState = AIState::IDLE;
        attackCooldown = 0.1f;
      } else {
        // Mix-up Decision
        attackStep = 0;
        int r = GetRandomValue(1, 100);
        if (r <= 33) {
          aiState = AIState::ATTACK_HEAVY;
          attackPhaseTimer = 0.6f;
          hasHit = false;
          mixupDecided = true;
        } else if (r <= 66) {
          aiState = AIState::ATTACK_ROCKS;
          attackPhaseTimer = 1.0f;
          rocksSpawned = 0;
          rockSpawnTimer = 0.0f;
          mixupDecided = true;
          screenShake = fmaxf(screenShake, 1.0f);
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
      Vector2 diff = Vector2Subtract(player.position, position);
      if (Vector2Length(diff) > 0)
        facing = Vector2Normalize(diff);
      if (attackPhaseTimer <= 0) {
        stateTimer = 0.35f; // Tiempo fijo del dash (0.35s de duracion)
      }
    } else {
      stateTimer -= dt;

      // Sobrescribir la fricción cada frame para garantizar distancia lineal
      // estricta
      velocity = Vector2Scale(facing, currentDashDist / 0.35f);

      if (!hasHit) {
        if (CheckAttackCollision(player, 100.0f, 60.0f,
                                 36.0f)) { // 10% menos daño (36)
          hasHit = true;
          screenShake = 3.0f; // Intensidad alta de stun
          player.stunTimer = 0.5f;
        }
      }
      if (stateTimer <= 0) {
        velocity = {0, 0}; // Parada en seco

        if (dashCharges < 1) {
          dashCharges++;
          attackPhaseTimer = 0.4f; // Preparar segunda embestida
          hasHit = false;
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
      // Vibración visual mientras carga el golpe (Pasadez/Predictibilidad)
      if (attackPhaseTimer < 0.4f)
        screenShake = fmaxf(screenShake, 0.4f);
    } else {
      if (!hasHit) {
        // Hitbox del Terremoto (Ajustando representación gráfica isométrica)
        Vector2 slamDiff = Vector2Subtract(player.position, position);
        float slamIsoDist = Vector2Length({slamDiff.x, slamDiff.y * 2.0f});

        // Rango incrementado en un 25% (280 -> 350)
        float maxSlamDist = 350.0f;

        if (!player.IsImmune() && slamIsoDist <= maxSlamDist + player.radius) {
          player.hp -= 30.0f;
          player.slowTimer = 1.5f; // Efecto Slow

          // ScreenShake escalable en funcion de cercania
          float proximity = 1.0f - (slamIsoDist / maxSlamDist);
          if (proximity < 0)
            proximity = 0;
          screenShake = 1.8f + (2.5f * proximity);

          hasHit = true;
        }
        // HasHit debe ser true en general para no triggerear collision todo el
        // rato
        hasHit = true;
        stateTimer = 0.8f; // Recovery
      }
      stateTimer -= dt;
      if (stateTimer <= 0) {
        aiState = AIState::IDLE;
        stateTimer = 0.5f;
        attackCooldown =
            1.2f; // Un poco menos de espera para compensar el recovery
      }
    }
    break;
  }
  case AIState::ATTACK_HEAVY: {
    attackPhaseTimer -= dt;
    if (attackPhaseTimer <= 0 && !hasHit) {
      if (CheckAttackCollision(player, 330.0f, 30.0f, 25.0f)) {
        screenShake = 1.5f;
      }
      hasHit = true;

      // Spawn de GroundBurst pequeño al final (Visual + Daño)
      Vector2 tipPos = Vector2Add(position, Vector2Scale(facing, 330.0f));
      Vector2 diff = Vector2Subtract(player.position, tipPos);
      float tipDist =
          sqrtf(diff.x * diff.x + (diff.y * 2.0f) * (diff.y * 2.0f));

      // Particulas para la punta
      for (int i = 0; i < 15; i++) {
        particles.push_back({tipPos,
                             {(float)GetRandomValue(-200, 200),
                              (float)GetRandomValue(-200, 200)},
                             0.4f,
                             ORANGE});
      }

      if (!player.IsImmune() && tipDist <= 60.0f + player.radius) {
        player.hp -= 10.0f; // 40% del daño
      }

      stateTimer = 0.6f;
    }
    if (hasHit) {
      stateTimer -= dt;
      if (stateTimer <= 0) {
        aiState = AIState::IDLE;
        attackCooldown = 0.8f;
      }
    }
    break;
  }

  case AIState::ATTACK_JUMP: {
    if (attackPhaseTimer > 0) {
      attackPhaseTimer -= dt;

      // Track lightly mid-air
      Vector2 diff = Vector2Subtract(player.position, position);
      if (Vector2Length(diff) > 20.0f && attackPhaseTimer > 0.3f) {
        velocity = Vector2Add(
            velocity, Vector2Scale(Vector2Normalize(diff), 800.0f * dt));
      }

      if (attackPhaseTimer <= 0.0f) {
        stateTimer = 0.6f; // Recovery for shockwave expansion
      }
    } else {
      if (!hasHit) {
        // Caída e impacto!
        screenShake =
            fmaxf(screenShake, 2.5f); // Reduced wait, target this manually
        hasHit = true;

        // Daño e impulso Player
        Vector2 diff = Vector2Subtract(player.position, position);
        float isoDist = Vector2Length({diff.x, diff.y * 2.0f});
        if (!player.IsImmune() && isoDist <= 300.0f + player.radius) {
          player.hp -= 35.0f; // Daño del salto
          Vector2 pushDir = Vector2Normalize(diff);
          player.velocity =
              Vector2Add(player.velocity,
                         Vector2Scale(pushDir, 2800.0f)); // Fuerte empuje!
        }

        for (int i = 0; i < 30; i++) {
          particles.push_back({position,
                               {(float)GetRandomValue(-500, 500),
                                (float)GetRandomValue(-500, 500)},
                               0.6f,
                               RED});
        }
      }

      stateTimer -= dt;
      velocity = Vector2Scale(velocity, 0.5f); // Parada
      if (stateTimer <= 0.0f) {
        aiState = AIState::IDLE;
        attackCooldown = baseAttackCooldown;
      }
    }
    break;
  }
  case AIState::ATTACK_ROCKS: {
    attackPhaseTimer -= dt;
    if (attackPhaseTimer <= 0.0f) {
      // Instantly go back to chase, but register the rocks
      rocksToSpawn = 5;
      stateTimer = 0.4f; // Very short recovery
      aiState = AIState::CHASE;
      attackCooldown = 0.5f;
    }
    break;
  }
  case AIState::EVADE: {
    stateTimer -= dt;
    velocity = Vector2Scale(
        evadeDir,
        2200.0f *
            dt); // Dash altísimo (2200px/s pero usando dt para frame indep)

    // Falsificar fricción y velocidad para evitar bugs isométricos
    // Actually, ya uso dt. Es mejor establecer la velocidad base
    velocity = Vector2Scale(evadeDir, 800.0f); // Fast fixed speed

    if (GetRandomValue(1, 100) <= 40) {
      particles.push_back({position, {0, 0}, 0.2f, GRAY});
    }
    Vector2 d = Vector2Subtract(player.position, position);
    if (Vector2Length(d) > 0)
      facing = Vector2Normalize(d);

    if (stateTimer <= 0.0f) {
      aiState = AIState::CHASE;
      velocity = {0, 0};
      attackCooldown = 0.2f; // Contrataca rápido tras evadir
    }
    break;
  }
  case AIState::STAGGERED:
    stateTimer -= dt;
    if (stateTimer <= 0.0f) {
      aiState = AIState::IDLE;
      stateTimer = 0.3f;
      // Particulas de recuperación
      for (int i = 0; i < 8; i++) {
        particles.push_back({position,
                             {(float)GetRandomValue(-200, 200),
                              (float)GetRandomValue(-200, 200)},
                             0.5f,
                             WHITE});
      }
    }
    break;
  }
}

void Enemy::Update() {
  if (isDead) {
    respawnTimer -= GetFrameTime();
    if (respawnTimer <= 0.0f) {
      isDead = false;
      hp = maxHp;
      position = spawnPos;
      velocity = {0, 0};
    }
    return;
  }

  if (recentDamageTimer > 0.0f) {
    recentDamageTimer -= GetFrameTime();
    if (recentDamageTimer <= 0.0f)
      recentDamage = 0.0f;
  }

  Vector2 nextPos =
      Vector2Add(position, Vector2Scale(velocity, GetFrameTime()));

  // Restricción de Arena (Hull de Diamante)
  float dx = std::abs(nextPos.x - 2000.0f);
  float dy = std::abs(nextPos.y - 2000.0f);
  if ((dx + 2.0f * dy) <= (1400.0f - radius * 2.236f)) {
    position = nextPos;
  }

  velocity = Vector2Scale(velocity, 0.85f);
}

void Enemy::Draw() {
  if (isDead)
    return;

  // 0. Cálculos previos (necesarios para el fallback y efectos)
  float jumpHeight = 0.0f;
  if (aiState == AIState::ATTACK_JUMP && attackPhaseTimer > 0) {
    float progress = 1.0f - (attackPhaseTimer / 1.0f);
    jumpHeight = sinf(progress * PI) * 450.0f; // Sube al cielo!
  }

  bool isTelegraphing =
      (attackPhaseTimer > 0 && aiState != AIState::IDLE &&
       aiState != AIState::CHASE && aiState != AIState::ORBITING &&
       aiState != AIState::STAGGERED && aiState != AIState::EVADE);
  Color drawTint = WHITE;
  if (hitFlashTimer > 0) {
    drawTint = {255, 255, 255, 255};
    color = WHITE;
  } else if (isTelegraphing) {
    float fTint = fmaxf(0.0f, sinf(attackPhaseTimer * 30.0f));
    drawTint.g = (unsigned char)(255 * (1.0f - fTint));
    drawTint.b = (unsigned char)(255 * (1.0f - fTint));
    color = {(unsigned char)(130 + 125 * fTint), 0, 0, 255};
  } else {
    color = MAROON;
  }

  // Sombra
  DrawEllipse((int)position.x, (int)position.y, radius, radius * 0.5f,
              Fade(BLACK, 0.4f));

  // 1. Dibujo del Boss (Prioridad Textura, Fallback Círculo)
  if (ResourceManager::texEnemy.id != 0) {
    // Cálculo dinámico de dimensiones y posición del frame
    float fWidth = (float)ResourceManager::texEnemy.width / 12.0f;
    float fHeight = (float)ResourceManager::texEnemy.height / 6.0f;

    float frameX = (float)currentFrameX * fWidth;
    float frameY = (float)currentFrameY * fHeight;

    // Recorte exacto y escala corregida para el Golem
    float finalSourceWidth = fWidth;
    Rectangle sourceRec = {frameX, frameY, finalSourceWidth, fHeight};

    float visualScale =
        2.0f; // Escala reducida para ajustarse mejor al escenario
    Rectangle destRec = {position.x, position.y - jumpHeight,
                         fWidth * visualScale, fHeight * visualScale};

    // El origin se ajusta para que el centro inferior (pies) coincida con
    // position
    Vector2 origin = {(fWidth * visualScale) / 2.0f,
                      (fHeight * visualScale) * 0.85f};

    DrawTexturePro(ResourceManager::texEnemy, sourceRec, destRec, origin, 0.0f,
                   drawTint);
  } else {
    DrawCircleV({position.x, position.y - 30 - jumpHeight}, radius, color);
  }

  // Indicador visual de ataque de la IA
  if (aiState == AIState::ATTACK_SLAM) {
    if (attackPhaseTimer > 0) {
      float progress = 1.0f - (attackPhaseTimer / 1.2f);
      float pulse =
          sinf(GetTime() * 15.0f) * 0.1f * progress; // Vibración visual
      rlPushMatrix();
      rlTranslatef(position.x, position.y, 0);
      rlScalef(1.0f + pulse, 0.5f + pulse, 1.0f); // Óvalo que vibra
      DrawCircleV({0, 0}, 350.0f, Fade(RED, progress * 0.5f + 0.1f));
      DrawCircleLines(0, 0, 350.0f, Fade(WHITE, progress * 0.8f + 0.2f));
      // Anillo de advertencia que se cierra
      DrawCircleLines(0, 0, 350.0f * (1.0f - progress), Fade(RED, 0.8f));
      rlPopMatrix();
    }
  } else if (aiState == AIState::ATTACK_HEAVY) {
    if (attackPhaseTimer > 0) {
      float progress = 1.0f - (attackPhaseTimer / 0.6f);
      float angleDash = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
      rlPushMatrix();
      rlTranslatef(position.x, position.y, 0);
      rlScalef(1.0f, 0.5f, 1.0f);
      Rectangle dashRect = {0, -30.0f, 330.0f, 60.0f};
      DrawRectanglePro(dashRect, {0, 30.0f}, angleDash, Fade(ORANGE, 0.2f));
      Rectangle progressRect = {0, -30.0f, 330.0f * progress, 60.0f};
      DrawRectanglePro(progressRect, {0, 30.0f}, angleDash,
                       Fade(ORANGE, 0.5f + 0.5f * progress));
      rlPopMatrix();
    }
  } else if (aiState == AIState::ATTACK_BASIC ||
             aiState == AIState::ATTACK_DASH) {
    if (attackPhaseTimer > 0) {
      float maxTime = (aiState == AIState::ATTACK_BASIC) ? 0.5f : 1.0f;
      float progress = 1.0f - (attackPhaseTimer / maxTime);
      Color warnCol = (aiState == AIState::ATTACK_DASH) ? RED : ORANGE;

      if (aiState == AIState::ATTACK_DASH) {
        float dashDist = currentDashDist;
        rlPushMatrix();
        rlTranslatef(position.x, position.y, 0);
        rlScalef(1.0f, 0.5f, 1.0f);
        float angleDash = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
        // Rectángulo con relleno progresivo
        Rectangle dashRect = {0, -100.0f, dashDist, 200.0f};
        DrawRectanglePro(dashRect, {0, 100.0f}, angleDash, Fade(RED, 0.2f));
        Rectangle progressRect = {0, -100.0f, dashDist * progress, 200.0f};
        DrawRectanglePro(progressRect, {0, 100.0f}, angleDash, Fade(RED, 0.5f));

        Vector2 lineEnd = {dashDist * cosf(angleDash * DEG2RAD),
                           dashDist * sinf(angleDash * DEG2RAD)};
        DrawLineEx({0, 0}, lineEnd, 4.0f, ORANGE);
        rlPopMatrix();
      } else {
        float warningRadius = 220.0f;
        float warningAngle = 60.0f;
        rlPushMatrix();
        rlTranslatef(position.x, position.y, 0);
        rlScalef(1.0f, 0.5f, 1.0f);
        float startAngle =
            atan2f(facing.y * 2.0f, facing.x) * RAD2DEG - (warningAngle / 2.0f);
        // Sector con borde brillante y pulso
        DrawCircleSector({0, 0}, warningRadius, startAngle,
                         startAngle + warningAngle, 32,
                         Fade(warnCol, 0.2f + 0.3f * progress));
        DrawCircleSectorLines({0, 0}, warningRadius, startAngle,
                              startAngle + warningAngle, 32,
                              Fade(WHITE, progress));
        rlPopMatrix();
      }
    }
  } else if (aiState == AIState::ATTACK_JUMP) {
    rlPushMatrix();
    rlTranslatef(position.x, position.y, 0);
    rlScalef(1.0f, 0.5f, 1.0f);
    if (attackPhaseTimer > 0) {
      // Telegrafiando posición de caída (sombra roja encogiéndose)
      float prog = 1.0f - (attackPhaseTimer / 1.0f);
      DrawCircleV({0, 0}, 300.0f, Fade(RED, prog * 0.4f));
      DrawCircleLines(0, 0, 300.0f, Fade(WHITE, 0.5f));
    } else {
      // Onda expansiva en recovery
      float wProg = 1.0f - (stateTimer / 0.6f);
      float currentRadius = 300.0f * wProg;
      DrawCircleLines(0, 0, currentRadius, Fade(WHITE, 1.0f - wProg));
      DrawCircleLines(0, 0, currentRadius + 15.0f, Fade(RED, 1.0f - wProg));
    }
    rlPopMatrix();
  }

  // --- Rocas ---
  for (int i = 0; i < rocksSpawned; i++) {
    if (rocks[i].active) {
      float prog = 1.0f - rocks[i].fallTimer;
      // Sombra en el suelo
      rlPushMatrix();
      rlTranslatef(rocks[i].position.x, rocks[i].position.y, 0);
      rlScalef(1.0f, 0.5f, 1.0f);
      DrawCircleV({0, 0}, 60.0f * prog, Fade(BLACK, 0.5f + 0.3f * prog));
      DrawCircleLines(0, 0, 60.0f, Fade(RED, 0.7f));
      rlPopMatrix();
      // Roca cayendo
      Vector2 dropPos = {rocks[i].position.x,
                         rocks[i].position.y - 1000.0f * (1.0f - prog)};
      DrawCircleV(dropPos, 20.0f, DARKGRAY);
    }
  }
}

void DrawQuad(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, Color c) {
  DrawTriangle(p1, p2, p3, c);
  DrawTriangle(p1, p3, p2, c);
  DrawTriangle(p1, p3, p4, c);
  DrawTriangle(p1, p4, p3, c);
}

bool isTimeStopped = false;
float hitstopTimer = 0.0f;
float playerGhostHp = 0.0f;
float bossGhostHp = 0.0f;

void ResetGame(Player *player, Enemy &boss, GamePhase &phase) {
  player->Reset({2000, 2000});
  isTimeStopped = false;
  boss.hp = boss.maxHp;
  boss.position = boss.spawnPos;
  boss.velocity = {0, 0};
  boss.aiState = Enemy::AIState::IDLE;
  boss.stateTimer = 1.0f;
  boss.isDead = false;
  boss.bleedTimer = 0;
  boss.isBleeding = false;
  boss.recentDamage = 0.0f;
  boss.rocksSpawned = 0;
  boss.rocksToSpawn = 0;
  for (int i = 0; i < 5; i++)
    boss.rocks[i].active = false;

  particles.clear();
  damageTexts.clear();
  playerGhostHp = player->hp;
  bossGhostHp = boss.hp;
  phase = GamePhase::RUNNING;
}

int main() {
  InitWindow(1920, 1080, "Hades Prototype - Polymorphic Players");
  SetTargetFPS(60);

  ResourceManager::Load(); // CARGA DE ASSETS EN GPU

  Reaper reaper({2000, 2000});
  Ropera ropera({2000, 2000});
  Player *activePlayer = &reaper; // Default, se reemplaza en CHAR_SELECT

  Enemy boss({2300, 2000});
  boss.maxHp = 2000.0f;
  boss.hp = 2000.0f;

  Camera2D camera = {{0, 0}, activePlayer->position, 0.0f, 1.35f};
  camera.offset = {(float)GetScreenWidth() / 2.0f,
                   (float)GetScreenHeight() / 2.0f};

  screenShake = 0.0f;

  GamePhase currentPhase = GamePhase::CHAR_SELECT;
  int *rebindingKey = nullptr;
  std::string rebindingName = "";

  ShowCursor();

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    // --- GESTIÓN DE PAUSA Y MENÚS ---
    if (IsKeyPressed(KEY_K)) {
      if (currentPhase == GamePhase::RUNNING) {
        currentPhase = GamePhase::PAUSED;
        ShowCursor();
      } else if (currentPhase == GamePhase::PAUSED) {
        currentPhase = GamePhase::RUNNING;
      } else if (currentPhase == GamePhase::SETTINGS ||
                 currentPhase == GamePhase::REBINDING) {
        currentPhase = GamePhase::PAUSED;
        rebindingKey = nullptr;
      }
    }

    // Reinicio rápido con 'P' en cualquier momento excepto selección de
    // personaje
    if (IsKeyPressed(KEY_P) && currentPhase != GamePhase::CHAR_SELECT) {
      ResetGame(activePlayer, boss, currentPhase);
    }

    switch (currentPhase) {
    case GamePhase::RUNNING: {
      if (hitstopTimer > 0)
        hitstopTimer -= dt;
      if (screenShake > 0)
        screenShake -= dt;
      if (hitstopTimer > 0)
        hitstopTimer -= dt;
      if (screenShake > 0)
        screenShake -= dt;
      // Cámara suave centrada en el jugador con perspectiva mejorada
      camera.offset = {(float)GetScreenWidth() / 2.0f,
                       (float)GetScreenHeight() / 2.0f};
      float lerpCoeff = 10.0f * dt;
      camera.target.x +=
          (activePlayer->position.x - camera.target.x) * lerpCoeff;
      camera.target.y +=
          ((activePlayer->position.y - 40.0f) - camera.target.y) * lerpCoeff;
      activePlayer->targetAim = GetScreenToWorld2D(GetMousePosition(), camera);

      if (hitstopTimer <= 0) {
        activePlayer->Update();
        activePlayer->HandleSkills(boss);
      }

      // =========================================
      // BOSS: DoT (Sangrado) tick - Pausado en Time Stop
      // =========================================
      if (boss.isBleeding && !boss.isDead && !isTimeStopped) {
        boss.bleedTimer -= dt;
        boss.bleedTickTimer -= dt;
        if (boss.bleedTickTimer <= 0) {
          boss.bleedTickTimer =
              1.0f; // Tick cada 1s (O mas lento para mayor visibilidad?)
          float tickDmg =
              (boss.maxHp * 0.05f) / 10.0f; // 10 ticks de 0.5% hp c/u
          boss.hp -= tickDmg;
          damageTexts.push_back({boss.position,
                                 {(float)GetRandomValue(-25, 25), -60.0f},
                                 1.2f,
                                 1.2f,
                                 (int)tickDmg,
                                 {255, 30, 30, 255}});
          for (int i = 0; i < 6; i++)
            particles.push_back({boss.position,
                                 {(float)GetRandomValue(-100, 100),
                                  (float)GetRandomValue(-100, 100)},
                                 0.6f,
                                 {255, 0, 0, 255}});
        }
        if (boss.bleedTimer <= 0) {
          boss.bleedTimer = 0;
          boss.isBleeding = false;
        }
      }

      if (!boss.isDead && !isTimeStopped) {
        boss.UpdateAI(*activePlayer);
      }

      // No actualizar el boss durante el time-stop
      if (!isTimeStopped) {
        boss.Update();

        // --- Actualizacion Rocas (Artilleria del Mix-up) ---
        for (int i = 0; i < boss.rocksSpawned; i++) {
          if (boss.rocks[i].active) {
            boss.rocks[i].fallTimer -= GetFrameTime();
            if (boss.rocks[i].fallTimer <= 0) {
              boss.rocks[i].active = false;

              // Chequeo colision isometrica
              Vector2 activeP = activePlayer->position;
              float pRad = activePlayer->radius;
              Vector2 rDiff = Vector2Subtract(activeP, boss.rocks[i].position);
              float rDist = sqrtf(rDiff.x * rDiff.x +
                                  (rDiff.y * 2.0f) * (rDiff.y * 2.0f));
              float impactRadius = 60.0f;

              bool hit = false;
              if (rDist <= impactRadius + pRad) {
                if (!activePlayer->IsImmune()) {
                  activePlayer->hp -= 20.0f;
                  hit = true;
                }
              }
              if (hit) {
                screenShake = fmaxf(screenShake, 1.2f);
              }

              // Efectos al impactar el suelo
              for (int k = 0; k < 10; k++) {
                particles.push_back({boss.rocks[i].position,
                                     {(float)GetRandomValue(-200, 200),
                                      (float)GetRandomValue(-200, 200)},
                                     0.5f,
                                     DARKGRAY});
              }
            }
          }
        }
      }

      // Separacion de cuerpos
      Vector2 &activePos = activePlayer->position;
      if (!boss.isDead) {
        float distEntities = Vector2Distance(activePos, boss.position);
        float minDist = 20.0f + boss.radius;
        bool dashActive = activePlayer->IsImmune();
        if (!dashActive && distEntities < minDist && distEntities > 0) {
          Vector2 pushDir =
              Vector2Normalize(Vector2Subtract(activePos, boss.position));
          float overlap = minDist - distEntities;
          activePos =
              Vector2Add(activePos, Vector2Scale(pushDir, overlap * 0.5f));
          boss.position = Vector2Subtract(
              boss.position, Vector2Scale(pushDir, overlap * 0.5f));
        }
      }

      // --- GHOST HEALTH LERP (Step 3) ---
      playerGhostHp = Lerp(playerGhostHp, activePlayer->hp, 3.0f * dt);
      bossGhostHp = Lerp(bossGhostHp, boss.hp, 3.0f * dt);

      // --- FURY PHASE (Step 4) ---
      if (boss.hp <= boss.maxHp * 0.5f && !boss.isDead) {
        boss.aggressionLevel = 1.6f;
        boss.baseAttackCooldown =
            boss.baseAttackCooldown * 0.8f; // Respiro más corto
      }

      // Muerte / Victoria
      if (activePlayer->hp <= 0.0f) {
        currentPhase = GamePhase::GAME_OVER;
        ShowCursor();
      }
      if (boss.hp <= 0.0f) {
        currentPhase = GamePhase::VICTORY;
        ShowCursor();
      }
    } break;

    case GamePhase::REBINDING: {
      int key = GetKeyPressed();
      if (key > 0) {
        *rebindingKey = key;
        currentPhase = GamePhase::SETTINGS;
        rebindingKey = nullptr;
      }
    } break;

    default:
      break;
    }

    // =====================================================
    // COLLISION HANDLING
    // =====================================================
    if (hitstopTimer <= 0 && !boss.isDead) { // <--- Added hitstopTimer check
      float prevBossHp = boss.hp;
      activePlayer->CheckCollisions(boss);
      if (boss.hp < prevBossHp) {
        float dmg = prevBossHp - boss.hp;

        // Sistema Stagger
        boss.recentDamage += dmg;
        boss.recentDamageTimer = 1.2f;
        if (boss.recentDamage >= 80.0f &&
            boss.aiState != Enemy::AIState::STAGGERED) {
          boss.aiState = Enemy::AIState::STAGGERED;
          boss.stateTimer = 1.2f;
          boss.recentDamage = 0.0f;
          boss.attackPhaseTimer = 0.0f;
          screenShake = fmaxf(screenShake, 2.0f); // Impacto fuerte!
        }

        damageTexts.push_back({boss.position,
                               {(float)GetRandomValue(-40, 40), -120.0f},
                               1.0f,
                               1.0f,
                               (int)dmg,
                               {200, 0, 255, 255}});
        screenShake =
            fmaxf(screenShake, dmg * 0.015f); // Scale shake with damage reduced
        for (int i = 0; i < 16; i++) {
          float pAngle =
              atan2f(activePlayer->facing.y, activePlayer->facing.x) +
              (float)GetRandomValue(-70, 70) * DEG2RAD;
          particles.push_back({boss.position,
                               {cosf(pAngle) * 400.0f, sinf(pAngle) * 400.0f},
                               0.5f,
                               {200, 0, 255, 255}});
        }
      }
    }

    // Partículas
    for (auto &p : particles) {
      p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt));
      p.life -= dt;
    }
    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
                       [](const Particle &p) { return p.life <= 0; }),
        particles.end());

    // Actualizar Textos
    for (auto &dtText : damageTexts)
      dtText.Update(dt);
    damageTexts.erase(
        std::remove_if(damageTexts.begin(), damageTexts.end(),
                       [](const DamageText &text) { return text.life <= 0; }),
        damageTexts.end());

    BeginDrawing();
    ClearBackground({25, 30, 40, 255});

    // Solo dibujar el mundo si estamos en una fase de juego activa
    if (currentPhase != GamePhase::CHAR_SELECT) {
      // Aplicar Screen Shake Mínimo
      Camera2D shakeCam = camera;
      if (screenShake > 0) {
        shakeCam.offset.x += (float)GetRandomValue(-1, 1) * screenShake;
        shakeCam.offset.y += (float)GetRandomValue(-1, 1) * screenShake;
      }

      BeginMode2D(shakeCam);
      // Arena Isométrica Mejorada (Diamante 1400x700)
      Vector2 pNorth = {2000, 2000 - 700};
      Vector2 pSouth = {2000, 2000 + 700};
      Vector2 pEast = {2000 + 1400, 2000};
      Vector2 pWest = {2000 - 1400, 2000};

      Color groundColor = {245, 245, 245, 255};
      Color wallColor = {180, 185, 195, 255};
      Color borderColor = {60, 65, 75, 255};
      float wallHeight = 120.0f;

      // --- DIBUJO DE PAREDES TRASERAS (Profundidad 2.5D) ---
      DrawQuad(pWest, pNorth, {pNorth.x, pNorth.y - wallHeight},
               {pWest.x, pWest.y - wallHeight}, wallColor);
      DrawQuad(pNorth, pEast, {pEast.x, pEast.y - wallHeight},
               {pNorth.x, pNorth.y - wallHeight},
               ColorBrightness(wallColor, -0.1f));

      DrawTriangle(pWest, pNorth, pEast, groundColor);
      DrawTriangle(pWest, pEast, pSouth, groundColor);

      DrawLineEx(pNorth, pEast, 5.0f, borderColor);
      DrawLineEx(pEast, pSouth, 5.0f, borderColor);
      DrawLineEx(pSouth, pWest, 5.0f, borderColor);
      DrawLineEx(pWest, pNorth, 5.0f, borderColor);

      DrawLineEx({pWest.x, pWest.y - wallHeight},
                 {pNorth.x, pNorth.y - wallHeight}, 5.0f, borderColor);
      DrawLineEx({pNorth.x, pNorth.y - wallHeight},
                 {pEast.x, pEast.y - wallHeight}, 5.0f, borderColor);
      DrawLineEx(pWest, {pWest.x, pWest.y - wallHeight}, 5.0f, borderColor);
      DrawLineEx(pNorth, {pNorth.x, pNorth.y - wallHeight}, 5.0f, borderColor);
      DrawLineEx(pEast, {pEast.x, pEast.y - wallHeight}, 5.0f, borderColor);

      // Z-sorting por Y
      struct YObj {
        float y;
        Entity *ptr;
      };
      YObj objs[2] = {{activePlayer->position.y, activePlayer},
                      {boss.position.y, &boss}};
      if (objs[0].y > objs[1].y)
        std::swap(objs[0], objs[1]);
      for (int i = 0; i < 2; i++) {
        objs[i].ptr->Draw();
      }

      for (auto &p : particles)
        DrawCircleV(p.pos, 3, p.col);
      for (auto &dtText : damageTexts)
        dtText.Draw();

      // --- INDICADOR VISUAL DE SANGRADO EN EL BOSS ---
      if (boss.isBleeding && !boss.isDead) {
        float bleedPulse = 0.5f + 0.4f * sinf((float)GetTime() * 8.0f);
        rlPushMatrix();
        rlTranslatef(boss.position.x, boss.position.y, 0);
        rlScalef(1.0f, 0.5f, 1.0f);
        DrawCircleLines(0, 0, boss.radius + 8.0f,
                        Fade({220, 30, 30, 255}, bleedPulse));
        DrawCircleLines(0, 0, boss.radius + 12.0f,
                        Fade({220, 30, 30, 255}, bleedPulse * 0.5f));
        rlPopMatrix();
        DrawText(TextFormat("BLEED %.1fs", boss.bleedTimer),
                 (int)boss.position.x - 35,
                 (int)boss.position.y - boss.radius - 65, 14,
                 {220, 50, 50, 255});
      }

      EndMode2D();
    } // end if != CHAR_SELECT

    // --- UI DE JUEGO (Vida Fantasma - Step 3) ---
    if (currentPhase == GamePhase::RUNNING ||
        currentPhase == GamePhase::PAUSED ||
        currentPhase == GamePhase::VICTORY) {
      // Vida Player
      float pctVida = activePlayer->hp / activePlayer->maxHp;
      float pctGhost = playerGhostHp / activePlayer->maxHp;

      float hudX = 20.0f, hudY = 20.0f;
      float barW = 500.0f, barH = 32.0f;

      DrawRectangle(hudX, hudY, barW, barH, Color{60, 20, 20, 255}); // Fondo
      DrawRectangle(hudX, hudY, barW * pctGhost, barH,
                    Color{250, 250, 250, 180}); // Fantasma
      DrawRectangle(hudX, hudY, barW * pctVida, barH,
                    Color{0, 220, 100, 255}); // Actual
      DrawRectangleLines(hudX, hudY, barW, barH, BLACK);

      // Overlay de textura si existe
      if (ResourceManager::texVida.id != 0) {
        DrawTexturePro(ResourceManager::texVida,
                       {0, 0, (float)ResourceManager::texVida.width * pctVida,
                        (float)ResourceManager::texVida.height},
                       {hudX, hudY, barW * pctVida, barH}, {0, 0}, 0, WHITE);
      }

      // Energia Player (barra debajo)
      float pctEnergia = activePlayer->energy / activePlayer->maxEnergy;
      float enerY = hudY + barH + 6.0f;
      float enerW = 460.0f, enerH = 14.0f;
      DrawRectangle(hudX, enerY, enerW, enerH, DARKGRAY);
      DrawRectangle(hudX, enerY, enerW * pctEnergia, enerH, ORANGE);
      DrawRectangleLines(hudX, enerY, enerW, enerH, BLACK);

      // Overlay de textura de energia si existe
      if (ResourceManager::texEnergia.id != 0) {
        DrawTexturePro(
            ResourceManager::texEnergia,
            {0, 0, (float)ResourceManager::texEnergia.width * pctEnergia,
             (float)ResourceManager::texEnergia.height},
            {hudX, enerY, enerW * pctEnergia, enerH}, {0, 0}, 0, WHITE);
      }

      DrawText(activePlayer->GetName().c_str(), (int)hudX,
               (int)(enerY + enerH + 10.0f), 18, activePlayer->GetHUDColor());
      DrawText(TextFormat("%d / %d", (int)activePlayer->hp,
                          (int)activePlayer->maxHp),
               hudX + barW - 100, hudY + 8, 16, WHITE);

      // --- VIDA BOSS (Barra Superior Derecha) ---
      float bBarW = 400.0f, bBarH = 22.0f;
      float bx = GetScreenWidth() - bBarW - 25.0f, by = 25.0f;
      float bPct = boss.hp / boss.maxHp;
      float bGhost = bossGhostHp / boss.maxHp;

      // Step 4: Visual de Furia (Cambia a rojo brillante)
      Color bossColor = (boss.hp <= boss.maxHp * 0.5f) ? RED : MAROON;

      DrawRectangle(bx, by, bBarW, bBarH, {40, 40, 40, 255});
      DrawRectangle(bx, by, bBarW * bGhost, bBarH, Fade(WHITE, 0.4f));
      DrawRectangle(bx, by, bBarW * bPct, bBarH, bossColor);
      DrawRectangleLines(bx, by, bBarW, bBarH, {20, 20, 20, 255});

      std::string bossName = (boss.hp <= boss.maxHp * 0.5f)
                                 ? "BOSS ENFURECIDO"
                                 : "GUARDIÁN DE LA ARENA";
      DrawText(bossName.c_str(), bx + bBarW - MeasureText(bossName.c_str(), 18),
               by + bBarH + 5, 18, GOLD);

      // --- HUD de Habilidades (Génerico Abajo) ---
      auto abilities = activePlayer->GetAbilities();
      float screenH = (float)GetScreenHeight();
      float iconScale = 1.8f;
      float bw = 64.0f * iconScale, bh = 64.0f * iconScale;
      float startX = 20.0f;

      for (size_t i = 0; i < abilities.size(); i++) {
        float bx = startX + i * (bw + 12.0f);
        float by = screenH - bh - 20.0f;
        Color bg = abilities[i].ready ? Fade({60, 0, 80, 255}, 0.9f)
                                      : Fade(BLACK, 0.7f);
        DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, bg);
        DrawRectangleLines((int)bx, (int)by, (int)bw, (int)bh,
                           abilities[i].ready ? abilities[i].color : DARKGRAY);
        DrawText(abilities[i].label.c_str(), (int)bx + 4, (int)by + 6, 14,
                 WHITE);
        if (abilities[i].cooldown > 0) {
          float cdPct =
              fminf(abilities[i].cooldown / abilities[i].maxCooldown, 1.0f);
          DrawRectangle((int)bx, (int)(by + bh - 8), (int)(bw * (1.0f - cdPct)),
                        8, abilities[i].color);
          DrawText(TextFormat("%.1f", abilities[i].cooldown),
                   (int)bx + (int)(bw / 2) - 15, (int)by + (int)(bh / 2) - 10,
                   20, WHITE);
        } else {
          DrawRectangle((int)bx, (int)(by + bh - 8), (int)bw, 8,
                        abilities[i].color);
        }
      }

      // Status especiales
      std::string status = activePlayer->GetSpecialStatus();
      if (!status.empty()) {
        DrawText(status.c_str(),
                 GetScreenWidth() / 2 - MeasureText(status.c_str(), 28) / 2, 30,
                 28, {255, 120, 0, 255});
      }
      if (activePlayer->IsBuffed()) {
        DrawText(TextFormat("BUFF %.1fs", activePlayer->GetBuffTimer()),
                 GetScreenWidth() / 2 - 60, 62, 20, {180, 0, 255, 255});
      }
    }

    // --- MENÚ DE PAUSA ---
    if (currentPhase == GamePhase::PAUSED) {
      DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                    Fade(BLACK, 0.7f));
      DrawText("JUEGO EN PAUSA", GetScreenWidth() / 2 - 120, 150, 30, RAYWHITE);
      Rectangle btnResume = {(float)GetScreenWidth() / 2 - 100, 250, 200, 50};
      Rectangle btnSettings = {(float)GetScreenWidth() / 2 - 100, 320, 200, 50};
      if (CheckCollisionPointRec(GetMousePosition(), btnResume)) {
        DrawRectangleRec(btnResume, GRAY);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
          currentPhase = GamePhase::RUNNING;
          ShowCursor();
        }
      } else
        DrawRectangleRec(btnResume, DARKGRAY);
      DrawText("REANUDAR", (int)btnResume.x + 45, (int)btnResume.y + 15, 20,
               WHITE);
      if (CheckCollisionPointRec(GetMousePosition(), btnSettings)) {
        DrawRectangleRec(btnSettings, GRAY);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
          currentPhase = GamePhase::SETTINGS;
      } else
        DrawRectangleRec(btnSettings, DARKGRAY);
      DrawText("AJUSTES", (int)btnSettings.x + 55, (int)btnSettings.y + 15, 20,
               WHITE);
    }

    if (currentPhase == GamePhase::VICTORY) {
      DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                    Fade(GOLD, 0.4f));
      DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                    Fade(BLACK, 0.6f));
      DrawText("¡VICTORIA!",
               GetScreenWidth() / 2 - MeasureText("¡VICTORIA!", 60) / 2, 200,
               60, GOLD);
      DrawText("HAS DERROTADO AL BOSS",
               GetScreenWidth() / 2 -
                   MeasureText("HAS DERROTADO AL BOSS", 20) / 2,
               280, 20, RAYWHITE);
      DrawText("PULSA P PARA VOLVER AL MENU",
               GetScreenWidth() / 2 -
                   MeasureText("PULSA P PARA VOLVER AL MENU", 20) / 2,
               420, 20, GRAY);

      Rectangle btnMenu = {(float)GetScreenWidth() / 2 - 120, 480, 240, 60};
      if (CheckCollisionPointRec(GetMousePosition(), btnMenu)) {
        DrawRectangleRec(btnMenu, YELLOW);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
          ResetGame(activePlayer, boss, currentPhase);
          currentPhase = GamePhase::CHAR_SELECT;
          ShowCursor();
        }
      } else
        DrawRectangleRec(btnMenu, GOLD);
      DrawText("MENU PRINCIPAL", (int)btnMenu.x + 40, (int)btnMenu.y + 18, 20,
               BLACK);
    }

    if (currentPhase == GamePhase::GAME_OVER) {
      DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                    Fade(BLACK, 0.8f));

      DrawText("HAS MUERTO",
               GetScreenWidth() / 2 - MeasureText("HAS MUERTO", 50) / 2, 180,
               50, RED);
      DrawText("PULSA P PARA REINTENTAR",
               GetScreenWidth() / 2 -
                   MeasureText("PULSA P PARA REINTENTAR", 20) / 2,
               245, 20, RAYWHITE);

      Rectangle btnRevive = {(float)GetScreenWidth() / 2 - 140, 350, 280, 60};
      Rectangle btnMenu = {(float)GetScreenWidth() / 2 - 140, 430, 280, 60};

      // Boton Revivir
      bool hoverRevive = CheckCollisionPointRec(GetMousePosition(), btnRevive);
      DrawRectangleRec(btnRevive, hoverRevive ? GRAY : DARKGRAY);
      DrawRectangleLinesEx(btnRevive, 2, WHITE);
      DrawText("REVIVIR",
               (int)btnRevive.x + btnRevive.width / 2 -
                   MeasureText("REVIVIR", 25) / 2,
               (int)btnRevive.y + 18, 25, WHITE);

      if (hoverRevive && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ResetGame(activePlayer, boss, currentPhase);
      }

      // Boton Menu de Seleccion
      bool hoverMenu = CheckCollisionPointRec(GetMousePosition(), btnMenu);
      DrawRectangleRec(btnMenu, hoverMenu ? GRAY : DARKGRAY);
      DrawRectangleLinesEx(btnMenu, 2, WHITE);
      DrawText("CAMBIAR PERSONAJE",
               (int)btnMenu.x + btnMenu.width / 2 -
                   MeasureText("CAMBIAR PERSONAJE", 20) / 2,
               (int)btnMenu.y + 20, 20, WHITE);

      if (hoverMenu && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ResetGame(activePlayer, boss, currentPhase);
        currentPhase = GamePhase::CHAR_SELECT;
        ShowCursor();
      }
    }
    // --- MENÚ DE AJUSTES ---
    if (currentPhase == GamePhase::SETTINGS ||
        currentPhase == GamePhase::REBINDING) {
      DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                    {15, 15, 20, 255});
      DrawText("AJUSTES Y CONTROLES", 100, 50, 30, GOLD);
      Rectangle btnBack = {20, 20, 130, 40};
      if (CheckCollisionPointRec(GetMousePosition(), btnBack) &&
          IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        currentPhase = GamePhase::PAUSED;
      DrawRectangleRec(btnBack, DARKGRAY);
      DrawText("< VOLVER", (int)btnBack.x + 15, (int)btnBack.y + 10, 20, WHITE);

      DrawText("RESOLUCION:", 100, 120, 20, GRAY);
      Rectangle res1 = {300, 110, 100, 40};
      Rectangle res2 = {410, 110, 100, 40};
      Rectangle res3 = {520, 110, 100, 40};
      Rectangle btnScreen = {650, 110, 220, 40};
      if (CheckCollisionPointRec(GetMousePosition(), res1) &&
          IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        SetWindowSize(1280, 720);
      if (CheckCollisionPointRec(GetMousePosition(), res2) &&
          IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        SetWindowSize(1600, 900);
      if (CheckCollisionPointRec(GetMousePosition(), res3) &&
          IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        SetWindowSize(1920, 1080);
      if (CheckCollisionPointRec(GetMousePosition(), btnScreen) &&
          IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        ToggleFullscreen();
      DrawRectangleRec(res1, DARKGRAY);
      DrawText("720p", (int)res1.x + 25, (int)res1.y + 10, 20, WHITE);
      DrawRectangleRec(res2, DARKGRAY);
      DrawText("900p", (int)res2.x + 25, (int)res2.y + 10, 20, WHITE);
      DrawRectangleRec(res3, DARKGRAY);
      DrawText("1080p", (int)res3.x + 20, (int)res3.y + 10, 20, WHITE);
      DrawRectangleRec(btnScreen, MAROON);
      DrawText(IsWindowFullscreen() ? "MODO VENTANA" : "PANTALLA COMPLETA",
               (int)btnScreen.x + 10, (int)btnScreen.y + 10, 20, WHITE);

      DrawText("CONTROLES (Click para reasignar):", 100, 200, 20, GRAY);
      auto DrawRebind = [&](const char *label, int *key, int y) {
        DrawText(label, 120, y, 20, WHITE);
        Rectangle r = {340, (float)y - 5, 150, 30};
        bool hover = CheckCollisionPointRec(GetMousePosition(), r);
        DrawRectangleRec(r, hover ? GRAY : DARKGRAY);
        DrawText(currentPhase == GamePhase::REBINDING && rebindingKey == key
                     ? "..."
                     : TextFormat("K: %i", *key),
                 (int)r.x + 10, y, 20, GOLD);
        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
          currentPhase = GamePhase::REBINDING;
          rebindingKey = key;
          rebindingName = label;
        }
      };
      DrawRebind("Dash", &activePlayer->controls.dash, 250);
      DrawRebind("Q Skill", &activePlayer->controls.boomerang, 300);
      DrawRebind("E Skill", &activePlayer->controls.berserker, 350);
      DrawRebind("R Ultimate", &activePlayer->controls.ultimate, 400);

      if (currentPhase == GamePhase::REBINDING)
        DrawText(TextFormat("Pulsa una tecla para [%s]", rebindingName.c_str()),
                 100, 500, 20, SKYBLUE);
      DrawText("PRESIONA K PARA VOLVER", 100, 650, 15, GRAY);
    }

    // =====================================================
    // --- MENÚ SELECCIÓN DE PERSONAJE ---
    // =====================================================
    if (currentPhase == GamePhase::CHAR_SELECT) {
      // Fondo oscuro con degradado
      DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                    {12, 14, 22, 255});

      // Titulo
      const char *title = "SELECCION DE PERSONAJE";
      int titleSize = 48;
      DrawText(title, GetScreenWidth() / 2 - MeasureText(title, titleSize) / 2,
               120, titleSize, GOLD);
      DrawText("Elige a tu luchador",
               GetScreenWidth() / 2 -
                   MeasureText("Elige a tu luchador", 22) / 2,
               185, 22, LIGHTGRAY);

      float cardW = 340.0f, cardH = 420.0f;
      float cardY = 260.0f;
      float gap = 80.0f;
      float totalW = cardW * 2 + gap;
      float startX = GetScreenWidth() / 2.0f - totalW / 2.0f;

      // --- Tarjeta REAPER ---
      Rectangle cardReaper = {startX, cardY, cardW, cardH};
      bool hoverReaper = CheckCollisionPointRec(GetMousePosition(), cardReaper);
      Color reaperBg =
          hoverReaper ? Color{50, 0, 75, 255} : Color{30, 0, 50, 220};
      DrawRectangleRec(cardReaper, reaperBg);
      DrawRectangleLinesEx(cardReaper, 3.0f,
                           hoverReaper ? Color{200, 0, 255, 255}
                                       : Color{80, 0, 120, 200});

      // Icono Reaper
      DrawCircleGradient((int)(startX + cardW / 2), (int)(cardY + 130), 60.0f,
                         Fade({180, 0, 255, 255}, 0.9f),
                         Fade({60, 0, 120, 255}, 0.1f));
      DrawCircleV({startX + cardW / 2, cardY + 130}, 38.0f,
                  Color{160, 0, 220, 255});
      DrawCircleLines((int)(startX + cardW / 2), (int)(cardY + 130), 42.0f,
                      Fade(WHITE, 0.5f));

      DrawText("SEGADOR",
               (int)(startX + cardW / 2) - MeasureText("SEGADOR", 30) / 2,
               (int)(cardY + 210), 30, Color{220, 80, 255, 255});
      DrawText("Maestro del sangrado",
               (int)(startX + cardW / 2) -
                   MeasureText("Maestro del sangrado", 16) / 2,
               (int)(cardY + 250), 16, LIGHTGRAY);
      DrawText("- Orbes teledirigidos", (int)(startX + 20), (int)(cardY + 285),
               15, GRAY);
      DrawText("- Explosiones de suelo", (int)(startX + 20), (int)(cardY + 308),
               15, GRAY);
      DrawText("- Ultimate: Tiempo Detenido", (int)(startX + 20),
               (int)(cardY + 331), 15, GRAY);

      Rectangle btnReaper = {startX + 30.0f, cardY + cardH - 65.0f,
                             cardW - 60.0f, 45.0f};
      Color btnRCol =
          hoverReaper ? Color{200, 0, 255, 255} : Color{100, 0, 160, 255};
      DrawRectangleRec(btnReaper, btnRCol);
      DrawRectangleLinesEx(btnReaper, 2.0f, WHITE);
      DrawText("ELEGIR",
               (int)(btnReaper.x + btnReaper.width / 2) -
                   MeasureText("ELEGIR", 22) / 2,
               (int)(btnReaper.y + 10), 22, WHITE);

      if (hoverReaper && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        reaper.Reset({2000, 2000});
        activePlayer = &reaper;
        camera.target = activePlayer->position;
        currentPhase = GamePhase::RUNNING;
      }

      // --- Tarjeta ROPERA ---
      float ropCardX = startX + cardW + gap;
      Rectangle cardRopera = {ropCardX, cardY, cardW, cardH};
      bool hoverRopera = CheckCollisionPointRec(GetMousePosition(), cardRopera);
      Color roperaBg =
          hoverRopera ? Color{0, 55, 50, 255} : Color{0, 30, 28, 220};
      DrawRectangleRec(cardRopera, roperaBg);
      DrawRectangleLinesEx(cardRopera, 3.0f,
                           hoverRopera ? Color{0, 220, 180, 255}
                                       : Color{0, 80, 70, 200});

      // Icono Ropera
      DrawCircleGradient((int)(ropCardX + cardW / 2), (int)(cardY + 130), 60.0f,
                         Fade({0, 230, 180, 255}, 0.9f),
                         Fade({0, 80, 60, 255}, 0.1f));
      DrawCircleV({ropCardX + cardW / 2, cardY + 130}, 38.0f,
                  Color{0, 180, 160, 255});
      DrawCircleLines((int)(ropCardX + cardW / 2), (int)(cardY + 130), 42.0f,
                      Fade(WHITE, 0.5f));
      // Espada decorativa
      DrawRectanglePro(
          {ropCardX + cardW / 2 - 4.0f, cardY + 95.0f, 8.0f, 70.0f},
          {4.0f, 35.0f}, 0.0f, Fade(GOLD, 0.85f));
      DrawRectanglePro(
          {ropCardX + cardW / 2 - 18.0f, cardY + 133.0f, 36.0f, 6.0f},
          {18.0f, 3.0f}, 0.0f, Fade(LIGHTGRAY, 0.8f));

      DrawText("ROPERA",
               (int)(ropCardX + cardW / 2) - MeasureText("ROPERA", 30) / 2,
               (int)(cardY + 210), 30, Color{0, 220, 180, 255});
      DrawText("Duelista veloz",
               (int)(ropCardX + cardW / 2) -
                   MeasureText("Duelista veloz", 16) / 2,
               (int)(cardY + 250), 16, LIGHTGRAY);
      DrawText("- Combos de estocadas", (int)(ropCardX + 20),
               (int)(cardY + 285), 15, GRAY);
      DrawText("- Buff de velocidad Q", (int)(ropCardX + 20),
               (int)(cardY + 308), 15, GRAY);
      DrawText("- Ultimate: Modo Garras", (int)(ropCardX + 20),
               (int)(cardY + 331), 15, GRAY);

      Rectangle btnRopera = {ropCardX + 30.0f, cardY + cardH - 65.0f,
                             cardW - 60.0f, 45.0f};
      Color btnRoCol =
          hoverRopera ? Color{0, 220, 180, 255} : Color{0, 100, 80, 255};
      DrawRectangleRec(btnRopera, btnRoCol);
      DrawRectangleLinesEx(btnRopera, 2.0f, WHITE);
      DrawText("ELEGIR",
               (int)(btnRopera.x + btnRopera.width / 2) -
                   MeasureText("ELEGIR", 22) / 2,
               (int)(btnRopera.y + 10), 22, WHITE);

      if (hoverRopera && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ropera.Reset({2000, 2000});
        activePlayer = &ropera;
        camera.target = activePlayer->position;
        currentPhase = GamePhase::RUNNING;
      }
    }
    EndDrawing();
  }
  ResourceManager::Unload();
  CloseWindow();
  return 0;
}