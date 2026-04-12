#pragma once
#include <raylib.h>
#include <raymath.h>
#include <string>
#include <vector>

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

// --- ROPERA STATES ---
enum class RoperaState {
    NORMAL,
    ATTACKING,      // Combo de 3 fases
    CHARGING_HEAVY, // Hold click: cargando
    HEAVY_ATTACK,   // Super estocada frontal
    DASHING,        // i-frames breves post-blink
    CASTING_Q,      // Dos tajos rapidos Q
    ULT_ACTIVE      // Modo Garras
};

enum class CharacterType { REAPER, ROPERA };

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
    float hitCooldown;
};

// =====================================================
// --- GROUND BURST (Habilidad Q del Reaper) ---
// =====================================================
struct GroundBurst {
    Vector2 position;
    float   radius;
    float   lifetime;
    float   maxLifetime;
    bool    active;
    bool    hasDealtDamage;
    bool    isTip;
    float   damage;
    float   visualRadius;

    void Update(float dt) {
        if (!active) return;
        lifetime -= dt;
        if (lifetime <= 0) { active = false; return; }
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
    bool isHoming       = false; 
    bool isShadow       = false; 
    float homingStrength = 3.5f; 
    float speed         = 1000.0f;
    
    // --- RASTRO (TRAIL) ---
    Vector2 trail[8];
    int trailCount = 0;
};

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
    Texture2D icon;
};

struct Destructible {
    Vector2 position;
    float   hp;
    float   maxHp;
    float   radius;
    bool    isDead        = false;
    float   respawnTimer  = 0.0f;
    float   respawnTime   = 20.0f;

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

// Global variables extern declarations
extern bool isTimeStopped;
extern float hitstopTimer;
extern float screenShake;
