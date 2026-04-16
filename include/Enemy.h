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
  // ── Parametros de dificultad ─────────────────────────────────────
  float reactionSpeed = 0.5f;
  float baseAttackCooldown = 1.5f;
  float aggressionLevel = 1.15f; // Aumentado ligeramente para mas agresividad

  // ── Estados de IA ─────────────────────────────────────────────────
  enum class AIState {
    IDLE,
    CHASE,
    ORBITING,
    STAGGERED,
    EVADE,
    ATTACK_BASIC,
    ATTACK_DASH,
    ATTACK_SLAM,
    ATTACK_HEAVY,
    ATTACK_ROCKS,
    ATTACK_JUMP,
    AVALANCHE_START, // Moviendose a la esquina
    AVALANCHE_ACTIVE // Golpeando el suelo e invulnerable
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
  
  // ── Avalancha (Evento unico al 25% HP) ───────────────────────────
  bool avalancheTriggered = false;
  float avalancheTimer = 0.0f;
  float waveSpawnTimer = 0.0f;
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

  bool IsImmune() const { return false; }

  bool IsInvulnerable() const override {
    return (aiState == AIState::AVALANCHE_ACTIVE);
  }

  // ── Constructor ───────────────────────────────────────────────────
  Enemy(Vector2 pos) {
    spawnPos = pos;
    position = pos;
    radius = 40.0f;
    maxHp = 1950.0f; // 1500 + 30%
    hp = maxHp;
    color = {139, 0, 0, 255};
    frameCols = 1;
    frameRows = 1;
    previousAIState = AIState::IDLE;
  }

  void UpdateAI(Player &player) override;
  void Update() override;
  void Draw() override;
  void ScaleDifficulty(int wave) override;
};
