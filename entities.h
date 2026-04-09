#ifndef ENTITIES_H
#define ENTITIES_H

#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>
#include <cmath>

enum class GamePhase { RUNNING, PAUSED, SETTINGS, REBINDING, GAME_OVER, CHAR_SELECT, VICTORY };

// --- REAPER STATES ---
enum class ReaperState {
    NORMAL,
    ATTACKING,          // Combo de 3 hits pesados
    CHARGING_HEAVY,     // Hold click: carga el heavy attack
    HEAVY_ATTACK,       // El tajo frontal despues del mini-dash
    DASHING,            // Blink (teletransporte)
    CASTING_E,          // Lanzar orbes teledirigidos
    LOCKED,             // Bloqueo duro durante cinematicas
    ULT_PHASE3          // Buff post-ult
};

// Variable Global para pausa de tiempo
extern bool isTimeStopped;
extern float hitstopTimer;
extern float screenShake;
enum class CharacterType { REAPER, ROPERA };

// --- ROPERA STATES ---
enum class RoperaState {
    NORMAL,
    ATTACKING,      // Combo de 3 fases
    CHARGING_HEAVY, // Hold click: cargando
    HEAVY_ATTACK,   // Super estocada frontal
    DASHING,        // i-frames breves post-blink (reutilizado visualmente)
    CASTING_Q,      // Dos tajos rapidos Q
    ULT_ACTIVE      // Modo Garras
};

struct ControlScheme {
    int dash = KEY_SPACE;
    int boomerang = KEY_Q;
    int berserker = KEY_E;
    int ultimate = KEY_R;
};

// --- RECURSOS (ASSETS) CENTRALIZADOS ---
struct ResourceManager {
    static Texture2D texVida;
    static Texture2D texEnergia;
    static Texture2D texBerserker;
    static Texture2D texBoomerang;
    static Texture2D texUltimate;
    
    // --- SPRITES DE ENTIDADES ---
    static Texture2D texPlayer;
    static Texture2D texEnemy;

    static void Load();
    static void Unload();
};

enum class AttackPhase { NONE, STARTUP, ATTACK_ACTIVE, RECOVERY };

struct AttackFrame {
    float range;
    float angleWidth; 
    float damage;
    float startup;
    float active;
    float recovery;
    float hitCooldown; // Tiempo entre golpes dentro de la ventana activa (barrido del arma)
};

// =====================================================
// --- GROUND BURST (Habilidad Q del Reaper) ---
// =====================================================
struct GroundBurst {
    Vector2 position;       // Posicion en el suelo
    float   radius;         // Radio del circulo de daño
    float   lifetime;       // Cuanto tiempo permanece visible
    float   maxLifetime;
    bool    active;         // Si esta activo
    bool    hasDealtDamage; // Para hit-once
    bool    isTip;          // El 5to estallido (punta de flecha, mas grande)
    float   damage;
    // Visual: expande desde 0 al radio maximo
    float   visualRadius;   // Radio actual para la animacion de aparicion

    void Update(float dt) {
        if (!active) return;
        lifetime -= dt;
        if (lifetime <= 0) { active = false; return; }
        // Animacion de expansion: crece en los primeros 0.08s
        float expandT = fminf(1.0f - (lifetime / maxLifetime) * (maxLifetime / 0.08f), 1.0f);
        visualRadius = radius * fminf(expandT + 0.1f, 1.0f);
    }

    void Draw() const {
        if (!active) return;
        float alpha = (lifetime / maxLifetime);
        Color c = isTip ? Color{255, 80, 255, 255} : Color{180, 0, 220, 255};
        DrawCircleV(position, visualRadius, Fade(c, alpha * 0.5f));
        DrawCircleLines((int)position.x, (int)position.y, visualRadius, Fade(WHITE, alpha * 0.8f));
        if (isTip) {
            DrawCircleLines((int)position.x, (int)position.y, visualRadius * 1.15f, Fade(c, alpha * 0.4f));
        }
    }
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
    
    // --- REAPER EXTENSIONS ---
    bool isHoming       = false;  // Orbe teledirigido (habilidad E)
    bool isShadow       = false;  // Proyectil visual de la ult (no hace daño)
    float homingStrength = 3.5f;  // Velocidad de giro en rad/s
    float speed         = 1000.0f;
    
    // --- RASTRO (TRAIL) ---
    Vector2 trail[8];
    int trailCount = 0;
};

class Entity {
public:
    Vector2 position;
    Vector2 velocity = {0, 0};
    float hp, maxHp, radius;
    Color color;
    
    // --- CC (Crowd Control) ---
    float stunTimer = 0.0f;
    float slowTimer = 0.0f;
    float hitFlashTimer = 0.0f; // Step 2: Destello de impacto

    // --- SOPORTE PARA SPRITESHEETS ---
    int frameCols = 1;
    int frameRows = 1;
    int currentFrameX = 0;
    int currentFrameY = 0;
    float frameTimer = 0.0f;
    float frameSpeed = 0.15f; // segundos por frame

    virtual void Update() = 0;
    virtual void Draw() = 0;
    
    void DrawHealthBar(float width, float height) {
        float healthPct = hp / maxHp;
        DrawRectangle((int)(position.x - width/2), (int)(position.y - radius - 50), (int)width, (int)height, RED);
        DrawRectangle((int)(position.x - width/2), (int)(position.y - radius - 50), (int)(width * healthPct), (int)height, GREEN);
    }
};

// --- TEXTO FLOTANTE DE DAÑO ---
struct DamageText {
    Vector2 position;
    Vector2 velocity;
    float life;
    float maxLife;
    int damage;
    Color color;

    void Update(float dt) {
        position = Vector2Add(position, Vector2Scale(velocity, dt));
        life -= dt;
    }
    void Draw() {
        if (life <= 0) return;
        float alpha = life / maxLife;
        Color c = color;
        c.a = (unsigned char)(255.0f * alpha);
        // Desplazamiento sutil hacia arriba
        DrawText(TextFormat("%i", damage), (int)position.x, (int)position.y, 22, c);
    }
};

struct AbilityInfo {
    std::string label;
    float cooldown;
    float maxCooldown;
    float energyCost;
    bool ready;
    Color color;
};

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
    float dashCooldown    = 0.0f;
    float qCooldown       = 0.0f;
    float eCooldown       = 0.0f;
    float ultCooldown     = 0.0f;

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

class Enemy : public Entity {
public:
    Vector2 spawnPos;
    float respawnTimer = 0.0f;
    bool isDead = false;

    // --- PARAMETROS DE DIFICULTAD ---
    float reactionSpeed = 0.5f;
    float baseAttackCooldown = 1.5f;
    float aggressionLevel = 1.0f;

    // --- IA Y ESTADOS ---
    enum class AIState { 
        IDLE, CHASE, ORBITING, STAGGERED, EVADE,
        ATTACK_BASIC, ATTACK_DASH, ATTACK_SLAM, ATTACK_HEAVY, ATTACK_ROCKS,
        ATTACK_JUMP 
    };
    AIState aiState = AIState::IDLE;
    AIState previousAIState = AIState::IDLE;
    float stateTimer = 0.0f;
    int attackStep = 0;
    float attackCooldown = 1.0f;
    float dashTimer = 0.0f; 
    float slamTimer = 12.0f; 
    float jumpTimer = 15.0f; // CD del JUMP
    float currentDashDist = 400.0f; 
    int dashCharges = 0; // Para el combo de doble dash
    bool mixupDecided = false; // Indica si ya escogio su mix-up
    Vector2 facing = {1, 0};
    
    // --- Stagger System ---
    float recentDamage = 0.0f;
    float recentDamageTimer = 0.0f;
    
    // --- Orbiting e IA Reactiva ---
    float orbitAngle = 0.0f;
    int orbitDir = 1;
    float evadeCooldown = 3.0f;
    Vector2 evadeDir = {0, 0};
    
    // Proyectiles de Rocas
    struct RockDrop {
        Vector2 position;
        float fallTimer;
        bool active;
    } rocks[5];
    int rocksSpawned = 0;
    float rockSpawnTimer = 0.0f;
    int rocksToSpawn = 0; // Number of rocks left to spawn simultaneously

    // Variables de animacion de ataque
    float attackPhaseTimer = 0.0f;
    bool hasHit = false;

    // =====================================================
    // --- SISTEMA DE SANGRADO (DoT) - REAPER ---
    // =====================================================
    float bleedTimer      = 0.0f;   // Duracion total del sangrado (10s)
    float bleedTickTimer  = 0.0f;   // Temporizador entre ticks de daño
    float bleedTotalDamage = 0.0f;  // Calculado como maxHp * 0.05f
    bool  isBleeding      = false;
    
    // Aplica/refresca el sangrado (llamar al impactar con habilidades Reaper)
    void ApplyBleed() {
        isBleeding       = true;
        bleedTimer       = 10.0f;
        bleedTickTimer   = 0.5f;
        bleedTotalDamage = (maxHp * 0.05f) / 10.0f; // Exactamente maxHp * 0.05 divido en 10 ticks
    }
    
    // Devuelve el daño remanente del sangrado (para DoT Pop de la Ult)
    float GetRemainingBleedDamage() const {
        if (!isBleeding || bleedTimer <= 0.0f) return 0.0f;
        // ticks restantes * daño por tick
        float tickDmg = bleedTotalDamage; // por tick (cada 0.5s)
        float ticksLeft = bleedTimer / 0.5f;
        return tickDmg * ticksLeft;
    }

    Enemy(Vector2 pos) {
        spawnPos = pos;
        position = pos;
        radius = 40.0f;
        maxHp = 500.0f;
        hp = maxHp;
        color = MAROON;
        frameCols = 12;
        frameRows = 6;
        previousAIState = AIState::IDLE;
    }

    void UpdateAI(Player& player);
    bool CheckAttackCollision(Player& player, float range, float angle, float damage);
    void Update() override;
    void Draw() override;
};



// =====================================================
// --- CLASE REAPER (Nuevo Personaje) ---
// =====================================================
class Reaper : public Player {
public:
    ReaperState state  = ReaperState::NORMAL;

    // --- Combo pesado (3 hits) ---

    // Datos de los 3 golpes del combo
    //              range    angle   dmg   startup  active  recovery  hitCD
    AttackFrame combo[3] = {
        { 130.0f, 180.0f, 16.2f, 0.28f, 0.18f, 0.30f, 0.06f }, // Golpe 1: Media luna (-10%)
        { 125.0f, 180.0f, 19.8f, 0.30f, 0.20f, 0.32f, 0.07f }, // Golpe 2: Media luna (-10%)
        { 145.0f, 360.0f, 34.2f, 0.35f, 0.22f, 0.40f, 0.08f }, // Golpe 3: Circulo completo (-10%)
    };

    // --- Ataque Cargado (Hold Click) ---
    float miniDashTimer  = 0.0f;   // Duracion del mini-dash post-release (0.15s)

    // --- Blink (Dash) ---
    float blinkDistance = 170.0f;

    // --- Habilidad Q: Ground Bursts secuenciales ---
    float qMaxCooldown  = 10.0f; // Aumentado a 10s
    float qBurstTimer   = 0.0f;  // Timer entre cada estallido (0.1s)
    int   qBurstsSpawned = 0;    // Cuantos de los 5 ya aparecieron
    Vector2 qBurstOrigin  = {0,0}; // Desde donde empieza la cadena
    Vector2 qBurstDir     = {0,0}; // Direccion de la cadena
    bool  qActive        = false;  // Cadena en progreso
    GroundBurst groundBursts[5]; // Los 5 estallidos simultaneos en escena
    std::vector<Projectile> activeProjectiles;

    // --- Habilidad E: Orbes Teledirigidos ---

    // --- Ultimate: Secuencia Cinematica ---
    float buffTimer       = 0.0f;  // Fase 3 (6s de buff)
    bool  isBuffed        = false;
    // Estado interno de la secuencia
    float ultSeqTimer     = 0.0f;  // Timer general de la secuencia
    int   ultSeqPhase     = 0;     // 0=inactivo 1=sombras 2=tajo_final 3=buff

    // Sombras de la Ult (2 entidades que cruzan en X)
    struct UltShadow {
        Vector2 position;
        Vector2 velocity;
        float   lifetime;
        bool    active;
    } ultShadows[2];
    bool  ultFinalSlash     = false;  // Se activo el tajo final
    bool  ultFinalSlashHit  = false;  // El tajo final ya golpeo
    ReaperState prevReaperState = ReaperState::NORMAL;

    Reaper(Vector2 pos) {
        position  = pos;
        radius    = 20.0f;
        maxHp     = 100.0f;
        hp        = maxHp;
        color     = { 160, 0, 220, 255 }; // Purpura oscuro
    }

    void Update() override;
    void Draw()   override;
    void Reset(Vector2 pos) override;
    void HandleSkills(Enemy& boss) override;
    void CheckCollisions(Enemy& enemy) override;
    bool IsImmune() const override { return state == ReaperState::DASHING; }
    std::vector<AbilityInfo> GetAbilities() const override;

    std::string GetName() const override { return "[SEGADOR]"; }
    Color GetHUDColor() const override { return {255, 0, 255, 255}; }

    bool IsTimeStoppedActive() const override { return state == ReaperState::LOCKED; }
    std::string GetSpecialStatus() const override;
    bool IsBuffed() const override { return isBuffed; }
    float GetBuffTimer() const override { return buffTimer; }

    bool CheckComboCollision(Enemy& enemy);
    bool CheckHeavyCollision(Enemy& enemy);
    bool CheckUltFinalSlash(Enemy& enemy);
    void StartGroundBurstChain();  // Inicia la cadena Q
    void LaunchHomingOrbs(Enemy& boss);
    void ActivateUltimate(Vector2 bossPos);
};

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
    float dashMaxCD       = 1.8f;
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

// =====================================================
// --- OBJETOS DESTRUIBLES (BASE PARA ESCENARIO) ---
// =====================================================

// Base genérica reutilizable para cualquier objeto de escenario destruible
struct Destructible {
    Vector2 position;
    float   hp;
    float   maxHp;
    float   radius;
    bool    isDead        = false;
    float   respawnTimer  = 0.0f;
    float   respawnTime   = 20.0f;

    // Recibe daño; devuelve true si acaba de morir
    bool TakeDamage(float dmg) {
        if (isDead) return false;
        hp -= dmg;
        if (hp <= 0.0f) {
            hp           = 0.0f;
            isDead       = true;
            respawnTimer = respawnTime;
            return true;
        }
        return false;
    }

    void Update(float dt) {
        if (isDead) {
            respawnTimer -= dt;
            if (respawnTimer <= 0.0f) {
                isDead = false;
                hp     = maxHp;
            }
        }
    }
};



#endif