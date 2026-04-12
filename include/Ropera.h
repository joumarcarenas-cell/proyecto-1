#pragma once
#include "Player.h"
#include "CommonTypes.h"
#include "Enemy.h"

// =====================================================
// --- CLASE ROPERA ---
// =====================================================
class Ropera : public Player {
public:
    RoperaState state = RoperaState::NORMAL;

    // --- Combo de 3 golpes (+18% rango vs Reaper) ---
    //              range    angle   dmg   startup  active  recovery  hitCD
    AttackFrame combo[3] = {
        { 148.0f,  50.0f, 15.0f, 0.20f, 0.16f, 0.26f, 0.05f }, // Hit 1: Estocada frontal
        { 138.0f,  90.0f, 17.0f, 0.22f, 0.17f, 0.28f, 0.05f }, // Hit 2: Tajo lateral 90deg
        { 142.0f,  48.0f, 12.0f, 0.25f, 0.42f, 0.32f, 0.11f }, // Hit 3: Rafaga x3 (+rango)
    };

    // Hit 3: paso atras + rafaga con 3 sub-hits individuales
    bool  step3BackDone   = false;
    int   burstHitCount   = 0;     // cuantos sub-hits ejecutados (max 3)
    float burstSubTimer   = 0.0f;  // delay entre cada sub-hit (0.12s)
    bool  burstSubActive  = false; // rafaga en progreso

    // --- Ataque cargado (hold click) ---

    // --- Dash independiente (SPACE), con i-frames ---
    float dashGraceTimer  = 0.0f;  // duracion de i-frames y del dash fisico

    // --- Habilidad Q: Dos tajos angulo cerrado ---
    float qMaxCooldown       = 8.0f;
    int   qSlashIndex        = 0;      // 0=primer tajo, 1=segundo
    float qSlashActiveTimer  = 0.0f;   // ventana activa del tajo actual
    float qSlashGapTimer     = 0.0f;   // pausa entre el tajo 1 y 2
    bool  qActive            = false;
    bool  qHasHit            = false;

    // --- Habilidad E: Buff activo ---
    float eMaxCooldown       = 14.0f;
    float eBuffTimer         = 0.0f;
    bool  eBuffActive        = false;
    static constexpr float eLifestealFrac   = 0.20f;
    static constexpr float eMaxHpBonusFrac  = 0.03f;

    // --- Buff de velocidad (Q con hit) ---
    float moveSpeedBuffTimer = 0.0f;

    // --- Ultimate: Modo Garras (requiere hp < 60%) ---
    float ultMaxCooldown  = 30.0f;
    float ultTimer        = 0.0f;
    bool  ultActive       = false;

    // --- Espadas Voladoras: flotan detras, se disparan al golpear ---
    enum class SwordState { BEHIND, FIRING, RETURNING };
    struct FlyingSword {
        Vector2    position      = {0, 0};
        SwordState swordState    = SwordState::BEHIND;
        float      fireDelay     = 0.0f;   // delay escalonado (0, 0.1, 0.2s)
        float      fireDelayTimer = 0.0f;
        Vector2    targetPos     = {0, 0};
        bool       hasDealt      = false;
        float      flashTimer    = 0.0f;
        bool       active        = false;
    } swords[3];

    static constexpr float swordHitDamage   = 9.0f;
    static constexpr float swordFireSpeed   = 1600.0f;
    static constexpr float swordReturnSpeed = 1200.0f;
    static constexpr float swordBehindDist  = 48.0f;
    Vector2 swordTargetSnapshot = {0, 0};

    Ropera(Vector2 pos) {
        position = pos;
        radius   = 20.0f;
        maxHp    = 110.0f;
        hp       = maxHp;
        color    = { 0, 180, 160, 255 };
        for (int i = 0; i < 3; i++) {
            swords[i].swordState = SwordState::BEHIND;
            swords[i].fireDelay  = i * 0.10f;
            swords[i].active     = false;
        }
    }

    void Update() override;
    void Draw()   override;
    void Reset(Vector2 pos) override;
    void HandleSkills(Enemy& boss) override;
    void CheckCollisions(Enemy& enemy) override;

    bool IsImmune() const override { return state == RoperaState::DASHING; }
    std::vector<AbilityInfo> GetAbilities() const override;

    std::string GetName()     const override { return "[ROPERA]"; }
    Color       GetHUDColor() const override { return {0, 220, 180, 255}; }

    bool  IsBuffed() const override {
        return eBuffActive || moveSpeedBuffTimer > 0.0f || ultActive;
    }
    float GetBuffTimer() const override {
        if (ultActive)   return ultTimer;
        if (eBuffActive) return eBuffTimer;
        return moveSpeedBuffTimer;
    }
    std::string GetSpecialStatus() const override;

    bool CheckComboCollision(Enemy& enemy);
    bool CheckHeavyCollision(Enemy& enemy);
    bool CheckQCollision(Enemy& enemy, int slashIdx);
    void TriggerSwords(Vector2 enemyPos);
    void InitSwords();
    void UpdateSwords(float dt, Enemy& boss);
};
