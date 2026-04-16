#pragma once
#include "CommonTypes.h"
#include "Boss.h"
#include "Player.h"

// =====================================================
// --- CLASE ROPERA ---
// =====================================================
class Ropera : public Player {
public:
  RoperaState state = RoperaState::NORMAL;

  // --- Combo de 3 golpes (+18% rango vs Reaper) ---
  //              range    angle   dmg   startup  active  recovery  hitCD
  AttackFrame combo[3] = {
      {170.0f, 52.0f, 18.0f, 0.22f, 0.14f, 0.22f,
       0.05f}, // Hit 1: Estocada frontal (+15% reach, dmg reverted)
      {156.0f, 95.0f, 20.4f, 0.25f, 0.15f, 0.26f,
       0.05f}, // Hit 2: Tajo lateral 90deg (+13% reach, dmg reverted)
      {160.0f, 50.0f, 14.4f, 0.32f, 0.35f, 0.35f,
       0.11f}, // Hit 3: Rafaga x3 (+12% reach, dmg reverted)
  };

  // Hit 3: paso atras + rafaga con 3 sub-hits individuales
  bool step3BackDone = false;
  int burstHitCount = 0;       // cuantos sub-hits ejecutados (max 3)
  float burstSubTimer = 0.0f;  // delay entre cada sub-hit (0.12s)
  bool burstSubActive = false; // rafaga en progreso

  // --- Ataque cargado (hold click) ---

  // --- Dash independiente (SPACE), con i-frames ---
  float dashGraceTimer = 0.0f; // duracion de i-frames y del dash fisico

  // --- Habilidad Q: Dos tajos angulo cerrado ---
  float qMaxCooldown = 8.0f;
  int qSlashIndex = 0;            // 0=primer tajo, 1=segundo
  float qSlashActiveTimer = 0.0f; // ventana activa del tajo actual
  float qSlashGapTimer = 0.0f;    // pausa entre el tajo 1 y 2
  bool qActive = false;
  bool qHasHit = false;

  // --- Habilidad E: Buff activo ---
  float eMaxCooldown = 14.0f;
  float eBuffTimer = 0.0f;
  bool eBuffActive = false;
  static constexpr float eLifestealFrac = 0.15f; // Reducido a 0.15 a peticion del usuario
  static constexpr float eMaxHpBonusFrac = 0.03f;

  // --- Buff de velocidad (Q con hit) ---
  float moveSpeedBuffTimer = 0.0f;

  // --- Ultimate: Modo Garras (requiere hp < 60%) ---
  float ultMaxCooldown = 30.0f;
  float ultTimer = 0.0f;
  bool ultActive = false;

  // --- Espadas Voladoras: flotan detras, se disparan al golpear ---
  enum class SwordState { BEHIND, FIRING, RETURNING };
  struct FlyingSword {
    Vector2 position = {0, 0};
    SwordState swordState = SwordState::BEHIND;
    float fireDelay = 0.0f; // delay escalonado (0, 0.1, 0.2s)
    float fireDelayTimer = 0.0f;
    Vector2 targetPos = {0, 0};
    bool hasDealt = false;
    float flashTimer = 0.0f;
    bool active = false;
  } swords[3];

  static constexpr float swordHitDamage = 10.8f; // 9.0 * 1.2 (+20%)
  static constexpr float swordFireSpeed = 1600.0f;
  static constexpr float swordReturnSpeed = 1200.0f;
  static constexpr float swordBehindDist = 48.0f;
  Vector2 swordTargetSnapshot = {0, 0};

  Ropera(Vector2 pos) {
    position = pos;
    radius = 20.0f;
    maxHp = 200.0f;
    hp = maxHp;
    color = {0, 180, 160, 255};
    for (int i = 0; i < 3; i++) {
      swords[i].swordState = SwordState::BEHIND;
      swords[i].fireDelay = i * 0.10f;
      swords[i].active = false;
    }
  }

  void Update() override;
  void Draw() override;
  void Reset(Vector2 pos) override;
  void HandleSkills(Boss &boss) override;
  void CheckCollisions(Boss &boss) override;

  bool IsImmune() const override { return state == RoperaState::DASHING; }
  std::vector<AbilityInfo> GetAbilities() const override;

  std::string GetName() const override { return "[ROPERA]"; }
  Color GetHUDColor() const override { return {0, 220, 180, 255}; }

  bool IsBuffed() const override {
    return eBuffActive || moveSpeedBuffTimer > 0.0f || ultActive;
  }
  float GetBuffTimer() const override {
    if (ultActive)
      return ultTimer;
    if (eBuffActive)
      return eBuffTimer;
    return moveSpeedBuffTimer;
  }
  std::string GetSpecialStatus() const override;

  bool CheckComboCollision(Boss &boss);
  bool CheckHeavyCollision(Boss &boss);
  bool CheckQCollision(Boss &boss, int slashIdx);
  void TriggerSwords(Vector2 enemyPos);
  void InitSwords();
  void UpdateSwords(float dt, Boss &boss);
};
