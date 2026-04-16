// =====================================================================
// GameplayScene.cpp - Escena Principal de Combate
// =====================================================================
// Encapsula toda la lógica que antes vivía en GamePhase::RUNNING,
// GamePhase::GAME_OVER y GamePhase::VICTORY dentro de main.cpp.
// =====================================================================
#include "../include/scenes/GameplayScene.h"
#include "../include/EliteEnemies.h"
#include "../include/EtherCorrupto.h"
#include "../include/graphics/RenderManager.h"
#include "../include/graphics/VFXSystem.h"
#include "../include/scenes/MainMenuScene.h"
#include "../include/scenes/PauseScene.h"
#include "../include/scenes/SceneManager.h"
#include "rlgl.h"
#include <raylib.h>
#include <raymath.h>

// ─── Variables globales compartidas: DEFINIDAS en main.cpp ───────────────────
// Se declaran extern aquí para que el linker las enlace con las de main.cpp.
extern bool isTimeStopped;
extern bool showCursorInGame;
extern float hitstopTimer;
extern float screenShake;
extern float g_timeScale;
extern double g_gameTime;
extern ResourceManager res;
extern const char* G_BUILD_VERSION;
// Ya definidos en CommonTypes.h, quitamos duplicados locales si molestan, pero
// mantengo consistencia

// Las variables de entidad también viven en main.cpp (fuera de namespace
// Scenes)
extern Reaper g_reaper;
extern Ropera g_ropera;
extern Player *g_activePlayer;

namespace Scenes {

// ─── Init ─────────────────────────────────────────────────────────────────
void GameplayScene::Init() {
  m_playerGhostHp = m_activePlayer->hp;

  // ── Limpiar estado previo completamente ──────────────────────────
  // Eliminar todos los enemigos vivos de la oleada anterior
  for (auto *b : m_aliveBosses) {
    delete b;
  }
  m_aliveBosses.clear();
  m_boss = nullptr;

  // Limpiar cola anterior si existiera
  while (!m_bossQueue.empty()) {
    delete m_bossQueue.front();
    m_bossQueue.pop();
  }

  // Reiniciar estado de oleada
  m_currentWave = 1;
  m_bossXpAwarded = false;
  m_nextBossTimer = 0.0f;
  m_showLevelUpMenu = false;

  // Reiniciar estado de victoria y cronómetro
  m_isVictory = false;
  m_victoryTimer = 0.0f;
  m_totalGameTime = 0.0f;
  m_waveTextTimer = 0.0f;
  m_waveText = "";
  m_waveSubText = "";

  // Anuncio de bienvenida a la oleada 1
  m_waveText    = "OLEADA 1";
  m_waveSubText = "EL GUARDIAN DE LA ARENA";
  m_waveTextIsStart = true;
  m_waveTextTimer   = 3.5f;

  // Llenar cola inicial: Golem -> ELITE WAVE -> etc.
  m_boss = new Enemy({2000, 1600}); // Lejos del jugador para no aparecer encima
  m_aliveBosses.push_back(m_boss);
  m_bossGhostHp = m_boss->hp;

  m_damageTexts.clear();

  // ── Inicializar mapa isométrico de pasto ────────────────────────
  IsoMap::InitDefaultMap(m_isoMap);

  m_isoMapOffset = {2000.0f, 1300.0f};

  m_camera.target = m_activePlayer->position;
  m_camera.offset = {(float)GetScreenWidth() * 0.5f,
                     (float)GetScreenHeight() * 0.5f};
  m_camera.rotation = 0.0f;
  m_camera.zoom = 1.35f;

  isTimeStopped = false;
  hitstopTimer = 0.0f;
  screenShake = 0.0f;

  Graphics::VFXSystem::GetInstance().Clear();
  AnimeVFX::AnimeTrailSystem::Get().ClearAll();

  // ── Inicializar pipeline de post-procesado ─────────────────────
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  AnimeVFX::PostProcessPipeline::Get().Init(sw, sh);

  // ── Registrar trails por personaje ────────────────────────────
  //    Cada personaje tiene un color de estela unico
  m_trailIdReaper = AnimeVFX::AnimeTrailSystem::Get().Register({180,   0, 255, 255}, 32.0f); // Purpura
  m_trailIdRopera = AnimeVFX::AnimeTrailSystem::Get().Register({  0, 230, 180, 255}, 28.0f); // Cian
  m_trailIdMage   = AnimeVFX::AnimeTrailSystem::Get().Register({ 80, 180, 255, 255}, 24.0f); // Azul
  m_trailIdPlayer = m_trailIdReaper; // default

  // ── Sistema de ambiente: arena diamante de 1400px radio ───────
  AnimeVFX::AmbientSystem::Get().Init({ 2000, 2000 }, 1400.0f);

  m_prevDashCharges = (float)m_activePlayer->dashCharges;

  if (showCursorInGame)
    ShowCursor();
  else
    HideCursor();

  m_showVersion = false;
  m_versionTimer = 0.0f;
}

// ─── Update
// ─────────────────────────────────────────────────────────────────────
void GameplayScene::Update(float dt) {
  // ── Victoria: el juego está congelado, solo mostrar la pantalla ──────────
  if (m_isVictory) {
    m_victoryTimer += dt;
    // Permitir volver al menu o reiniciar
    if (IsKeyPressed(KEY_R)) { Init(); }
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_K)) {
      SceneManager::Get().ChangeScene(std::make_unique<MainMenuScene>());
    }
    return;
  }

  // ── Menú de Nivel abierto: el mundo no avanza, solo se procesa el modal ──
  if (m_showLevelUpMenu) {
    ShowCursor();
    return;
  }

  // ── Pausa
  if (IsKeyPressed(KEY_K) || IsKeyPressed(KEY_ESCAPE)) {
    SceneManager::Get().PushOverlay(std::make_unique<PauseScene>());
    return;
  }

  // ── Reinicio rápido ────────────────────────────────────────────────────
  if (IsKeyPressed(KEY_P)) {
    Init();
    return;
  }

  // ── Verificación de Versión (F5) ──────────────────────────────────────
  if (IsKeyPressed(KEY_F5)) {
    m_showVersion = !m_showVersion;
    m_versionTimer = 3.0f;
  }

  if (IsKeyPressed(KEY_F4)) {
    m_showDebugHitboxes = !m_showDebugHitboxes;
  }
  if (m_versionTimer > 0) m_versionTimer -= dt;
  else if (m_showVersion) m_showVersion = false;

  // ── Cronómetro total ───────────────────────────────────────────────────
  m_totalGameTime += dt;

  // ── Anuncio de oleada: tick ────────────────────────────────────────────
  if (m_waveTextTimer > 0) m_waveTextTimer -= dt;

  // ── Timers globales ────────────────────────────────────────────────────
  if (hitstopTimer > 0) hitstopTimer -= dt;
  if (screenShake > 0)  screenShake -= dt * 2.5f;
  if (screenShake > 4.0f) screenShake = 4.0f;

  // g_timeScale handled smoothly
  g_timeScale = Lerp(g_timeScale, (m_activePlayer->hp <= 0.0f) ? 0.2f : 1.0f, 5.0f * dt);

  UpdateCamera(dt);

  // Capturar posición del mouse en coordenadas de mundo
  m_activePlayer->targetAim = GetScreenToWorld2D(GetMousePosition(), m_camera);

  // ── Lógica de juego (no durante hitstop) ──────────────────────────────
  if (hitstopTimer <= 0) {
    m_activePlayer->Update();
    if (m_boss)
      m_activePlayer->HandleSkills(*m_boss);
  }

  UpdateBleedDoT(dt);

  UpdateBossRush(dt);

  for (auto *b : m_aliveBosses) {
    if (b && !b->isDead && !isTimeStopped) {
      b->UpdateAI(*m_activePlayer);
    }
    if (b && !isTimeStopped) {
      b->UpdateBossStatus(dt);
      b->Update();
    }
  }
  // TODO: UpdateRocks for all enemies if they have them,
  // currently only Enemy (Golem) has them.
  if (m_boss && dynamic_cast<Enemy *>(m_boss)) {
    UpdateRocks(dt);
  }

  // ── Separación de cuerpos ──────────────────────────────────────────────
  for (auto *b : m_aliveBosses) {
    if (b && !b->isDead) {
      float dist = Vector2Distance(m_activePlayer->position, b->position);
      float minDist = 20.0f + b->radius;
      if (!m_activePlayer->IsImmune() && dist < minDist && dist > 0) {
        Vector2 push = Vector2Normalize(
            Vector2Subtract(m_activePlayer->position, b->position));
        float ov = minDist - dist;
        m_activePlayer->position =
            Vector2Add(m_activePlayer->position, Vector2Scale(push, ov * 0.5f));
        b->position =
            Vector2Subtract(b->position, Vector2Scale(push, ov * 0.5f));
      }
    }
  }

  // ── Ghost HP lerp ──────────────────────────────────────────────────────
  m_playerGhostHp = Lerp(m_playerGhostHp, m_activePlayer->hp, 3.0f * dt);
  if (m_boss)
    m_bossGhostHp = Lerp(m_bossGhostHp, m_boss->hp, 3.0f * dt);

  // ── Fase Furia del Boss ────────────────────────────────────────────────
  if (m_boss && m_boss->hp <= m_boss->maxHp * 0.5f && !m_boss->isDead) {
    // Si es un Enemy normal, aplicar furia
    if (Enemy *en = dynamic_cast<Enemy *>(m_boss)) {
      en->aggressionLevel = 1.6f;
      en->baseAttackCooldown = en->baseAttackCooldown * 0.8f;
    }
  }

  // ── Colisiones ────────────────────────────────────────────────────────
  UpdateCollisions();

  // ── Partículas y textos flotantes ─────────────────────────────────────
  Graphics::VFXSystem::GetInstance().Update(dt);
  for (auto &t : m_damageTexts)
    t.Update(dt);
  m_damageTexts.erase(
      std::remove_if(m_damageTexts.begin(), m_damageTexts.end(),
                     [](const DamageText &t) { return t.life <= 0; }),
      m_damageTexts.end());

  // ── AnimeVFX: actualizar pipelines, speed lines, ambiente y trails ────
  AnimeVFX::PostProcessPipeline::Get().Update(dt, screenShake, hitstopTimer);
  AnimeVFX::SpeedLineSystem::Get().Update(dt);
  AnimeVFX::AmbientSystem::Get().Update(dt, m_activePlayer->position);

  // Detectar uso de dash -> burst de speed lines con color del personaje
  {
    float curCharges = (float)m_activePlayer->dashCharges;
    if (curCharges < m_prevDashCharges) {
      AnimeVFX::SpeedLineSystem::Get().Burst(
          m_activePlayer->position, m_activePlayer->GetHUDColor(), 24);
    }
    m_prevDashCharges = curCharges;
  }

  // Weapon trail: solo durante el frame activo del ataque
  {
    int trailId;
    if      (m_activePlayer == (Player*)(&g_reaper))  trailId = m_trailIdReaper;
    else if (m_activePlayer == (Player*)(&g_ropera))  trailId = m_trailIdRopera;
    else                                              trailId = m_trailIdMage;
    m_trailIdPlayer = trailId;

    bool isAttacking = (m_activePlayer->attackPhase == AttackPhase::ATTACK_ACTIVE);
    if (isAttacking) {
      Vector2 base = m_activePlayer->position;
      Vector2 tip  = Vector2Add(base, Vector2Scale(m_activePlayer->facing, 140.0f));
      AnimeVFX::AnimeTrailSystem::Get().Push(trailId, base, tip);
    } else {
      AnimeVFX::AnimeTrailSystem::Get().Clear(trailId);
    }
  }

  // ── Comprobación de fin de partida
  // ──────────────────────────────────────────
  UpdateDeathCheck();

  // ── Paso B: Recompensa de muerte del Boss ──────────────────────────────
  UpdateBossDeathReward();
}

// ─── Draw
// ──────────────────────────────────────────────────────────────────────
void GameplayScene::Draw() {
  DrawWorld();
  DrawHUD();
  DrawWaveAnnouncement();
  if (m_showLevelUpMenu)
    DrawLevelUpMenu();
  if (m_isVictory)
    DrawVictoryScreen();
}

// ─── Unload ───────────────────────────────────────────────────────────────
void GameplayScene::Unload() {
  // Liberar todos los enemigos vivos para evitar memory leaks y punteros
  // colgantes si la escena se descarga o se cambia
  for (auto *b : m_aliveBosses) {
    delete b;
  }
  m_aliveBosses.clear();
  m_boss = nullptr;

  while (!m_bossQueue.empty()) {
    delete m_bossQueue.front();
    m_bossQueue.pop();
  }

  m_damageTexts.clear();
  Graphics::VFXSystem::GetInstance().Clear();
  AnimeVFX::AnimeTrailSystem::Get().ClearAll();
  AnimeVFX::PostProcessPipeline::Get().Unload();
  ShowCursor();
}

void GameplayScene::UpdateBossRush(float dt) {
  if (m_aliveBosses.empty()) return;

  // Comprobar si TODOS los enemigos actuales han muerto
  bool allDead = true;
  for (auto* b : m_aliveBosses) {
    if (b && !b->isDead) { allDead = false; break; }
  }
  if (!allDead) return;

  // Esperar a que UpdateBossDeathReward haya procesado XP/stat points
  if (!m_bossXpAwarded) return;

  // Mostrar "WAVE CLEAR" una sola vez (cuando el timer aun no ha bajado del dia)
  if (m_waveTextTimer <= 0.0f && !m_isVictory) {
    m_waveText     = TextFormat("OLEADA %d COMPLETADA", m_currentWave);
    m_waveSubText  = "PREPARATE...";
    m_waveTextIsStart = false;
    m_waveTextTimer = 3.0f;
  }

  // Countdown para el proximo spawn
  m_nextBossTimer -= dt;
  if (m_nextBossTimer > 0.0f) return;

  // ── Limpiar enemigos muertos de forma segura ─────────────────────
  for (auto* b : m_aliveBosses) { delete b; }
  m_aliveBosses.clear();
  m_boss = nullptr;

  // ── Spawn siguiente oleada ───────────────────────────────────────
  if (m_currentWave == 1) {
    // Oleada 2: Horda de élites (sin jefe, sin barra de HP superior)
    m_currentWave = 2;
    m_aliveBosses.push_back(new GreatswordElite({2000, 2000}));
    m_aliveBosses.push_back(new SimpleKnight({1800, 2000}));
    m_aliveBosses.push_back(new SimpleKnight({2200, 2000}));
    m_aliveBosses.push_back(new SimplyArcher({1300, 1650}));
    m_aliveBosses.push_back(new SimplyArcher({2700, 2350}));
    m_boss = nullptr; // Sin jefe designado en wave 2

    m_waveText    = "OLEADA 2";
    m_waveSubText = "HORDA DE ÉLITE";
    m_waveTextIsStart = true;
    m_waveTextTimer   = 3.5f;

  } else if (m_currentWave == 2) {
    // Oleada 3: Boss Final - Ether Corrupto
    m_currentWave = 3;
    EtherCorrupto* ec = new EtherCorrupto({2000, 2000});
    m_boss = ec;
    m_aliveBosses.push_back(m_boss);
    m_bossGhostHp = m_boss->hp;

    m_waveText    = "OLEADA FINAL";
    m_waveSubText = "ETHER CORRUPTO DESPIERTA";
    m_waveTextIsStart = true;
    m_waveTextTimer   = 4.0f;

    screenShake = 5.0f;
    AnimeVFX::PostProcessPipeline::Get().SpawnFlash(0.3f);

  } else {
    // Todas las oleadas completadas → Victoria
    m_isVictory = true;
    ShowCursor();
    g_timeScale = 0.0f;
    return;
  }

  // ── Escalar dificultad y reiniciar estado ────────────────────────
  for (auto* b : m_aliveBosses) {
    if (b) b->ScaleDifficulty(m_currentWave);
  }
  m_bossXpAwarded = false;
  if (m_boss) m_bossGhostHp = m_boss->hp;
  m_nextBossTimer = 5.0f;
  isTimeStopped   = false;
}

// ─── UpdateCamera ─────────────────────────────────────────────────────────
void GameplayScene::UpdateCamera(float dt) {
  m_camera.offset = {(float)GetScreenWidth() * 0.5f,
                     (float)GetScreenHeight() * 0.5f};
  float lerpCoeff = 10.0f * dt;
  m_camera.target.x +=
      (m_activePlayer->position.x - m_camera.target.x) * lerpCoeff;
  m_camera.target.y +=
      ((m_activePlayer->position.y - 40.0f) - m_camera.target.y) * lerpCoeff;
}

// ─── UpdateBleedDoT ───────────────────────────────────────────────────────
void GameplayScene::UpdateBleedDoT(float dt) {
  if (isTimeStopped)
    return;

  for (auto *b : m_aliveBosses) {
    if (!b || !b->isBleeding || b->isDead)
      continue;

    b->bleedTimer -= dt;
    b->bleedTickTimer -= dt;

    if (b->bleedTickTimer <= 0) {
      b->bleedTickTimer = 0.5f;      // Tick cada 0.5s
      float dmg = b->maxHp * 0.015f; // 1.5% de vida máxima
      b->hp -= dmg;

      // Partículas de sangre
      for (int i = 0; i < 3; i++) {
        Graphics::VFXSystem::GetInstance().SpawnParticle(
            b->position,
            {(float)GetRandomValue(-100, 100),
             (float)GetRandomValue(-100, 100)},
            0.6f, {255, 0, 0, 255});
      }
    }
    if (b->bleedTimer <= 0) {
      b->bleedTimer = 0;
      b->isBleeding = false;
    }
  }
}

// ─── UpdateRocks ──────────────────────────────────────────────────────────
void GameplayScene::UpdateRocks(float dt) {
  if (!m_boss)
    return;
  Enemy *en = dynamic_cast<Enemy *>(m_boss);
  if (!en)
    return;

  for (int i = 0; i < en->rocksSpawned; i++) {
    if (!en->rocks[i].active)
      continue;
    en->rocks[i].fallTimer -= dt;
    if (en->rocks[i].fallTimer <= 0) {
      en->rocks[i].active = false;

      Vector2 diff =
          Vector2Subtract(m_activePlayer->position, en->rocks[i].position);
      float dist = sqrtf(diff.x * diff.x + (diff.y * 2.0f) * (diff.y * 2.0f));
      if (dist <= 60.0f + m_activePlayer->radius &&
          !m_activePlayer->IsImmune()) {
        m_activePlayer->hp -= 20.0f;
        screenShake = fmaxf(screenShake, 0.8f); // Reducido (era 1.2)
      }
      for (int k = 0; k < 10; k++) {
        Graphics::VFXSystem::GetInstance().SpawnParticle(
            en->rocks[i].position,
            {(float)GetRandomValue(-200, 200),
             (float)GetRandomValue(-200, 200)},
            0.5f, DARKGRAY);
      }
    }
  }
}

// ─── UpdateCollisions ─────────────────────────────────────────────────────
void GameplayScene::UpdateCollisions() {
  if (hitstopTimer > 0 || m_aliveBosses.empty())
    return;

  for (auto *b : m_aliveBosses) {
    if (!b || b->isDead)
      continue;

    float prevHp = b->hp;
    m_activePlayer->CheckCollisions(*b);
    float dmg = prevHp - b->hp;

    if (dmg > 0) {
      // Crítico
      bool isCrit = m_activePlayer->rpg.RollCrit();
      if (isCrit) {
        float critExtra = dmg * (RPGStats::CRIT_MULTIPLIER - 1.0f);
        b->hp -= critExtra;
        dmg += critExtra;
      }

      // Texto de daño
      Color baseColor;
      switch (m_activePlayer->lastDamageType) {
      case Player::DamageType::PHYSICAL:
        baseColor = {255, 60, 60, 255};
        break;
      case Player::DamageType::MAGICAL:
        baseColor = {0, 220, 255, 255};
        break;
      case Player::DamageType::MIXED:
        baseColor = {200, 80, 255, 255};
        break;
      case Player::DamageType::DoT:
        baseColor = {160, 0, 160, 255};
        break;
      default:
        baseColor = WHITE;
        break;
      }
      Color finalColor = isCrit ? GOLD : baseColor;

      m_damageTexts.push_back({b->position,
                               {(float)GetRandomValue(-40, 40), -120.0f},
                               isCrit ? 1.3f : 1.0f,
                               isCrit ? 1.3f : 1.0f,
                               (int)dmg,
                               finalColor});

      if (isCrit) {
        m_damageTexts.push_back({{b->position.x, b->position.y - 30.0f},
                                 {(float)GetRandomValue(-20, 20), -90.0f},
                                 0.9f,
                                 0.9f,
                                 -1,
                                 {255, 240, 60, 255}});
      }

      screenShake = fmaxf(screenShake, dmg * 0.008f + (isCrit ? 1.0f : 0.0f)); // Reducido (era 0.012 y 1.5)
      b->hitFlashTimer = 0.18f;
      if (isCrit) {
        hitstopTimer = 0.10f;
        // Flash blanco + Ripple de shockwave en el punto de impacto
        AnimeVFX::PostProcessPipeline::Get().SpawnFlash(0.05f);
        AnimeVFX::PostProcessPipeline::Get().SpawnRipple(b->position, 0.45f);
      }

      Graphics::SpawnImpactBurst(b->position, m_activePlayer->facing,
                                 finalColor, WHITE, isCrit ? 20 : 12,
                                 isCrit ? 10 : 6);

      if (isCrit) {
        Graphics::SpawnHitFlash(b->position, 120.0f, Fade(finalColor, 0.4f));
      }
    }
  }
}

// ─── UpdateDeathCheck ─────────────────────────────────────────────────────
void GameplayScene::UpdateDeathCheck() {
  bool anyBossAlive = false;
  for (auto *b : m_aliveBosses)
    if (b && !b->isDead)
      anyBossAlive = true;

  if (m_activePlayer->hp <= 0.0f || !anyBossAlive) {
    ShowCursor();
  }
}

// ─── DrawWorld ────────────────────────────────────────────────────────────
void GameplayScene::DrawWorld() {
  auto& postProc = AnimeVFX::PostProcessPipeline::Get();

  Camera2D shakeCam = m_camera;
  if (screenShake > 0) {
    shakeCam.offset.x += (float)GetRandomValue(-1, 1) * screenShake;
    shakeCam.offset.y += (float)GetRandomValue(-1, 1) * screenShake;
  }

  // ── 1. Redirigir dibujado del mundo al buffer de post-proceso ─────
  bool usePP = postProc.IsReady();
  if (usePP) BeginTextureMode(postProc.GetTarget());
  ClearBackground({22, 26, 36, 255});

  BeginMode2D(shakeCam);

  // ── 1. Suelo isométrico con tiles de pasto ───────────────────────
  // Se dibuja primero (sin Z-sort) para que todo lo demás vaya encima.
  IsoMap::DrawIsoMap(m_isoMap, ResourceManager::texPasto, m_isoMapOffset);

  // Partículas de ambiente (polvo/aire de la arena, antes de entidades)
  AnimeVFX::AmbientSystem::Get().Draw();

  // ── 2. Arena (suelo sólido + paredes) ────────────────────────────
  DrawArena();

  // Speed Lines del dash (en espacio mundo, sobre el suelo)
  AnimeVFX::SpeedLineSystem::Get().Draw();

  // --- SISTEMA DE SOMBRAS PROYECTADAS (SOL DESDE NW) ---
  auto DrawProjectedShadow = [](Vector2 pos, float r) {
    Graphics::RenderManager::GetInstance().Submit(
        pos.y - 1.0f,
        [pos, r]() {
          // Sombra ovalada e inclinada para simular sol direccional
          rlPushMatrix();
          rlTranslatef(pos.x + 15, pos.y + 10,
                       0); // Desplazamiento de la sombra
          rlScalef(1.4f, 0.6f, 1.0f);
          rlRotatef(-20.0f, 0, 0, 1);
          DrawCircle(0, 0, r, Fade(BLACK, 0.4f));
          rlPopMatrix();
        },
        Graphics::RenderLayer::BACKGROUND);
  };

  DrawProjectedShadow(m_activePlayer->position, m_activePlayer->radius);
  if (m_boss && !m_boss->isDead)
    DrawProjectedShadow(m_boss->position, m_boss->radius);

  // --- INDICADORES DE HURTBOX (HITBOX DE COLISIÓN) ---
  auto DrawHurtbox = [](Vector2 pos, float r, Color col) {
    Graphics::RenderManager::GetInstance().Submit(
        pos.y - 0.5f,
        [pos, r, col]() {
          rlPushMatrix();
          rlTranslatef(pos.x, pos.y, 0);
          rlScalef(1.0f, 0.5f, 1.0f);
          // Anillo nítido que representa el radio de colisión exacto
          DrawCircleLines(0, 0, r, Fade(col, 0.7f));
          // Brillo sutil externo
          DrawCircleLines(0, 0, r + 1.0f, Fade(col, 0.3f));
          rlPopMatrix();
        },
        Graphics::RenderLayer::BACKGROUND);
  };

  DrawHurtbox(m_activePlayer->position, m_activePlayer->radius, WHITE);
  for (auto *b : m_aliveBosses) {
    if (b && !b->isDead)
      DrawHurtbox(b->position, b->radius, RED);
  }

  if (m_showDebugHitboxes) {
    Graphics::RenderManager::GetInstance().Submit(
        10000.0f, // UI layer
        []() {
          DrawText("MODO DEBUG: HITBOXES ISOMÉTRICAS ACTIVAS (Y*2.0)", 10, 10, 20, YELLOW);
        },
        Graphics::RenderLayer::UI);
  }
  Graphics::RenderManager::GetInstance().Submit(
      m_activePlayer->GetZDepth(), [this]() { m_activePlayer->Draw(); });

  for (auto *b : m_aliveBosses) {
    if (b && !b->isDead) {
      Graphics::RenderManager::GetInstance().Submit(b->GetZDepth(),
                                                    [b]() { b->Draw(); });
    }
  }

  Graphics::VFXSystem::GetInstance().SubmitDraws();

  // Textos de daño flotantes (en espacio mundo)
  for (auto &t : m_damageTexts) {
    Graphics::RenderManager::GetInstance().Submit(
        t.position.y, [t]() mutable { t.Draw(); }, Graphics::RenderLayer::VFX);
  }

  // ── Anime Weapon Trails (sobre entidades, debajo de UI) ─────────────
  Graphics::RenderManager::GetInstance().Submit(
      99999.0f, // siempre encima de todo lo del mundo
      []() { AnimeVFX::AnimeTrailSystem::Get().Draw(); },
      Graphics::RenderLayer::VFX,
      BLEND_ADDITIVE); // Modo aditivo = look energético

  // Indicador de sangrado
  if (m_boss && m_boss->isBleeding && !m_boss->isDead) {
    Graphics::RenderManager::GetInstance().Submit(
        m_boss->position.y - 10.0f,
        [this]() {
          float pulse = 0.5f + 0.4f * sinf((float)g_gameTime * 8.0f);
          rlPushMatrix();
          rlTranslatef(m_boss->position.x, m_boss->position.y, 0);
          rlScalef(1.0f, 0.5f, 1.0f);
          DrawCircleLines(0, 0, m_boss->radius + 8.0f,
                          Fade({220, 30, 30, 255}, pulse));
          DrawCircleLines(0, 0, m_boss->radius + 12.0f,
                          Fade({220, 30, 30, 255}, pulse * 0.5f));
          rlPopMatrix();
          DrawText(TextFormat("BLEED %.1fs", m_boss->bleedTimer),
                   (int)m_boss->position.x - 35,
                   (int)(m_boss->position.y - m_boss->radius - 65.0f), 14,
                   {220, 50, 50, 255});
        },
        Graphics::RenderLayer::VFX);
  }

  Graphics::RenderManager::GetInstance().Render();

  EndMode2D();

  // ── Aplicar post-proceso sobre el buffer (fuera del BeginMode2D) ─────
  if (usePP) {
    EndTextureMode();
    postProc.DrawToScreen(shakeCam);
  }
}

// ─── DrawArena ────────────────────────────────────────────────────────────
void GameplayScene::DrawArena() {
  Vector2 pN = {2000, 1300}, pS = {2000, 2700};
  Vector2 pE = {3400, 2000}, pW = {600, 2000};
  float wh = 160.0f; // Un poco más altas para que luzcan mejor

  // 1. SUELO (Capa BACKGROUND)
  Graphics::RenderManager::GetInstance().Submit(
      0.0f,
      [pN, pS, pE, pW]() {
        if (ResourceManager::texSuelo.id != 0) {
          float scale = 0.002f;

          rlSetTexture(ResourceManager::texSuelo.id);
          rlBegin(RL_TRIANGLES);
          rlColor4ub(255, 255, 255, 255);

          // Triángulo Norte-Oeste-Este
          rlTexCoord2f(pW.x * scale, pW.y * scale);
          rlVertex2f(pW.x, pW.y);
          rlTexCoord2f(pN.x * scale, pN.y * scale);
          rlVertex2f(pN.x, pN.y);
          rlTexCoord2f(pE.x * scale, pE.y * scale);
          rlVertex2f(pE.x, pE.y);

          // Triángulo Sur-Oeste-Este
          rlTexCoord2f(pW.x * scale, pW.y * scale);
          rlVertex2f(pW.x, pW.y);
          rlTexCoord2f(pE.x * scale, pE.y * scale);
          rlVertex2f(pE.x, pE.y);
          rlTexCoord2f(pS.x * scale, pS.y * scale);
          rlVertex2f(pS.x, pS.y);

          rlEnd();
          rlSetTexture(0);

          // --- SOMBRAS ARROJADAS POR LA PARED ---
          Color shadowCol = Fade(BLACK, 0.4f);
          Vector2 shadowOffset = {140, 70};

          // Sombra de la pared Noroeste
          Vector2 pW_s = Vector2Add(pW, shadowOffset);
          Vector2 pN_s = Vector2Add(pN, shadowOffset);

          rlBegin(RL_TRIANGLES);
          rlColor4ub(shadowCol.r, shadowCol.g, shadowCol.b, shadowCol.a);
          rlVertex2f(pW.x, pW.y);
          rlVertex2f(pN_s.x, pN_s.y);
          rlVertex2f(pN.x, pN.y);
          rlVertex2f(pW.x, pW.y);
          rlVertex2f(pW_s.x, pW_s.y);
          rlVertex2f(pN_s.x, pN_s.y);

          // Sombra de la pared Noreste
          Vector2 pE_s = Vector2Add(pE, shadowOffset);
          rlVertex2f(pN.x, pN.y);
          rlVertex2f(pE_s.x, pE_s.y);
          rlVertex2f(pE.x, pE.y);
          rlVertex2f(pN.x, pN.y);
          rlVertex2f(pN_s.x, pN_s.y);
          rlVertex2f(pE_s.x, pE_s.y);
          rlEnd();

        } else {
          DrawTriangle(pW, pN, pE, {210, 210, 215, 255});
          DrawTriangle(pW, pE, pS, {210, 210, 215, 255});
        }
      },
      Graphics::RenderLayer::BACKGROUND);

  // 2. PAREDES (Capa WORLD)
  auto DrawWallSegment = [wh](Vector2 b1, Vector2 b2, Color tint) {
    Graphics::RenderManager::GetInstance().Submit(
        fminf(b1.y, b2.y) - 5.0f,
        [b1, b2, wh, tint]() {
          if (ResourceManager::texPared.id != 0) {
            float len = Vector2Distance(b1, b2);
            rlSetTexture(ResourceManager::texPared.id);
            rlBegin(RL_QUADS);
            rlColor4ub(tint.r, tint.g, tint.b, tint.a);
            rlTexCoord2f(0, 1);
            rlVertex2f(b1.x, b1.y);
            rlTexCoord2f(len / 256.0f, 1);
            rlVertex2f(b2.x, b2.y);
            rlTexCoord2f(len / 256.0f, 0);
            rlVertex2f(b2.x, b2.y - wh);
            rlTexCoord2f(0, 0);
            rlVertex2f(b1.x, b1.y - wh);
            rlEnd();
            rlSetTexture(0);
          } else {
            DrawQuad(b1, b2, {b2.x, b2.y - wh}, {b1.x, b1.y - wh}, tint);
          }
          DrawLineEx({b1.x, b1.y - wh}, {b2.x, b2.y - wh}, 3.0f,
                     {50, 50, 60, 255});
        },
        Graphics::RenderLayer::WORLD);
  };

  const int segments = 12;
  for (int i = 0; i < segments; i++) {
    float t1 = (float)i / segments;
    float t2 = (float)(i + 1) / segments;

    // Pared Noroeste (W -> N)
    Vector2 w1 = Vector2Lerp(pW, pN, t1);
    Vector2 w2 = Vector2Lerp(pW, pN, t2);
    DrawWallSegment(w1, w2, WHITE);

    // Pared Noreste (N -> E)
    Vector2 e1 = Vector2Lerp(pN, pE, t1);
    Vector2 e2 = Vector2Lerp(pN, pE, t2);
    DrawWallSegment(e1, e2, ColorBrightness(WHITE, -0.15f));
  }

  // Bordes de la arena en el suelo
  Graphics::RenderManager::GetInstance().Submit(
      2700.0f,
      [pN, pE, pS, pW]() {
        Color border = {60, 65, 75, 255};
        DrawLineEx(pE, pS, 5.0f, border);
        DrawLineEx(pS, pW, 5.0f, border);
      },
      Graphics::RenderLayer::BACKGROUND);

  // 3. VERSION CHECK (Overlay temporal)
  if (m_showVersion || m_versionTimer > 0) {
    DrawText(G_BUILD_VERSION, 20, GetScreenHeight() - 40, 20, Fade(GOLD, m_versionTimer > 1.0f ? 1.0f : m_versionTimer));
  }
}

// ─── DrawHUD ──────────────────────────────────────────────────────────────
void GameplayScene::DrawHUD() {
  bool showHUD = (m_activePlayer->hp > 0);

  if (showHUD) {
    float pct = m_activePlayer->hp / m_activePlayer->maxHp;
    float ghost = m_playerGhostHp / m_activePlayer->maxHp;
    float hudX = 20.0f, hudY = 20.0f;
    float barW = 650.0f, barH = 45.0f;

    // Fondo de la barra
    DrawRectangle((int)hudX, (int)hudY, (int)barW, (int)barH,
                  {30, 30, 30, 255});
    // Ghost HP (Lerp suave en blanco)
    DrawRectangle((int)hudX, (int)hudY, (int)(barW * ghost), (int)barH,
                  {250, 250, 250, 100});

    // Vida con relleno sólido y textura (Clipped)
    if (ResourceManager::texVida.id != 0) {
      // Relleno sólido central
      DrawRectangle((int)hudX, (int)hudY, (int)(barW * pct), (int)barH,
                    {0, 200, 50, 255});

      Rectangle src = {0, 0, (float)ResourceManager::texVida.width * pct,
                       (float)ResourceManager::texVida.height};
      Rectangle dst = {hudX, hudY, barW * pct, barH};
      // Dibujamos la textura encima (quizás tenga brillos o bordes)
      DrawTexturePro(ResourceManager::texVida, src, dst, {0, 0}, 0.0f, WHITE);
    } else {
      DrawRectangle((int)hudX, (int)hudY, (int)(barW * pct), (int)barH,
                    {0, 228, 48, 255});
    }
    DrawRectangleLinesEx({hudX, hudY, barW, barH}, 2.0f, BLACK);

    // Energía
    float ePct = m_activePlayer->energy / m_activePlayer->maxEnergy;
    float enerY = hudY + barH + 8.0f;
    float enerW = 550.0f;
    float enerH = 22.0f;

    DrawRectangle((int)hudX, (int)enerY, (int)enerW, (int)enerH,
                  {30, 30, 30, 255});
    if (ResourceManager::texEnergia.id != 0) {
      // Relleno sólido amarillo-naranjoso
      DrawRectangle((int)hudX, (int)enerY, (int)(enerW * ePct), (int)enerH,
                    {255, 180, 0, 255});

      Rectangle src = {0, 0, (float)ResourceManager::texEnergia.width * ePct,
                       (float)ResourceManager::texEnergia.height};
      Rectangle dst = {hudX, enerY, enerW * ePct, enerH};
      DrawTexturePro(ResourceManager::texEnergia, src, dst, {0, 0}, 0.0f,
                     WHITE);
    } else {
      DrawRectangle((int)hudX, (int)enerY, (int)(enerW * ePct), (int)enerH,
                    {255, 180, 0, 255});
    }
    DrawRectangleLinesEx({hudX, enerY, enerW, enerH}, 2.0f, BLACK);

    DrawText(m_activePlayer->GetName().c_str(), (int)hudX, (int)(enerY + 20),
             18, m_activePlayer->GetHUDColor());
    DrawText(TextFormat("%d / %d", (int)m_activePlayer->hp,
                        (int)m_activePlayer->maxHp),
             (int)(hudX + barW - 100), (int)(hudY + 8), 16, WHITE);

    // Boss bar - solo wave 1 (Golem) y wave 3 (Ether Corrupto), NO en wave 2 (horda)
    if (m_boss && !m_boss->isDead && m_currentWave != 2) {
      float bw = 400.0f, bh = 22.0f;
      float bx = GetScreenWidth() - bw - 25.0f, by = 25.0f;
      float bPct = m_boss->hp / m_boss->maxHp;
      float bGhost = m_bossGhostHp / m_boss->maxHp;
      Color bCol = (m_boss->hp <= m_boss->maxHp * 0.5f) ? RED : MAROON;

      DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, {40, 40, 40, 255});
      DrawRectangle((int)bx, (int)by, (int)(bw * bGhost), (int)bh,
                    Fade(WHITE, 0.4f));
      DrawRectangle((int)bx, (int)by, (int)(bw * bPct), (int)bh, bCol);
      DrawRectangleLines((int)bx, (int)by, (int)bw, (int)bh, {20, 20, 20, 255});

      const char *bName;
      if (m_currentWave == 3) {
        bName = (m_boss->hp <= m_boss->maxHp * 0.5f) ? "ETHER CORRUPTO - FASE II" : "ETHER CORRUPTO";
      } else {
        bName = (m_boss->hp <= m_boss->maxHp * 0.5f) ? "BOSS ENFURECIDO" : "GUARDIAN DE LA ARENA";
      }
      Color bNameColor = (m_currentWave == 3) ? Color{180, 100, 255, 255} : GOLD;
      DrawText(bName, (int)(bx + bw) - MeasureText(bName, 18),
               (int)(by + bh + 5), 18, bNameColor);

      // --- STATUS ICONS (DoTs / Debuffs) ---
      float iconY = by + bh + 25.0f;
      float iconX = bx + bw - 22.0f;

      // 1. Sangrado (Bleed)
      if (m_boss->isBleeding) {
        DrawRectangleRounded({iconX, iconY, 22, 22}, 0.3f, 4, {180, 20, 20, 255});
        DrawRectangleRoundedLinesEx({iconX, iconY, 22, 22}, 0.3f, 4, 2.0f, MAROON);
        DrawText("S", (int)(iconX + 6), (int)(iconY + 5), 14, WHITE); // S de Sangrado
        DrawText(TextFormat("%.1fs", m_boss->bleedTimer), (int)(iconX - 2), (int)(iconY + 24), 10, Fade(WHITE, 0.8f));
        iconX -= 30.0f;
      }

      // 2. Estática (Static Stacks)
      if (m_boss->staticStacks > 0) {
        DrawRectangleRounded({iconX, iconY, 22, 22}, 0.3f, 4, {220, 180, 20, 255});
        DrawRectangleRoundedLinesEx({iconX, iconY, 22, 22}, 0.3f, 4, 2.0f, ORANGE);
        DrawText(TextFormat("%d", m_boss->staticStacks), (int)(iconX + (m_boss->staticStacks > 9 ? 3 : 7)), (int)(iconY + 5), 14, BLACK);
        DrawText(TextFormat("%.1fs", m_boss->staticTimer), (int)(iconX - 2), (int)(iconY + 24), 10, Fade(WHITE, 0.8f));
        iconX -= 30.0f;
      }
    }

    // Habilidades
    auto abs = m_activePlayer->GetAbilities();
    float sh = (float)GetScreenHeight();
    float sc = 1.8f;
    float abW = 64.0f * sc, abH = 64.0f * sc;
    for (size_t i = 0; i < abs.size(); i++) {
      float ax = 20.0f + i * (abW + 12.0f);
      float ay = sh - abH - 20.0f;
      Color abBg =
          abs[i].ready ? Fade({60, 0, 80, 255}, 0.9f) : Fade(BLACK, 0.7f);
      DrawRectangle((int)ax, (int)ay, (int)abW, (int)abH, abBg);

      // --- ÍCONO DE HABILIDAD ---
      if (abs[i].icon.id != 0) {
        float iconPadding = 8.0f;
        Rectangle src = {0, 0, (float)abs[i].icon.width,
                         (float)abs[i].icon.height};
        Rectangle dst = {ax + iconPadding, ay + iconPadding,
                         abW - iconPadding * 2, abH - iconPadding * 2};
        DrawTexturePro(abs[i].icon, src, dst, {0, 0}, 0.0f,
                       abs[i].ready ? WHITE : Fade(WHITE, 0.4f));
      }

      DrawRectangleLines((int)ax, (int)ay, (int)abW, (int)abH,
                         abs[i].ready ? abs[i].color : DARKGRAY);
      DrawText(abs[i].label.c_str(), (int)ax + 4, (int)ay + 6, 14, WHITE);
      if (abs[i].cooldown > 0) {
        float cdPct = fminf(abs[i].cooldown / abs[i].maxCooldown, 1.0f);
        DrawRectangle((int)ax, (int)(ay + abH - 8), (int)(abW * (1.0f - cdPct)),
                      8, abs[i].color);
        DrawText(TextFormat("%.1f", abs[i].cooldown), (int)(ax + abW / 2) - 15,
                 (int)(ay + abH / 2) - 10, 20, WHITE);
      } else {
        DrawRectangle((int)ax, (int)(ay + abH - 8), (int)abW, 8, abs[i].color);
      }
    }

    // Status especiales
    std::string status = m_activePlayer->GetSpecialStatus();
    if (!status.empty()) {
      DrawText(status.c_str(),
               GetScreenWidth() / 2 - MeasureText(status.c_str(), 28) / 2, 30,
               28, {255, 120, 0, 255});
    }
    if (m_activePlayer->IsBuffed()) {
      DrawText(TextFormat("BUFF %.1fs", m_activePlayer->GetBuffTimer()),
               GetScreenWidth() / 2 - 60, 62, 20, {180, 0, 255, 255});
    }
  }

  // ── Game Over ──────────────────────────────────────────────────────────
  if (m_activePlayer->hp <= 0.0f) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(BLACK, 0.85f));

    // Título con sombra desplazada
    const char *deadMsg = "HAS CAIDO EN COMBATE";
    int deadFs = 60;
    int dMsgX = GetScreenWidth() / 2 - MeasureText(deadMsg, deadFs) / 2;
    DrawText(deadMsg, dMsgX + 4, 184, deadFs, Fade(BLACK, 0.7f));
    DrawText(deadMsg, dMsgX, 180, deadFs, RED);

    // Subtítulo
    const char *subDead = "Tu alma ha sido reclamada por el abismo";
    DrawText(subDead, GetScreenWidth() / 2 - MeasureText(subDead, 20) / 2, 260,
             20, GRAY);

    Rectangle btnRevive = {(float)GetScreenWidth() / 2 - 160, 400, 320, 60};
    Rectangle btnMain = {(float)GetScreenWidth() / 2 - 160, 480, 320, 60};

    auto DrawDeadBtn = [&](Rectangle btn, const char *label, Color col) {
      bool hov = CheckCollisionPointRec(GetMousePosition(), btn);
      DrawRectangleRec(btn, hov ? ColorBrightness(col, 0.2f) : col);
      DrawRectangleLinesEx(btn, 2, hov ? WHITE : Fade(WHITE, 0.3f));
      int fs = 22;
      DrawText(label,
               (int)((float)btn.x + btn.width / 2.0f -
                     (float)MeasureText(label, fs) / 2.0f),
               (int)((float)btn.y + btn.height / 2.0f - (float)fs / 2.0f), fs,
               WHITE);
      return hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    };

    if (DrawDeadBtn(btnRevive, "INTENTAR DE NUEVO", {50, 10, 10, 255})) {
      m_activePlayer->Reset({2000, 2000});
      m_playerGhostHp = m_activePlayer->hp;
      Init(); // Restaura los jefes desde cero, curando y limpiando la arena
      if (showCursorInGame)
        ShowCursor();
      else
        HideCursor();
    }

    if (DrawDeadBtn(btnMain, "VOLVER AL MENU", {20, 20, 20, 255})) {
      SceneManager::Get().ChangeScene(std::make_unique<MainMenuScene>());
    }
  }



  // Hint de pausa
  DrawText("K / ESC: Pausa", 10, GetScreenHeight() - 24, 15, Fade(GRAY, 0.5f));
}

// ─── DrawQuad (static helper) ─────────────────────────────────────────────
void GameplayScene::DrawQuad(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4,
                             Color c) {
  DrawTriangle(p1, p2, p3, c);
  DrawTriangle(p1, p3, p2, c);
  DrawTriangle(p1, p3, p4, c);
  DrawTriangle(p1, p4, p3, c);
}

// ─── UpdateBossDeathReward (Paso B) ──────────────────────────────────────────
// Detecta la primera vez que el boss muere, otorga XP + 4 puntos de atributo
// y dispara el menú modal sin ningún temporizador intermedio.
void GameplayScene::UpdateBossDeathReward() {
  // Solo actúa una vez: cuando todos los enemigos de la wave acaban de morir
  if (m_aliveBosses.empty() || m_bossXpAwarded)
    return;

  bool allDead = true;
  for (auto *b : m_aliveBosses)
    if (b && !b->isDead) {
      allDead = false;
      break;
    }

  if (!allDead)
    return;

  m_bossXpAwarded = true;
  m_nextBossTimer = 5.0f; // Tiempo de espera para el siguiente boss

  // ── Conceder XP ──────────────────────────────────────────────────────────
  const float XP_REWARD = 120.0f;
  bool leveledUp = m_activePlayer->rpg.GainXP(XP_REWARD);

  // ── Siempre otorgamos 4 puntos distribuibles (independiente de level-up) ─
  m_activePlayer->rpg.puntosDisponibles += 4;

  if (leveledUp) {
    // Curar un 50% de vida al subir de nivel (aumentado segun solicitud)
    m_activePlayer->hp =
        fminf(m_activePlayer->hp + m_activePlayer->maxHp * 0.50f,
              m_activePlayer->maxHp);
  }

  // ── Disparar el menú modal inmediatamente ────────────────────────────────
  m_showLevelUpMenu = true;
  ShowCursor();
}

// ─── DrawLevelUpMenu (Paso C) ────────────────────────────────────────────────
// Menú modal bloqueante que permite distribuir puntosDisponibles entre 5 stats.
// Solo desaparece al presionar "CONFIRMAR" (con todos los puntos gastados)
// o "GUARDAR PARA DESPUÉS".
void GameplayScene::DrawLevelUpMenu() {
  const int SW = GetScreenWidth();
  const int SH = GetScreenHeight();

  // ── Fondo semitransparente con blur visual ───────────────────────────────
  DrawRectangle(0, 0, SW, SH, Fade(BLACK, 0.78f));
  DrawCircleGradient(SW / 2, SH / 2, 520, Fade({60, 0, 120, 255}, 0.25f),
                     {0, 0, 0, 0});

  // ── Panel central ────────────────────────────────────────────────────────
  const float PW = 560.0f, PH = 520.0f;
  const float PX = (SW - PW) * 0.5f, PY = (SH - PH) * 0.5f;

  DrawRectangleRounded({PX, PY, PW, PH}, 0.06f, 8, {18, 12, 30, 245});
  DrawRectangleRoundedLinesEx({PX, PY, PW, PH}, 0.06f, 8, 2.5f,
                              {160, 80, 255, 200});

  // ── Título ───────────────────────────────────────────────────────────────
  RPGStats &rpg = m_activePlayer->rpg;

  const char *title = TextFormat("NIVEL %d  —  DISTRIBUYE PUNTOS", rpg.nivel);
  int titleFs = 26;
  DrawText(title, SW / 2 - MeasureText(title, titleFs) / 2, (int)(PY + 22),
           titleFs, {220, 180, 255, 255});

  // Barra de XP
  const float barX = PX + 30, barY = PY + 60, barW = PW - 60, barH = 14;
  float xpPct = (rpg.xpRequerida > 0) ? rpg.xpActual / rpg.xpRequerida : 0.0f;
  DrawRectangleRounded({barX, barY, barW, barH}, 1.0f, 4, {40, 20, 60, 255});
  DrawRectangleRounded({barX, barY, barW * xpPct, barH}, 1.0f, 4,
                       {180, 80, 255, 255});
  DrawRectangleRoundedLinesEx({barX, barY, barW, barH}, 1.0f, 4, 1.5f,
                              {120, 60, 200, 180});
  DrawText(TextFormat("XP  %.0f / %.0f", rpg.xpActual, rpg.xpRequerida),
           (int)(barX + 4), (int)(barY + 18), 14, {180, 140, 220, 200});

  // Puntos disponibles
  const char *ptsLabel =
      TextFormat("Puntos disponibles:  %d", rpg.puntosDisponibles);
  int ptsFs = 20;
  Color ptsColor =
      (rpg.puntosDisponibles > 0) ? Color{255, 220, 60, 255} : GRAY;
  DrawText(ptsLabel, SW / 2 - MeasureText(ptsLabel, ptsFs) / 2, (int)(PY + 90),
           ptsFs, ptsColor);

  // ── Filas de estadísticas ────────────────────────────────────────────────
  struct StatRow {
    const char *name;
    int *stat;
    const char *desc;
    Color color;
  };

  StatRow rows[] = {
      {"HP", &rpg.statHP, "+20 vida max por punto", {80, 220, 120, 255}},
      {"ENERGÍA",
       &rpg.statEnergia,
       "+10 energía max por punto",
       {80, 180, 255, 255}},
      {"FUERZA", &rpg.statFuerza, "+5% daño por punto", {255, 100, 80, 255}},
      {"MENTE",
       &rpg.statMente,
       "+5% daño mágico por punto",
       {200, 120, 255, 255}},
      {"SUERTE",
       &rpg.statSuerte,
       TextFormat("+0.5%% Crit  (Actual: %.1f%%)", rpg.CritChance() * 100.0f),
       {255, 200, 60, 255}},
  };
  const int NUM_STATS = 5;

  float rowStartY = PY + 125.0f;
  float rowH = 54.0f;
  float btnSize = 34.0f;

  Vector2 mouse = GetMousePosition();

  for (int i = 0; i < NUM_STATS; i++) {
    float ry = rowStartY + i * rowH;
    bool rowHovered = (mouse.y >= ry && mouse.y < ry + rowH - 4 &&
                       mouse.x >= PX + 10 && mouse.x <= PX + PW - 10);

    // Fondo de fila con hover sutil
    if (rowHovered)
      DrawRectangleRounded({PX + 10, ry, PW - 20, rowH - 6}, 0.3f, 4,
                           {255, 255, 255, 18});

    // Nombre del stat
    DrawText(rows[i].name, (int)(PX + 26), (int)(ry + 8), 20, rows[i].color);

    // Valor actual (puntos gastados)
    const char *val = TextFormat("%d", *rows[i].stat);
    DrawText(val, (int)(PX + 180), (int)(ry + 8), 20, WHITE);

    // Descripción
    DrawText(rows[i].desc, (int)(PX + 220), (int)(ry + 12), 13,
             Fade(WHITE, 0.55f));

    // Botón  +
    Rectangle btnPlus = {PX + PW - btnSize - 20, ry + 8, btnSize, btnSize};
    bool canAdd = (rpg.puntosDisponibles > 0);
    bool hoverPlus = CheckCollisionPointRec(mouse, btnPlus);
    Color btnCol = canAdd ? (hoverPlus ? Color{140, 80, 255, 255}
                                       : Color{80, 40, 160, 255})
                          : Color{50, 50, 50, 200};
    DrawRectangleRounded(btnPlus, 0.4f, 4, btnCol);
    DrawRectangleRoundedLinesEx(btnPlus, 0.4f, 4, 1.5f,
                                canAdd ? rows[i].color : DARKGRAY);
    int plusFs = 22;
    DrawText(
        "+",
        (int)(btnPlus.x + btnPlus.width / 2 - MeasureText("+", plusFs) / 2),
        (int)(btnPlus.y + btnPlus.height / 2 - plusFs / 2), plusFs, WHITE);

    if (hoverPlus && canAdd && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      rpg.SpendPoint(*rows[i].stat);
      // Aplicar bonus de HP / Energía en tiempo real
      m_activePlayer->maxHp = 200.0f + rpg.MaxHpBonus();
      m_activePlayer->maxEnergy = 100.0f + rpg.MaxEnergyBonus();
    }

    // Botón  -  (solo si el stat tiene al menos 1 punto)
    Rectangle btnMinus = {PX + PW - btnSize * 2 - 28, ry + 8, btnSize, btnSize};
    bool canSub = (*rows[i].stat > 0);
    bool hoverMinus = CheckCollisionPointRec(mouse, btnMinus);
    Color btnMinCol = canSub ? (hoverMinus ? Color{200, 60, 60, 255}
                                           : Color{120, 30, 30, 255})
                             : Color{50, 50, 50, 200};
    DrawRectangleRounded(btnMinus, 0.4f, 4, btnMinCol);
    DrawRectangleRoundedLinesEx(btnMinus, 0.4f, 4, 1.5f,
                                canSub ? Color{255, 80, 80, 255} : DARKGRAY);
    int minFs = 22;
    DrawText(
        "-",
        (int)(btnMinus.x + btnMinus.width / 2 - MeasureText("-", minFs) / 2),
        (int)(btnMinus.y + btnMinus.height / 2 - minFs / 2), minFs, WHITE);

    if (hoverMinus && canSub && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      (*rows[i].stat)--;
      rpg.puntosDisponibles++;
      // Revertir bonus de HP / Energía
      m_activePlayer->maxHp = 200.0f + rpg.MaxHpBonus();
      m_activePlayer->maxEnergy = 100.0f + rpg.MaxEnergyBonus();
    }
  }

  // ── Botones de acción ────────────────────────────────────────────────────
  float btnY = PY + PH - 68.0f;
  float btnW = 200.0f, btnHgt = 46.0f;

  // CONFIRMAR (solo activo si se han gastado todos los puntos)
  bool allSpent = (rpg.puntosDisponibles == 0);
  Rectangle btnConfirm = {PX + PW / 2 - btnW - 10, btnY, btnW, btnHgt};
  bool hovConfirm = CheckCollisionPointRec(mouse, btnConfirm);
  Color confCol = allSpent ? (hovConfirm ? Color{100, 220, 100, 255}
                                         : Color{40, 140, 60, 255})
                           : Color{40, 60, 40, 160};
  DrawRectangleRounded(btnConfirm, 0.3f, 6, confCol);
  DrawRectangleRoundedLinesEx(btnConfirm, 0.3f, 6, 2.0f,
                              allSpent ? WHITE : DARKGRAY);
  int cfFs = 18;
  const char *confLabel =
      allSpent ? "CONFIRMAR"
               : TextFormat("Faltan %d pts", rpg.puntosDisponibles);
  DrawText(confLabel,
           (int)(btnConfirm.x + btnW / 2 - MeasureText(confLabel, cfFs) / 2),
           (int)(btnConfirm.y + btnHgt / 2 - cfFs / 2), cfFs, WHITE);

  if (hovConfirm && allSpent && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    m_showLevelUpMenu = false;
    if (!showCursorInGame)
      HideCursor();
  }

  // GUARDAR PARA DESPUÉS
  Rectangle btnLater = {PX + PW / 2 + 10, btnY, btnW, btnHgt};
  bool hovLater = CheckCollisionPointRec(mouse, btnLater);
  DrawRectangleRounded(btnLater, 0.3f, 6,
                       hovLater ? Color{80, 80, 80, 255}
                                : Color{40, 40, 40, 220});
  DrawRectangleRoundedLinesEx(btnLater, 0.3f, 6, 2.0f, Fade(WHITE, 0.4f));
  const char *laterLabel = "GUARDAR PARA DESPUÉS";
  int ltFs = 15;
  DrawText(laterLabel,
           (int)(btnLater.x + btnW / 2 - MeasureText(laterLabel, ltFs) / 2),
           (int)(btnLater.y + btnHgt / 2 - ltFs / 2), ltFs, LIGHTGRAY);

  if (hovLater && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    m_showLevelUpMenu = false;
    if (!showCursorInGame)
      HideCursor();
  }

  // Aviso si quedan puntos al guardar
  if (rpg.puntosDisponibles > 0) {
    const char *hint = "Puedes abrir este menú desde la pausa";
    DrawText(hint, SW / 2 - MeasureText(hint, 13) / 2, (int)(PY + PH + 8), 13,
             Fade(LIGHTGRAY, 0.6f));
  }
}

// ─── DrawVictoryScreen ────────────────────────────────────────────────────
void GameplayScene::DrawVictoryScreen() {
  const int SW = GetScreenWidth();
  const int SH = GetScreenHeight();

  // Fondo oscuro semitransparente que va apareciendo con el tiempo
  float fadeIn  = fminf(m_victoryTimer / 1.2f, 1.0f);
  DrawRectangle(0, 0, SW, SH, Fade(BLACK, 0.82f * fadeIn));

  // Gradiente interior dorado/violeta (como halo de victoria)
  DrawCircleGradient(SW / 2, SH / 2, 500, Fade({180, 80, 255, 255}, 0.18f * fadeIn), {0,0,0,0});

  // Texto VICTORIA  (aparece slide-down)
  float slide   = fmaxf(0.0f, 1.0f - expf(-m_victoryTimer * 3.5f));
  float textY   = -120.0f + slide * (SH * 0.28f + 120.0f);

  const char *mainTitle = "VICTORIA";
  int titleFs  = 90;
  int titleW   = MeasureText(mainTitle, titleFs);
  // Sombra
  DrawText(mainTitle, SW / 2 - titleW / 2 + 5, (int)textY + 5, titleFs, Fade(BLACK, 0.7f * fadeIn));
  // Texto dorado
  DrawText(mainTitle, SW / 2 - titleW / 2, (int)textY, titleFs, Fade({255, 210, 60, 255}, fadeIn));

  // Sub-title
  const char *subTitle = "DEMO TÉCNICA COMPLETADA";
  int subFs = 24;
  DrawText(subTitle, SW / 2 - MeasureText(subTitle, subFs) / 2,
           (int)(textY + titleFs + 10), subFs, Fade({220, 180, 255, 255}, fadeIn));

  // Línea decorativa
  float lineAlpha = fminf(m_victoryTimer / 1.8f, 1.0f);
  DrawRectangle(SW / 2 - 220, (int)(SH * 0.5f) - 1, 440, 2, Fade(GOLD, lineAlpha));

  // Estadística: Tiempo total
  if (m_victoryTimer > 1.5f) {
    float statFade = fminf((m_victoryTimer - 1.5f) / 0.8f, 1.0f);
    int   totalSecs = (int)m_totalGameTime;
    int   minutes   = totalSecs / 60;
    int   seconds   = totalSecs % 60;
    const char *timeLabel = TextFormat("TIEMPO  %02d:%02d", minutes, seconds);
    int   timeFs = 28;
    DrawText(timeLabel, SW / 2 - MeasureText(timeLabel, timeFs) / 2,
             (int)(SH * 0.52f), timeFs, Fade(WHITE, statFade));
  }

  // Instrucciones (aparecen después de 2 segundos)
  if (m_victoryTimer > 2.0f) {
    float pulse = 0.6f + 0.4f * sinf(m_victoryTimer * 3.0f);
    float instrFade = fminf((m_victoryTimer - 2.0f) / 0.6f, 1.0f) * pulse;
    const char *restart = "[R] Reiniciar   /   [ESC] Menú Principal";
    DrawText(restart, SW / 2 - MeasureText(restart, 18) / 2,
             SH - 60, 18, Fade(LIGHTGRAY, instrFade));
  }
}

// ─── DrawWaveAnnouncement ─────────────────────────────────────────────────
void GameplayScene::DrawWaveAnnouncement() {
  if (m_waveTextTimer <= 0.0f || m_waveText.empty()) return;

  const int SW = GetScreenWidth();
  const int SH = GetScreenHeight();

  // Calcular fade: aparece 0.4s, se mantiene, desaparece al final
  float totalDur = 3.5f; // duración aproximada máxima
  float t  = m_waveTextTimer;                // tiempo restante
  float fadeIn  = fminf(t / 0.4f, 1.0f);    // primeros 0.4s aparece
  // Slide horizontal: entra desde la izquierda
  float slideProgress = 1.0f - expf(-((totalDur - t) / totalDur) * 8.0f);
  slideProgress = fminf(slideProgress, 1.0f);

  float bannerH = 110.0f;
  float bannerY = SH * 0.42f;

  // Fondo del banner
  DrawRectangle(0, (int)bannerY, SW, (int)bannerH, Fade({10, 5, 20, 255}, 0.85f * fadeIn));
  // Líneas decorativas
  DrawRectangle(0, (int)bannerY,          SW, 2, Fade(m_waveTextIsStart ? GOLD : GREEN, fadeIn));
  DrawRectangle(0, (int)(bannerY + bannerH - 2), SW, 2, Fade(m_waveTextIsStart ? GOLD : GREEN, fadeIn));

  // Texto principal
  int mainFs  = m_waveTextIsStart ? 52 : 40;
  Color mainCol = m_waveTextIsStart ? Color{255, 210, 60, 255} : Color{80, 230, 100, 255};
  const char *mainTxt = m_waveText.c_str();
  float textX = -MeasureText(mainTxt, mainFs) + slideProgress * (SW / 2 + MeasureText(mainTxt, mainFs) / 2);
  DrawText(mainTxt, (int)textX, (int)(bannerY + 12), mainFs, Fade(mainCol, fadeIn));

  // Subtítulo
  if (!m_waveSubText.empty()) {
    const char *sub = m_waveSubText.c_str();
    int subFs = 22;
    DrawText(sub, SW / 2 - MeasureText(sub, subFs) / 2,
             (int)(bannerY + mainFs + 20), subFs, Fade(WHITE, fadeIn * 0.8f));
  }
}

} // namespace Scenes
