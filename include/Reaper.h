#pragma once
#include "Player.h"
#include "CommonTypes.h"
#include <vector>

struct BloodPool {
    Vector2 position;
    float lifetime;
};

struct BloodLeaf {
    Vector2 position;
    Vector2 velocity;
    float damage;
    bool active;
};

struct ScytheProjectile {
    Vector2 position;
    Vector2 startPos;
    Vector2 targetPos;
    float progress;
    bool isReturning;
    bool active;
    float angle;
    float damage;
};

// Echo = fantasma que queda donde se hizo un tajo durante el DashAttack
struct SlashEcho {
    Vector2 position;
    Vector2 dir;
    float lifetime;   // en segundos
    float maxLifetime;
    Color color;
};

// --- Q / R DashAttack state ---
struct DashAttackState {
    bool active        = false;
    bool isUltimate    = false;     // true = R (más poderosa)
    Vector2 start      = {0,0};
    Vector2 end        = {0,0};
    float timer        = 0.f;      // tiempo transcurrido
    float duration     = 0.f;      // duración total del dash
    float slashTimer   = 0.f;     // cada cuánto genera un tajo
    bool  hitEnemy     = false;    // para el sistema de hit único de la R
};

class Reaper : public Player {
public:
    ReaperState state = ReaperState::NORMAL;

    // Heavy charge
    float heavyHoldTimer  = 0.0f;
    bool  isChargingHeavy = false;

    // DashAttack (Q & R comparten el mismo sistema)
    DashAttackState dashAtk;

    // E Skill
    float eStunTimer      = 0.0f;
    bool  eExplosionReady = false;

    // Cooldowns
    float qCooldown   = 0;
    float eCooldown   = 0;
    float ultCooldown = 0;

    std::vector<BloodPool>         pools;
    std::vector<BloodLeaf>         leaves;
    std::vector<ScytheProjectile>  scythes;
    std::vector<SlashEcho>         echoes;

    Reaper(Vector2 pos) {
        position   = pos;
        radius     = 20.0f;
        maxHp      = 200.0f;
        hp         = maxHp;
        maxEnergy  = 100.0f;
        energy     = maxEnergy;
        color      = {180, 0, 255, 255};
    }

    void Update() override;
    void Draw()   override;
    void Reset(Vector2 pos) override;
    void HandleSkills(Boss& boss) override;
    void CheckCollisions(Boss& boss) override;
    std::vector<AbilityInfo> GetAbilities() const override;

    std::string GetName()     const override { return "Reaper"; }
    Color       GetHUDColor() const override { return {180, 0, 255, 255}; }
    bool        IsImmune()    const override { return dashAtk.active || state == ReaperState::DASHING; }

    void TakeDamage(float amount, Vector2 pushVel) override {
        if (dashAtk.active && !dashAtk.isUltimate) amount *= 0.5f;
        Player::TakeDamage(amount, pushVel);
    }

    void CancelAttack() override {
        state         = ReaperState::NORMAL;
        attackPhase   = AttackPhase::NONE;
        isChargingHeavy = false;
        dashAtk.active  = false;
        eStunTimer      = 0;
    }

    void ApplyBleed(Boss& boss) const;

    // Helper: inicia un DashAttack hacia targetPos
    void StartDashAttack(Vector2 targetPos, bool isUlt);
};
