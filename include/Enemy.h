#pragma once
// =====================================================================
// Enemy.h  -  Clase Enemy (Golem Boss)
// =====================================================================
// Declaracion de la clase Enemy y su maquina de estados de IA.
// La implementacion vive en Enemy.cpp.
//
// Dependencias:
//   - raylib.h / raymath.h  (tipos Vector2, Color, etc.)
//   - Entity (base class via entities.h)
//   - Player (forward declaration para UpdateAI / CheckAttackCollision)
// =====================================================================

#include "raylib.h"
#include "raymath.h"
#include <string>
#include <vector>

#include "Boss.h"
#include "CombatUtils.h"

// Forward declarations para evitar inclusion circular
class Player;

class Enemy : public Boss {
public:
  float previousHp = 0.0f;
  // ── Parametros de dificultad ─────────────────────────────────────
  float reactionSpeed = 0.5f;
  float baseAttackCooldown = 1.5f;
  float aggressionLevel = 1.45f; // Aumentado significativamente para mas agresividad

  // ── Estados de IA ─────────────────────────────────────────────────
  enum class AIState {
    IDLE,
    CHASE,
    STAGGERED,
    EVADE,
    ATTACK_BASIC,
    ATTACK_DASH,
    ATTACK_SLAM,
    ATTACK_HEAVY,
    ATTACK_ROCKS,
    ATTACK_JUMP,
    AVALANCHE_START,
    AVALANCHE_ACTIVE,
    CENTER_ROCKS_START,
    CENTER_ROCKS_LIFT,
    CENTER_ROCKS_THROW,
    DESPERATION_START,
    DESPERATION_ACTIVE
  };
  AIState aiState = AIState::IDLE;
  AIState previousAIState = AIState::IDLE;
  float stateTimer = 0.0f;
  int attackStep = 0;
  float attackCooldown = 1.0f;
  float dashTimer = 0.0f;
  float slamTimer = 12.0f;
  float jumpTimer = 15.0f;
  float currentDashDist = 400.0f;
  int dashCharges = 0;
  bool mixupDecided = false;
  
  // ── Fases y Eventos Centrales ───────────────────────────
  bool phase75Triggered = false;
  bool phase50Triggered = false;
  bool avalancheTriggered = false;
  bool desperationTriggered = false;
  float avalancheTimer = 0.0f;
  float waveSpawnTimer = 0.0f;
  
  float phaseTimer = 0.0f; // Multi-uso para las canalizaciones
  float rockThrowDelay = 0.0f;
  float subBurstTimer = 0.0f;
  int burstShotsLeft = 0;
  int desperationBombBurstsDone = 0;
  int desperationWavesFired = 0; // Rastreador de ondas disparadas en la fase final
  
  struct ThrownRock {
    Vector2 position;
    Vector2 direction;
    float speed;
    bool active;
  };
  ThrownRock thrownRocks[8];
  int thrownRockCount = 0;
  
  struct BombZone {
    Vector2 position;
    float delay;
    float radius;
    bool active;
  };
  BombZone desperationBombs[5];
  float desperationBombTimer = 0.0f;
  struct Wave {
    Vector2 center;
    float radius;
    float speed;
    bool active;
    bool hasHit;
  };
  Wave waves[10]; // Buffer para las ondas de choque
  int waveCount = 0;

  // ── Stagger y Combate (Compartido con Boss) ───────────────────────
  // Los jefes ahora son inmunes al stagger por petición del usuario.
  void OnStagger() override {}

  // ── Orbiting / Evasion ────────────────────────────────────────────
  float orbitAngle = 0.0f;
  int orbitDir = 1;
  float evadeCooldown = 3.0f;
  Vector2 evadeDir = {0, 0};

  // ── Rocas ─────────────────────────────────────────────────────────
  struct RockDrop {
    Vector2 position;
    float fallTimer;
    bool active;
  } rocks[5];
  int rocksSpawned = 0;
  float rockSpawnTimer = 0.0f;
  int rocksToSpawn = 0;

  // ── Fase de ataque ────────────────────────────────────────────────
  float attackPhaseTimer = 0.0f;

  // ── Sangrado (Heredado de Boss) ───────────────────────────────────

  // ── Helper de Z-depth (para RenderManager) ───────────────────────
  float GetZDepth() const { return position.y; }

  // ── Eje Z Falso: altura del salto para sombra dinámica ─────────────
  float GetFakeZ() const override {
    if (aiState == AIState::ATTACK_JUMP && attackPhaseTimer > 0)
      return sinf((1.0f - attackPhaseTimer) * PI) * 450.0f;
    return 0.0f;
  }

  bool IsInvulnerable() const override {
    return (aiState == AIState::AVALANCHE_ACTIVE || aiState == AIState::DESPERATION_START || aiState == AIState::DESPERATION_ACTIVE || desperationImmune);
  }

  // ── Constructor ───────────────────────────────────────────────────
  Enemy(Vector2 pos) {
    spawnPos = pos;
    position = pos;
    radius = 40.0f;
    maxHp = 3200.0f; // Aumentado significativamente para alargar las fases
    hp = maxHp;
    previousHp = hp;
    color = {139, 0, 0, 255};
    frameCols = 1;
    frameRows = 1;
    previousAIState = AIState::IDLE;
  }

  void UpdateAI(Player &player) override;
  void Update() override;
  void Draw() override;
  void ScaleDifficulty(int wave) override;
  void TakeDamage(float dmg, float poiseDmg, Vector2 pushVel) override;
};
