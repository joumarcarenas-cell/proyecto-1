#pragma once
#include "CommonTypes.h"
#include "Boss.h"
#include "Player.h"
#include "DirectionUtils.h"

// =====================================================
// --- CLASE REAPER (Nuevo Personaje) ---
// =====================================================
class Reaper : public Player {
public:
  ReaperState state = ReaperState::NORMAL;

  // --- Combo pesado (3 hits) ---

  // Datos de los 3 golpes del combo
  //              range    angle   dmg   startup  active  recovery  hitCD
  AttackFrame combo[3] = {
      {130.0f, 180.0f, 28.0f, 0.24f, 0.12f, 0.27f,
       0.06f}, // Golpe 1 (Damage +20%)
      {125.0f, 180.0f, 34.5f, 0.27f, 0.13f, 0.30f,
       0.07f}, // Golpe 2 (Damage +20%)
      {145.0f, 360.0f, 59.0f, 0.32f, 0.14f, 0.38f,
       0.08f}, // Golpe 3 (Damage +20%)
  };

  // --- Ataque Cargado (Hold Click) ---
  float miniDashTimer = 0.0f; // Duracion del mini-dash post-release (0.15s)

  // --- Blink (Dash) ---
  float blinkDistance = 88.0f; // Reduced further (was 98)
  float blinkGraceTimer = 0.0f; // Temporizador de recuperación post-blink

  // --- Habilidad Q: Ground Bursts secuenciales ---
  float qMaxCooldown = 10.0f;    // Aumentado a 10s
  float qBurstTimer = 0.0f;      // Timer entre cada estallido (0.1s)
  int qBurstsSpawned = 0;        // Cuantos de los 5 ya aparecieron
  Vector2 qBurstOrigin = {0, 0}; // Desde donde empieza la cadena
  Vector2 qBurstDir = {0, 0};    // Direccion de la cadena
  bool qActive = false;          // Cadena en progreso
  GroundBurst groundBursts[5];   // Los 5 estallidos simultaneos en escena
  std::vector<Projectile> activeProjectiles;

  // --- Habilidad E: Orbes Teledirigidos ---

  // --- Ultimate: Secuencia Cinematica ---
  float buffTimer = 0.0f; // Fase 3 (6s de buff)
  bool isBuffed = false;
  // Estado interno de la secuencia
  float ultSeqTimer = 0.0f; // Timer general de la secuencia
  int ultSeqPhase = 0;      // 0=inactivo 1=sombras 2=tajo_final 3=buff

  // Sombras de la Ult (2 entidades que cruzan en X)
  struct UltShadow {
    Vector2 position;
    Vector2 velocity;
    float lifetime;
    bool active;
  } ultShadows[2];
  bool ultFinalSlash = false;    // Se activo el tajo final
  bool ultFinalSlashHit = false; // El tajo final ya golpeo
  ReaperState prevReaperState = ReaperState::NORMAL;

  Reaper(Vector2 pos) {
    position = pos;
    radius = 20.0f;
    maxHp = 200.0f;
    hp = maxHp;
    color = {160, 0, 220, 255}; // Purpura oscuro
  }

  void Update() override;
  void Draw() override;
  void Reset(Vector2 pos) override;
  void HandleSkills(Boss &boss) override;
  void CheckCollisions(Boss &boss) override;
  bool IsImmune() const override { return state == ReaperState::DASHING; }
  
  void CancelAttack() override {
      state = ReaperState::NORMAL;
      attackPhase = AttackPhase::NONE;
      hasHit = false;
      isCharging = false;
      holdTimer = 0.0f;
      heavyHasHit = false;
      qActive = false;
  }
  
  std::vector<AbilityInfo> GetAbilities() const override;

  std::string GetName() const override { return "[SEGADOR]"; }
  Color GetHUDColor() const override { return {255, 0, 255, 255}; }

  bool IsTimeStoppedActive() const override {
    return state == ReaperState::LOCKED;
  }
  std::string GetSpecialStatus() const override;
  bool IsBuffed() const override { return isBuffed; }
  float GetBuffTimer() const override { return buffTimer; }

  bool CheckComboCollision(Boss &boss);
  bool CheckHeavyCollision(Boss &boss);
  bool CheckUltFinalSlash(Boss &boss);
  void StartGroundBurstChain(); // Inicia la cadena Q
  void LaunchHomingOrbs(Boss &boss);
  void ActivateUltimate(Vector2 bossPos);
};
