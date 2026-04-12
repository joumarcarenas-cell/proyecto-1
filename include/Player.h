#pragma once
#include "Entity.h"
#include "CommonTypes.h"
#include <string>
#include <vector>

class Enemy; // Forward declaration

class Player : public Entity {
public:
    Vector2 facing = {1, 0};
    Vector2 targetAim = {0, 0};
    ControlScheme controls;

    float energy = 100.0f;
    float maxEnergy = 100.0f;

    // --- Variables de Estado y Combate Comunes ---
    int   comboStep       = 0;
    float comboTimer      = 0.0f;
    float inputBufferTimer= 0.0f;
    bool  hasHit          = false;
    AttackPhase attackPhase      = AttackPhase::NONE;
    float attackPhaseTimer       = 0.0f;
    float hitCooldownTimer       = 0.0f;
    int   attackId               = 0;

    // --- Ataque Cargado ---
    float holdTimer       = 0.0f;
    bool  isCharging      = false;
    bool  heavyHasHit     = false;

    // --- Dash y Cooldowns ---
    int   dashCharges     = 2;
    int   maxDashCharges  = 2;
    float dashCooldown1   = 0.0f;
    float dashCooldown2   = 0.0f;
    float dashMaxCD       = 1.8f;
    float qCooldown       = 0.0f;
    float eCooldown       = 0.0f;
    float ultCooldown     = 0.0f;

    void UpdateDash(float dt) {
        if (dashCooldown1 > 0) {
            dashCooldown1 -= dt;
            if (dashCooldown1 <= 0) dashCharges++;
        }
        if (dashCooldown2 > 0) {
            dashCooldown2 -= dt;
            if (dashCooldown2 <= 0) dashCharges++;
        }
    }

    bool CanDash() const { return dashCharges > 0; }
    void UseDash() {
        if (dashCharges > 0) {
            dashCharges--;
            if (dashCooldown1 <= 0) dashCooldown1 = dashMaxCD;
            else if (dashCooldown2 <= 0) dashCooldown2 = dashMaxCD;
        }
    }

    virtual void Update() override = 0;
    virtual void Draw() override = 0;
    virtual void Reset(Vector2 pos) = 0;
    
    // Character info for HUD
    virtual std::string GetName() const = 0;
    virtual Color GetHUDColor() const = 0;
    
    // Skill handling that might need reference to enemies
    virtual void HandleSkills(Enemy& boss) = 0;
    virtual void CheckCollisions(Enemy& enemy) = 0;
    
    virtual bool IsImmune() const = 0;
    virtual std::vector<AbilityInfo> GetAbilities() const = 0;
    
    // UI specific flags
    virtual bool IsTimeStoppedActive() const { return false; }
    virtual std::string GetSpecialStatus() const { return ""; }
    virtual bool IsBuffed() const { return false; }
    virtual float GetBuffTimer() const { return 0.0f; }
};
