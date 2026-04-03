#ifndef ENTITIES_H
#define ENTITIES_H

#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>

enum class GamePhase { RUNNING, PAUSED, SETTINGS, REBINDING };

struct ControlScheme {
    int dash = KEY_SPACE;
    int boomerang = KEY_Q;
    int berserker = KEY_E;
    int ultimate = KEY_R;
};

enum class AttackPhase { NONE, STARTUP, ATTACK_ACTIVE, RECOVERY };

struct AttackFrame {
    float range;
    float angleWidth; 
    float damage;
    float startup;
    float active;
    float recovery;
};

struct Projectile {
    Vector2 position;
    Vector2 startPos;
    Vector2 direction;
    float maxDistance;
    bool returning;
    bool active;
    float damage;
    bool isOrbital;
    float orbitAngle;
    bool isLastUltCharge;
};

class Entity {
public:
    Vector2 position;
    Vector2 velocity = {0, 0};
    float hp, maxHp, radius;
    Color color;
    
    // --- SOPORTE PARA SPRITES ---
    Texture2D texture;
    bool hasTexture = false;

    virtual void Update() = 0;
    virtual void Draw() = 0;
    
    void DrawHealthBar(float width, float height) {
        float healthPct = hp / maxHp;
        DrawRectangle((int)(position.x - width/2), (int)(position.y - radius - 50), (int)width, (int)height, RED);
        DrawRectangle((int)(position.x - width/2), (int)(position.y - radius - 50), (int)(width * healthPct), (int)height, GREEN);
    }
};

class Enemy : public Entity {
public:
    Vector2 spawnPos;
    float respawnTimer = 0.0f;
    bool isDead = false;

    Enemy(Vector2 pos) {
        spawnPos = pos;
        position = pos;
        radius = 40.0f;
        maxHp = 500.0f;
        hp = maxHp;
        color = MAROON;
    }

    void Update() override;
    void Draw() override;
};

class Player : public Entity {
public:
    int comboStep = 0;
    bool isAttacking = false;
    bool hasHit = false;
    Vector2 facing = {1, 0};
    AttackPhase attackPhase = AttackPhase::NONE;
    float attackPhaseTimer = 0.0f;
    
    float dashCooldown = 0.0f;
    Vector2 targetAim = {0, 0};

    // --- NUEVOS ATRIBUTOS ---
    float energy = 0.0f;
    float maxEnergy = 100.0f;
    std::vector<Projectile> activeBoomerangs;
    
    float buffTimer = 0.0f;
    bool isBuffed = false;

    // --- HABILIDAD DEFINITIVA ---
    bool isUltActive = false;
    float ultTimer = 0.0f;
    int ultCharges = 0;
    bool isUltPending = false;
    float boomerangCooldown = 0.0f;
    float ultimateCooldown = 0.0f;
    
    // --- ATAQUE GIRATORIO (SPIN) ---
    bool isSpinning = false;
    float chargeTimer = 0.0f;
    int spinHitCount = 0;
    float spinAngle = 0.0f;
    float spinTimer = 0.0f;
    
    // --- SUPER ESTOCADA (DASH ATTACK) ---
    bool isDashAttacking = false;
    bool hasDashHit = false;
    Vector2 dashStartPos = {0, 0};
    float dashAttackTimer = 0.0f;

    ControlScheme controls;

    AttackFrame combo[4] = {
        {120.0f, 40.0f,  10.0f, 0.11f, 0.09f, 0.13f}, // Golpe 1: Estocada (Fina y larga)
        {110.0f, 140.0f, 15.0f, 0.11f, 0.11f, 0.15f}, // Golpe 2: Tajo Normal (Arco estándar)
        {110.0f, 240.0f, 15.0f, 0.11f, 0.11f, 0.15f}, // Golpe 3: Tajo Giratorio (Arco muy amplio)
        {140.0f, 80.0f,  35.0f, 0.17f, 0.13f, 0.28f}  // Golpe 4: Estocada Gruesa (Final pesado)
    };
    float comboTimer = 0;

    Player(Vector2 pos) {
        position = pos;
        radius = 20.0f;
        maxHp = 100.0f;
        hp = maxHp;
        color = BLUE;
    }

    void Update() override;
    void Draw() override;
    bool CheckAttackCollision(Enemy& enemy);
    bool CheckDashCollision(Enemy& enemy);
    void LaunchBoomerang(bool isLast = false);
    void ActivateUltimate();
};

#endif