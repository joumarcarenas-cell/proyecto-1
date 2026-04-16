#pragma once

// =====================================================================
// GameplayScene.h - Escena Principal de Combate
// =====================================================================
// Contiene toda la lógica que actualmente vive en el switch-case
// GamePhase::RUNNING de main.cpp, encapsulada limpiamente.
// =====================================================================

#include "../../entities.h"
#include "../Boss.h"
#include "../Enemy.h"
#include "../HardBoss.h"
#include "../EtherCorrupto.h"
#include "../IsoMap.h"
#include "../RPGStats.h"
#include "../graphics/VFXSystem.h"
#include "../graphics/AnimeVFX.h"
#include "Scene.h"
#include <queue>
#include <raylib.h>
#include <string>
#include <vector>

namespace Scenes {

class GameplayScene : public Scene {
public:
  // Recibe referencia al jugador activo
  GameplayScene(Player *&activePlayer)
      : m_activePlayer(activePlayer), m_boss(nullptr) {}

  void Init() override;
  void Update(float dt) override;
  void Draw() override;
  void Unload() override;

  // Acceso público para que PauseScene pueda leer la cámara si necesita
  // dibujar el mundo de fondo desenfocado.
  const Camera2D &GetCamera() const { return m_camera; }

private:
  std::vector<Boss*> m_aliveBosses; 
  std::queue<Boss*> m_bossQueue;
  Player *&m_activePlayer;
  Boss *m_boss;
  
  float m_nextBossTimer = 0.0f;
  int m_currentWave = 1;

  // ── Victoria ─────────────────────────────────────────────────────
  bool m_isVictory = false;
  float m_victoryTimer = 0.0f;    // tiempo que lleva la pantalla visible
  float m_totalGameTime = 0.0f;   // cronómetro desde el inicio
  bool m_gameFrozen = false;      // congela inputs y physics al vencer

  Camera2D m_camera{};

  // ── Mapa isométrico de suelo ──────────────────────────────────────
  IsoMap::Map m_isoMap;     // Datos de la cuadrícula (TileID por celda)
  Vector2 m_isoMapOffset{}; // Desplazamiento calculado en Init()

  // Vida fantasma (HUD con lerp)
  float m_playerGhostHp = 0.0f;
  float m_bossGhostHp = 0.0f;

  // Textos de daño flotantes
  std::vector<DamageText> m_damageTexts;

  // ── Sistema Level-Up ─────────────────────────────────────────
  bool m_showLevelUpMenu = false; // Pausa el mundo mientras sea true
  bool m_bossXpAwarded = false;   // Evita conceder XP/puntos más de una vez
  int m_pendingPoints = 0;        // Puntos repartidos en el modal (copia local)

  // Índice del stat destacado en el menú (para hover)
  int m_menuHoveredStat = -1;

  // ── Verificación de Versión ────────────────────────────────────
  bool m_showVersion = false;
  float m_versionTimer = 0.0f;

  bool m_showDebugHitboxes = false;

  // ── AnimeVFX: Trail IDs por personaje ─────────────────────────
  // -1 = no inicializado. Se asignan en Init() y se reutilizan por session.
  int m_trailIdPlayer  = -1;  // Trail del jugador activo
  int m_trailIdReaper  = -1;  // Colores especificos por personaje
  int m_trailIdRopera  = -1;
  int m_trailIdMage    = -1;

  float m_prevDashCharges = 2.0f;  // Para detectar cuando se usa el dash

  // ── Anuncio de Oleada ────────────────────────────────────────────
  float m_waveTextTimer = 0.0f;
  std::string m_waveText = "";
  std::string m_waveSubText = "";
  bool m_waveTextIsStart = true;   // true=WAVE START, false=WAVE CLEAR

  // ─── Helpers ───────────────────────────────────────────────────
  void UpdateCamera(float dt);
  void UpdateBleedDoT(float dt);
  void UpdateRocks(float dt);
  void UpdateCollisions();
  void UpdateDeathCheck();
  void UpdateBossDeathReward(); 
  void UpdateBossRush(float dt); // Maneja la cola y el temporizador
  void ScaleDifficulty(Boss* b, int wave);

  void DrawWorld();       // 2D isométrico (BeginMode2D / EndMode2D)
  void DrawHUD();         // HUD superpuesto en coordenadas de pantalla
  void DrawArena();       // Suelo + paredes del diamante isométrico
  void DrawLevelUpMenu(); // Paso C: menú modal bloqueante de stats
  void DrawVictoryScreen(); // Pantalla de victoria al completar todas las oleadas
  void DrawWaveAnnouncement(); // Anuncio animado de inicio / fin de oleada

  static void DrawQuad(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, Color c);
};

} // namespace Scenes
