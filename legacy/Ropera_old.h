#pragma once
#include "CommonTypes.h"
#include "Boss.h"
#include "Player.h"
#include "DirectionUtils.h"
class Ropera : public Player {
public:
  RoperaState state = RoperaState::NORMAL;

  // --- Combo de 3 golpes (Refined ranges) ---
  //              range    angle   dmg   startup  active  recovery  hitCD
  AttackFrame combo[3] = {
      {210.0f, 18.0f, 12.0f, 0.22f, 0.14f, 0.22f,
       0.05f}, // Hit 1: Estocada frontal
      {200.0f, 50.0f, 18.0f, 0.25f, 0.15f, 0.26f,
       0.05f}, // Hit 2: Tajo lateral
      {280.0f, 22.0f, 38.0f, 0.35f, 0.25f, 0.40f,
       0.10f}, // Hit 3: Gran Estocada Final (Larger Thrust)
  };

  // --- Ataque cargado (hold click) ---
  int ultHeavyCharges = 0;           // Cargas de ataque pesado en definitiva (max 4)
  std::vector<int> lungeHitIds;      // IDs de enemigos golpeados en este lunge
  Vector2 lockedHeavyTargetPos = {0,0}; // Posicion fijada para auto-aim en Ult
  bool hasLockedTarget = false;      // Indica si ya fijamos un objetivo en la rafaga

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
  int ultHeavySeqIndex = 0;      // Indice del tajo en la secuencia de 4 (X-pattern)
  float ultSeqResetTimer = 0.0f; // Reset de la secuencia ante inactividad

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

  static constexpr float swordHitDamage = 5.0f; // Reducido aún más para balancear el burst de la R
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

  std::vector<AbilityInfo> GetAbilities() const override;

  std::string GetName() const override { return "[ROPERA]"; }
  Color GetHUDColor() const override { return {0, 220, 180, 255}; }

  void CancelAttack() override {
      state = RoperaState::NORMAL;
      attackPhase = AttackPhase::NONE;
      hasHit = false;
      isCharging = false;
      holdTimer = 0.0f;
      heavyHasHit = false;
      ultHeavySeqIndex = 0;
      qActive = false;
  }

  bool IsImmune() const override {
    // Fases del Roll: Total 0.45s 
    // Startup (0.45 -> 0.40): Vulnerable
    // Active (0.40 -> 0.15): i-frames
    // Recovery (0.15 -> 0.00): Vulnerable
    bool isRollingIframe = (state == RoperaState::DASHING && dashGraceTimer <= 0.40f && dashGraceTimer > 0.15f);
    return isRollingIframe || (ultActive && state == RoperaState::HEAVY_ATTACK);
  }

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

private:
  int GetDirectionIndex(Vector2 dir) const {
      // Mapeo basado en el orden de las filas del sheet (SE, NE, S, SW, W, NW, E, N)
      float angle = atan2f(dir.y, dir.x) * RAD2DEG;
      if (angle < 0) angle += 360.0f;

      if (angle >= 337.5f || angle < 22.5f)   return 6; // E
      if (angle >= 22.5f && angle < 67.5f)    return 0; // SE
      if (angle >= 67.5f && angle < 112.5f)   return 2; // S
      if (angle >= 112.5f && angle < 157.5f)  return 3; // SW
      if (angle >= 157.5f && angle < 202.5f)  return 4; // W
      if (angle >= 202.5f && angle < 247.5f)  return 5; // NW
      if (angle >= 247.5f && angle < 292.5f)  return 7; // N
      if (angle >= 292.5f && angle < 337.5f)  return 1; // NE
      return 2; // Default S
  }
};
