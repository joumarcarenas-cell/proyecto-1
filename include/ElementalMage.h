#pragma once
#include "Player.h"
#include "Boss.h"
#include "ResourceManager.h"
#include <vector>

// Proyectil generico del mago
struct MageProjectile {
    Vector2 position;
    Vector2 direction;
    float speed;
    float range;
    float traveled;
    float damage;
    bool active;
    bool piercing;
    ElementMode element;
    // Para Hit 1 / 2 que persiguen o apuntan
    bool isTargetingMouse;  
    Vector2 targetPos;
    bool isKunai;
    bool isCrescent; // For "Media Luna" heavy attacks

    // Hit tracking for piercing
    void* hitEntities[16];
    int hitCount = 0;
    bool alreadyHit(void* entity) {
        for (int i = 0; i < hitCount && i < 16; i++) {
            if (hitEntities[i] == entity) return true;
        }
        return false;
    }
    void addHit(void* entity) {
        if (hitCount < 16) {
            hitEntities[hitCount++] = entity;
        }
    }
};

// Torbellino del modo Agua
struct MageTornado {
    Vector2 position;
    float durationTimer;
    float pullRadius;
    float explodeRadius;
    float damageTick;
    float explodeDamage;
    bool active;
    bool exploded;
};

// Rayo instantaneo (Detonador de modo Rayo) 
struct LightningRay {
    Vector2 start;
    Vector2 end;
    float lifeTimer;
};

// Area visual de impacto instantáneo (Básicos de rayo)
struct VisualHitArea {
    Vector2 pos;
    float radius;
    float lifeTimer;
    float spawnDelay;
    Color color;
    float damage;
    bool damageDealt;
    bool isHeavy; 
};

class ElementalMage : public Player {
public:
    ElementalMage(Vector2 pos);
    
    void Update() override;
    void Draw() override;
    void Reset(Vector2 pos) override;
    
    std::string GetName() const override { return "Elemental Mage"; }
    Color GetHUDColor() const override { 
        return (currentMode == ElementMode::WATER_ICE) ? SKYBLUE : YELLOW; 
    }
    
    void HandleSkills(Boss &boss) override;
    void CheckCollisions(Boss &boss) override;
    
    bool IsImmune() const override { return state == MageState::DASHING || attackPhase == AttackPhase::RECOVERY; }
    
    std::vector<AbilityInfo> GetAbilities() const override;
    
    bool IsBuffed() const override { return isOverloaded; }
    float GetBuffTimer() const override { return overloadTimer; }
    std::string GetSpecialStatus() const override {
        return (currentMode == ElementMode::WATER_ICE) ? "MODE: WATER" : "MODE: LIGHTNING";
    }

    MageState state = MageState::NORMAL;
    ElementMode currentMode = ElementMode::WATER_ICE;

    // Control de cargas estaticas por ataque para forzar cambio de modo
    bool staticStackBasicAvailable = true;
    bool staticStackEAvailable = true;
    bool staticStackUltAvailable = true;

    // Buffers y Buffos
    float attackSpeedBuffTimer = 0.0f;
    float eCastTimer = 0.0f;
    float heavyCastTimer = 0.0f;
    Vector2 castDir = {0,0};
    
    // E (Water Kunai)
    bool eMarkActive = false;
    Boss* eMarkedEnemy = nullptr; 
    float eMarkTimer = 0.0f;
    bool canReactivateE = false;
    
    // E (Lightning Charge)
    float eHoldTimer = 0.0f;
    bool isSuperE = false;
    
    // E (Lightning Spears)
    float eChargesCooldowns[3] = {0, 0, 0};
    int GetAvailableECharges() const;
    
    // R (Lightning Ultimate)
    bool isOverloaded = false;
    float overloadTimer = 0.0f;
    float overloadAuraTimer = 0.0f;
    
    // Contenedores
    std::vector<MageProjectile> projectiles;
    std::vector<MageTornado> tornados;
    std::vector<LightningRay> lightningRays;
    std::vector<VisualHitArea> hitAreas;

protected:
    void HandleInput(float dt);
    void UpdateState(float dt);
    void UpdateEntities(float dt); // Proyectiles, Tornados, etc.
    void ChangeMode();
    void StartBasicAttack();
    void StartHeavyAttack();
};
