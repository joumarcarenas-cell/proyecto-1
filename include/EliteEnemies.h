#pragma once
#include "Boss.h"
#include "Player.h"
#include "CommonTypes.h"
#include <vector>

// =====================================================================
// SimpleKnight: Caballero basico con combo de 3 hits y Berserker
// =====================================================================
class SimpleKnight : public Boss {
public:
    enum class AIState { IDLE, CHASE, ATTACK_COMBO, BERSERKER_TRANSITION };
    AIState aiState = AIState::IDLE;
    float stateTimer = 0.0f;
    int comboStep = 0;
    float attackCooldown = 0.0f;
    bool isBerserker = false;

    SimpleKnight(Vector2 pos);
    void UpdateAI(Player& player) override;
    void Update() override;
    void Draw() override;
    void ScaleDifficulty(int wave) override;

private:
    bool CheckAttack(Player& player, float range, float damage);
};

// =====================================================================
// GreatswordElite: Unidad Elite con rango extra, Torbellino y Embestida
// =====================================================================
class GreatswordElite : public Boss {
public:
    enum class AIState { IDLE, CHASE, TORBELLINO, EMBESTIDA, ATTACK_COMBO, BERSERKER_TRANSITION };
    AIState aiState = AIState::IDLE;
    float stateTimer = 0.0f;
    int comboStep = 0; // Added for 3-hit combo
    float attackCooldown = 1.0f;
    bool isBerserker = false;
    float whirlwindTimer = 0.0f;
    float whirlwindHitCooldown = 0.0f;

    GreatswordElite(Vector2 pos);
    void UpdateAI(Player& player) override;
    void Update() override;
    void Draw() override;
    void ScaleDifficulty(int wave) override;

private:
    float attackRangeMultiplier = 1.5f;
    bool CheckAttack(Player& player, float range, float damage);
};

// =====================================================================
// SimplyArcher: IA de distancia con flecha cargada
// =====================================================================
class SimplyArcher : public Boss {
public:
    enum class AIState { IDLE, KEEP_DISTANCE, SHOOT_NORMAL, SHOOT_CHARGED };
    AIState aiState = AIState::IDLE;
    float stateTimer = 0.0f;
    float shootCooldown = 1.5f;

    struct Arrow {
        Vector2 pos;
        Vector2 dir;
        float speed;
        float damage;
        bool active;
        bool isCharged;
        bool hasDealtDamage = false;
    };
    std::vector<Arrow> arrows;

    SimplyArcher(Vector2 pos);
    void UpdateAI(Player& player) override;
    void Update() override;
    void Draw() override;
    void ScaleDifficulty(int wave) override;

private:
    float safeDistance = 350.0f;
    Player* m_cachedPlayer = nullptr; // Se actualiza en UpdateAI antes de Update
};
