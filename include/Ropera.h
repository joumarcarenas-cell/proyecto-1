#pragma once
#include "Player.h"
#include "CommonTypes.h"

class Ropera : public Player {
public:
    RoperaState state = RoperaState::NORMAL;

    Ropera(Vector2 pos) {
        position = pos;
        radius = 20.0f;
        maxHp = 200.0f;
        hp = maxHp;
        color = {0, 220, 180, 255};
    }

    void Update() override;
    void Draw() override;
    void Reset(Vector2 pos) override;
    void HandleSkills(Boss &boss) override;
    void CheckCollisions(Boss &boss) override;

    std::vector<AbilityInfo> GetAbilities() const override;
    std::string GetName() const override { return "[ROPERA (REDISEÑO)]"; }
    Color GetHUDColor() const override { return {0, 220, 180, 255}; }
    void CancelAttack() override { state = RoperaState::NORMAL; }
    bool IsImmune() const override { return false; }
};
