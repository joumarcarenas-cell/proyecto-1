#pragma once
#include <raylib.h>
#include <raymath.h>
#include <string>
#include <vector>

enum class GamePhase {
  RUNNING,
  PAUSED,
  SETTINGS,
  REBINDING,
  GAME_OVER,
  CHAR_SELECT,
  VICTORY
};

// --- REAPER STATES ---
enum class ReaperState {
  NORMAL,
  ATTACKING,      // Combo de 3 hits pesados
  CHARGING_HEAVY, // Hold click: carga el heavy attack
  HEAVY_ATTACK,   // El tajo frontal despues del mini-dash
  DASHING,        // Blink (teletransporte)
  CASTING_E,      // Lanzar orbes teledirigidos
  LOCKED,         // Bloqueo duro durante cinematicas
  ULT_PHASE3      // Buff post-ult
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

// --- MAGE STATES ---
enum class MageState {
  NORMAL,
  ATTACKING,      
  CHARGING_HEAVY, 
  HEAVY_ATTACK,   
  DASHING,        
  CASTING_E,      
  CHARGING_E,
  CASTING_R,
  LOCKED
};

// --- ELEMENTAL SYSTEM ---
enum class ElementMode { NONE, WATER_ICE, LIGHTNING };

enum class CharacterType { REAPER, ROPERA, MAGE };

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
  Vector2 direction;
  float radius;
  float lifetime;
  float maxLifetime;
  bool active;
  bool hasDealtDamage;
  bool isTip;
  float damage;
  float visualRadius;

  void Update(float dt) {
    if (!active)
      return;
    lifetime -= dt;
    if (lifetime <= 0) {
      active = false;
      return;
    }
    // Instant appearance at full radius (like Mage)
    visualRadius = radius;
  }

  void Draw() const {
    if (!active)
      return;
    float alpha = (lifetime / maxLifetime);
    Color c = isTip ? Color{255, 80, 255, 255} : Color{180, 0, 220, 255};
    Color cBg = Fade(c, alpha * 0.25f);
    Color cLines = Fade(WHITE, alpha * 0.8f);

    // 1. Detección visual (Círculo de impacto sutil)
    DrawCircleLinesV(position, visualRadius, cLines);
    DrawCircleV(position, visualRadius, cBg);

    // 2. [NEW] HOJAS REALES (Blades/Leaves emerging from ground)
    // Dibujamos 3-4 cuchillas afiladas en angulos distintos
    float bladeProg = 1.0f - (lifetime / maxLifetime);
    if (bladeProg > 1.0f) bladeProg = 1.0f;
    
    // Solo dibujamos las hojas en la fase inicial y media
    if (alpha > 0.1f) {
        for (int i = 0; i < 4; i++) {
            float angle = (i * 90.0f + 25.0f) * DEG2RAD;
            float h = (visualRadius * 1.5f) * sinf(bladeProg * PI); // Sube y baja
            
            Vector2 base = position;
            Vector2 tip = { position.x + cosf(angle) * h * 0.8f, position.y + sinf(angle) * h * 0.4f - h * 0.6f };
            Vector2 side = { position.x + cosf(angle + 0.3f) * h * 0.3f, position.y + sinf(angle + 0.3f) * h * 0.15f };

            // Dibujo de la cuchilla afilada (Triangulo con gradiente manual)
            DrawTriangle(base, side, tip, Fade(c, alpha));
            DrawTriangle(base, tip, Vector2{side.x - 5, side.y - 5}, Fade(WHITE, alpha * 0.5f)); // Filo brillante
        }
    }

    // 3. Destello si es impacto inicial
    if (alpha > 0.8f) {
      float flashT = (alpha - 0.8f) / 0.2f;
      DrawCircleV(position, visualRadius * 0.7f, Fade(WHITE, flashT * 0.6f));
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
  bool isHoming = false;
  bool isShadow = false;
  float homingStrength = 3.5f;
  float speed = 1000.0f;

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
  bool isCritical = false;

  void Update(float dt) {
    position = Vector2Add(position, Vector2Scale(velocity, dt));
    life -= dt;
  }
  void Draw() {
    if (life <= 0)
      return;
    float alpha = life / maxLife;
    Color c = isCritical ? GOLD : color;
    c.a = (unsigned char)(255.0f * alpha);
    Color oc = BLACK;
    oc.a = c.a;
    int fs = isCritical ? 36 : 26; // Increased font size for crits

    const char* txt = TextFormat("%i", damage);
    int px = (int)position.x;
    int py = (int)position.y;
    
    // Outline
    DrawText(txt, px - 2, py, fs, oc);
    DrawText(txt, px + 2, py, fs, oc);
    DrawText(txt, px, py - 2, fs, oc);
    DrawText(txt, px, py + 2, fs, oc);
    DrawText(txt, px - 1, py - 1, fs, oc);
    DrawText(txt, px + 1, py + 1, fs, oc);
    
    // Text
    DrawText(txt, px, py, fs, c);
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
  float hp;
  float maxHp;
  float radius;
  bool isDead = false;
  float respawnTimer = 0.0f;
  float respawnTime = 20.0f;

  bool TakeDamage(float dmg) {
    if (isDead)
      return false;
    hp -= dmg;
    if (hp <= 0.0f) {
      hp = 0.0f;
      isDead = true;
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
        hp = maxHp;
      }
    }
  }
};

// Global variables extern declarations
extern bool isTimeStopped;
extern float g_timeScale;
extern double g_gameTime;
extern float hitstopTimer;
extern float screenShake;
extern bool g_showHitboxes;

// --- ARENA HELPERS ---
namespace Arena {
constexpr float CENTER_X = 2000.0f;
constexpr float CENTER_Y = 2000.0f;
constexpr float RADIUS   = 2100.0f; // Expanded for 30x30 tiles (30 * 70 = 2100)

inline bool IsInside(Vector2 pos, float radius) {
  float dx = std::abs(pos.x - CENTER_X);
  float dy = std::abs(pos.y - CENTER_Y);
  // Perspectiva isométrica: Y escala x2
  return (dx + 2.0f * dy) <= (RADIUS - radius * 2.236f);
}

inline Vector2 GetClampedPos(Vector2 pos, float radius) {
  float dx = pos.x - CENTER_X;
  float dy = pos.y - CENTER_Y;
  float R = RADIUS - radius * 2.236f;
  float current = std::abs(dx) + 2.0f * std::abs(dy);
  if (current > R) {
    float scale = R / current;
    return {CENTER_X + dx * scale, CENTER_Y + dy * scale};
  }
  return pos;
}
} // namespace Arena
