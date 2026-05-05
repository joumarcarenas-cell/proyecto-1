#pragma once
#include "CommonTypes.h"
#include "Entity.h"
#include "RPGStats.h"
#include <string>
#include <vector>

class Boss; // Forward declaration

class Player : public Entity {
public:
  Vector2 facing = {1, 0};
  Vector2 targetAim = {0, 0};
  ControlScheme controls;

  // ── Tipo de Daño ─────────────────────────────────────────────────
  enum class DamageType { PHYSICAL, MAGICAL, MIXED, DoT };
  DamageType lastDamageType = DamageType::PHYSICAL;

  // ── RPG Progression ──────────────────────────────────────────────
  RPGStats rpg;

  float energy = 100.0f;
  float maxEnergy = 100.0f;

  // --- Input Buffer ---
  static constexpr float BUFFER_WINDOW = 0.20f; // 200ms window para encadenar ataques
  float attackBufferTimer = 0.0f;               // antiguo inputBufferTimer

  // --- Variables de Estado y Combate Comunes ---
  int comboStep = 0;
  float comboTimer = 0.0f;
  bool hasHit = false;
  AttackPhase attackPhase = AttackPhase::NONE;
  float attackPhaseTimer = 0.0f;
  float hitCooldownTimer = 0.0f;
  int attackId = 0;

  // --- Ataque Cargado ---
  float holdTimer = 0.0f;
  bool isCharging = false;
  bool heavyHasHit = false;

  // --- Eje Z Falso (ilusión 3D: sombra dinámica) ---
  float fakeZ = 0.0f;      // Altura ficticia en píxeles (0 = en el suelo)
  virtual float GetFakeZ() const { return fakeZ; }

  // --- Dash y Cooldowns ---
  int dashCharges = 2;
  int maxDashCharges = 2;
  float dashCooldown1 = 0.0f;
  float dashCooldown2 = 0.0f;
  float dashMaxCD = 1.8f;
  float qCooldown = 0.0f;
  float eCooldown = 0.0f;
  float ultCooldown = 0.0f;

  float m_dashTimer = 0.0f; // Time since last dash started
  static constexpr float PERFECT_DODGE_WINDOW = 0.15f; // Window for perfect dodge (Ampliado para mejor accesibilidad)

  bool isAdminMode = false;

  void UpdateDash(float dt) {
    if (isAdminMode) {
      dashCharges = maxDashCharges;
      dashCooldown1 = 0;
      dashCooldown2 = 0;
      return;
    }
    if (dashCooldown1 > 0) {
      dashCooldown1 -= dt;
      if (dashCooldown1 <= 0)
        dashCharges++;
    }
    if (dashCooldown2 > 0) {
      dashCooldown2 -= dt;
      if (dashCooldown2 <= 0)
        dashCharges++;
    }
  }

  bool CanDash() const { return dashCharges > 0; }
  void UseDash() {
    if (isAdminMode) return; // No consume cargas en modo admin
    if (dashCharges > 0) {
      dashCharges--;
      if (dashCooldown1 <= 0)
        dashCooldown1 = dashMaxCD;
      else if (dashCooldown2 <= 0)
        dashCooldown2 = dashMaxCD;
    }
  }

  virtual void Update() override = 0;
  virtual void Draw() override = 0;
  virtual void Reset(Vector2 pos) = 0;

  // Character info for HUD
  virtual std::string GetName() const = 0;
  virtual Color GetHUDColor() const = 0;

  // Skill handling that might need reference to enemies
  virtual void HandleSkills(Boss &boss) = 0;
  virtual void CheckCollisions(Boss &boss) = 0;

  virtual bool IsImmune() const = 0;
  virtual std::vector<AbilityInfo> GetAbilities() const = 0;

  // UI specific flags
  virtual bool IsTimeStoppedActive() const { return false; }
  virtual std::string GetSpecialStatus() const { return ""; }
  virtual bool IsBuffed() const { return false; }
  virtual float GetBuffTimer() const { return 0.0f; }

  // --- Souls-like Combat Mechanics ---
  bool isStaggered = false;
  float staggerTimer = 0.0f;

  // Perfect Dodge: Buffed state after a frame-perfect dodge
  bool hasPerfectDodgeBuff = false;
  float perfectDodgeTimer = 0.0f;
  bool isPerfectCounter = false; // Indica si el ataque actual es un contraataque de esquiva perfecta

  // Callback to be implemented by scenes for visual effects
  static void (*OnPerfectDodge)(Vector2 pos); 

  virtual void CancelAttack() = 0;

  virtual void TakeDamage(float amount, Vector2 pushVel) {
      if (isAdminMode) return; // MODO ADMIN: Inmortalidad
      if (IsImmune()) {
          // Check for perfect dodge
          if (m_dashTimer <= PERFECT_DODGE_WINDOW && !hasPerfectDodgeBuff) {
              hasPerfectDodgeBuff = true;
              perfectDodgeTimer = 5.0f; // Buff lasts 5 seconds
              if (OnPerfectDodge) OnPerfectDodge(position);
          }
          return;
      }
      
      hp -= amount;
      velocity = Vector2Add(velocity, pushVel);
      
      // Souls-like Stagger: Interrumpe el ataque si estabas cargandolo o en su startup
      if (attackPhase == AttackPhase::STARTUP || isCharging) {
          isStaggered = true;
          staggerTimer = 0.45f;
          hitFlashTimer = 0.25f; // Feedback visual de stagger
          CancelAttack();
      } else {
          // Si no está en STARTUP, recibe daño normal pero no se interrumpe permanentemente
          hitFlashTimer = 0.15f; 
      }
  }
};
