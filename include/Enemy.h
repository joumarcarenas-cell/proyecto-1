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

#include "Entity.h"

// Forward declarations para evitar inclusion circular
class Player;

class Enemy : public Entity {
public:
    // ── Spawn / muerte ───────────────────────────────────────────────
    Vector2 spawnPos;
    float   respawnTimer = 0.0f;
    bool    isDead       = false;

    // ── Parametros de dificultad ─────────────────────────────────────
    float reactionSpeed      = 0.5f;
    float baseAttackCooldown = 1.5f;
    float aggressionLevel    = 1.0f;

    // ── Estados de IA ─────────────────────────────────────────────────
    enum class AIState {
        IDLE, CHASE, ORBITING, STAGGERED, EVADE,
        ATTACK_BASIC, ATTACK_DASH, ATTACK_SLAM, ATTACK_HEAVY, ATTACK_ROCKS,
        ATTACK_JUMP
    };
    AIState aiState         = AIState::IDLE;
    AIState previousAIState = AIState::IDLE;
    float   stateTimer      = 0.0f;
    int     attackStep      = 0;
    float   attackCooldown  = 1.0f;
    float   dashTimer       = 0.0f;
    float   slamTimer       = 12.0f;
    float   jumpTimer       = 15.0f;
    float   currentDashDist = 400.0f;
    int     dashCharges     = 0;
    bool    mixupDecided    = false;
    Vector2 facing          = {1, 0};

    // ── Stagger ───────────────────────────────────────────────────────
    float recentDamage      = 0.0f;
    float recentDamageTimer = 0.0f;

    // ── Orbiting / Evasion ────────────────────────────────────────────
    float   orbitAngle   = 0.0f;
    int     orbitDir     = 1;
    float   evadeCooldown = 3.0f;
    Vector2 evadeDir     = {0, 0};

    // ── Rocas ─────────────────────────────────────────────────────────
    struct RockDrop {
        Vector2 position;
        float   fallTimer;
        bool    active;
    } rocks[5];
    int   rocksSpawned    = 0;
    float rockSpawnTimer  = 0.0f;
    int   rocksToSpawn    = 0;

    // ── Fase de ataque ────────────────────────────────────────────────
    float attackPhaseTimer = 0.0f;
    bool  hasHit           = false;

    // ── Sangrado (DoT aplicado por Reaper) ────────────────────────────
    float bleedTimer       = 0.0f;
    float bleedTickTimer   = 0.0f;
    float bleedTotalDamage = 0.0f;
    bool  isBleeding       = false;

    void ApplyBleed() {
        isBleeding       = true;
        bleedTimer       = 10.0f;
        bleedTickTimer   = 0.5f;
        bleedTotalDamage = (maxHp * 0.05f) / 10.0f;
    }

    float GetRemainingBleedDamage() const {
        if (!isBleeding || bleedTimer <= 0.0f) return 0.0f;
        return bleedTotalDamage * (bleedTimer / 0.5f);
    }

    // ── Helper de Z-depth (para RenderManager) ───────────────────────
    float GetZDepth() const { return position.y; }

    bool IsImmune() const { return false; }

    // ── Constructor ───────────────────────────────────────────────────
    Enemy(Vector2 pos) {
        spawnPos        = pos;
        position        = pos;
        radius          = 40.0f;
        maxHp           = 500.0f;
        hp              = maxHp;
        color           = {139, 0, 0, 255};
        frameCols       = 1;
        frameRows       = 1;
        previousAIState = AIState::IDLE;
    }

    // ── Metodos (implementados en Enemy.cpp) ──────────────────────────
    void    UpdateAI(Player& player);
    bool    CheckAttackCollision(Player& player, float range, float angle, float damage);
    void    Update();
    void    Draw();
};
