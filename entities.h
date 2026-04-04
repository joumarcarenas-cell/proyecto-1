#ifndef ENTITIES_H
#define ENTITIES_H

#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>
#include <cmath>
#include <cmath>

enum class GamePhase { RUNNING, PAUSED, SETTINGS, REBINDING, GAME_OVER };
enum class PlayerState { NORMAL, ATTACKING, SPINNING, DASHING, DASH_ATTACK };

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
    
    // --- NUEVO: RASTRO (TRAIL) ---
    Vector2 trail[8];
    int trailCount = 0;
};

class Entity {
public:
    Vector2 position;
    Vector2 velocity = {0, 0};
    float hp, maxHp, radius;
    Color color;

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

class Player; // Forward declaration

class Enemy : public Entity {
public:
    Vector2 spawnPos;
    float respawnTimer = 0.0f;
    bool isDead = false;

    // --- IA Y ESTADOS ---
    enum class AIState { IDLE, PURSUE, ATTACK_BASIC, ATTACK_DASH, ATTACK_SLAM };
    AIState aiState = AIState::IDLE;
    float stateTimer = 0.0f;
    int attackStep = 0;
    float attackCooldown = 1.0f;
    float dashTimer = 0.0f; // CD de Embestida
    float slamTimer = 12.0f; // CD del Terremoto
    float currentDashDist = 400.0f; 
    Vector2 facing = {1, 0};
    
    // Variables de animacion de ataque
    float attackPhaseTimer = 0.0f;
    bool hasHit = false;

    Enemy(Vector2 pos) {
        spawnPos = pos;
        position = pos;
        radius = 40.0f;
        maxHp = 500.0f;
        hp = maxHp;
        color = MAROON;
    }

    void UpdateAI(Player& player);
    bool CheckAttackCollision(Player& player, float range, float angle, float damage);
    void Update() override;
    void Draw() override;
};

class Player : public Entity {
public:
    PlayerState state = PlayerState::NORMAL;
    int comboStep = 0;
    bool hasHit = false;
    Vector2 facing = {1, 0};
    AttackPhase attackPhase = AttackPhase::NONE;
    float attackPhaseTimer = 0.0f;
    
    float dashCooldown = 0.0f;
    Vector2 targetAim = {0, 0};
    float hitCooldownTimer = 0.0f; // CD entre hits dentro de la ventana activa
    int   attackId = 0;            // Cambia cada vez que empieza una nueva ventana ATTACK_ACTIVE

    // --- NUEVOS ATRIBUTOS ---
    float energy = 0.0f;
    float maxEnergy = 100.0f;
    std::vector<Projectile> activeBoomerangs;
    
    float buffTimer = 0.0f;
    bool isBuffed = false;

    // --- VARIABLES PARA EL HUD ---
    float vidaActual = 100.0f;
    float vidaMaxima = 100.0f;
    float estaminaActual = 0.0f;
    float estaminaMaxima = 100.0f;
    bool berserkerActivo = false;
    bool boomerangDisponible = true;

    // --- HABILIDAD DEFINITIVA ---
    bool isUltActive = false;
    float ultTimer = 0.0f;
    int ultCharges = 0;
    bool isUltPending = false;
    float boomerangCooldown = 0.0f;
    float ultimateCooldown = 0.0f;
    
    // --- ATAQUE GIRATORIO (SPIN) ---
    float chargeTimer = 0.0f;
    int spinHitCount = 0;
    float spinAngle = 0.0f;
    float spinTimer = 0.0f;
    
    // --- SUPER ESTOCADA (DASH ATTACK) ---
    bool hasDashHit = false;
    Vector2 dashStartPos = {0, 0};
    float dashAttackTimer = 0.0f;
    bool canDashAttack = true; 
    bool lastDashWasAttack = false;

    ControlScheme controls;

    AttackFrame combo[4] = {
        //  range   angle   dmg     startup active  recovery  hitCD
        {120.0f, 40.0f,  10.0f, 0.10f, 0.12f, 0.12f, 0.04f}, // Golpe 1: Estocada (arco fino, 1 hit interior)
        {110.0f, 140.0f, 15.0f, 0.10f, 0.14f, 0.14f, 0.05f}, // Golpe 2: Tajo Normal (puede conectar 2 hits en borde)
        {110.0f, 240.0f, 15.0f, 0.10f, 0.14f, 0.14f, 0.06f}, // Golpe 3: Tajo Giratorio (arco amplio)
        {140.0f, 80.0f,  35.0f, 0.16f, 0.15f, 0.26f, 0.08f}  // Golpe 4: Estocada Gruesa (pesado, 1-2 hits)
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

// Barril de madera: primer objeto de escenario destruible
struct Barrel : public Destructible {
    Color colorBody   = { 139, 90, 43, 255 };  // Madera
    Color colorRing   = {  80, 50, 20, 255 };   // Aro oscuro
    Color colorShadow = {   0,  0,  0,  80 };   // Sombra

    Barrel() = default;
    Barrel(Vector2 pos, float respawnSecs = 20.0f) {
        position    = pos;
        radius      = 22.0f;
        maxHp       = 30.0f;
        hp          = maxHp;
        respawnTime = respawnSecs;
    }

    void Draw() const {
        if (isDead) return;

        // Sombra elíptica (isométrico)
        DrawEllipse((int)position.x, (int)position.y + 4,
                    (int)(radius * 0.9f), (int)(radius * 0.45f), colorShadow);

        float h  = radius * 1.6f;
        float cx = position.x;
        float cy = position.y - h * 0.5f;

        // Cuerpo principal
        DrawRectangle((int)(cx - radius), (int)(cy - h * 0.5f),
                      (int)(radius * 2), (int)h, colorBody);

        // Tapa superior (elipse)
        DrawEllipse((int)cx, (int)(cy - h * 0.5f),
                    (int)radius, (int)(radius * 0.45f),
                    ColorBrightness(colorBody, 0.2f));

        // Dos aros metálicos
        float r1y = cy - h * 0.5f + h * 0.25f;
        float r2y = cy - h * 0.5f + h * 0.70f;
        DrawRectangle((int)(cx - radius), (int)r1y, (int)(radius * 2), 4, colorRing);
        DrawRectangle((int)(cx - radius), (int)r2y, (int)(radius * 2), 4, colorRing);

        // Barra de vida encima
        float pct = hp / maxHp;
        float bw  = radius * 2.2f;
        DrawRectangle((int)(cx - bw * 0.5f), (int)(cy - h * 0.5f - 14),
                      (int)bw, 5, Fade(BLACK, 0.5f));
        DrawRectangle((int)(cx - bw * 0.5f), (int)(cy - h * 0.5f - 14),
                      (int)(bw * pct), 5, ORANGE);
    }
};

#endif