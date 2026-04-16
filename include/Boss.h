#pragma once
#include "Entity.h"
#include "CommonTypes.h"

class Player;

class Boss : public Entity {
public:
  // ── Spawn / muerte ───────────────────────────────────────────────
  Vector2 spawnPos;
  float respawnTimer = 0.0f;
  bool isDead = false;
  bool isDying = false;
  float deathAnimTimer = 0.0f;

  float recentDamage = 0.0f;
  float recentDamageTimer = 0.0f;
  Vector2 facing = {1, 0};
  bool hasHit = false;

  // ── Mecánica Pasiva Estática (Elemental Mage) ──────────────────────
  int staticStacks = 0;
  float staticTimer = 0.0f;
  ElementMode lastElementHit = ElementMode::NONE;

  virtual void ApplyElement(ElementMode element) {
    if (element != ElementMode::NONE) {
      lastElementHit = element;
      staticTimer = 7.0f;
    }
  }

  virtual void OnStagger() {}

  // ── Sangrado (compartido para mecánicas del Reaper) ──────────────
  bool isBleeding = false;
  float bleedTimer = 0.0f;
  float bleedTickTimer = 0.0f;
  float bleedTotalDamage = 0.0f;

  virtual ~Boss() = default;

  // Implementaciones virtuales requeridas
  virtual void UpdateAI(Player &player) = 0;
  virtual void ScaleDifficulty(int wave) = 0;

  virtual void ApplyBleed() {
    isBleeding = true;
    bleedTimer = 10.0f;
    bleedTickTimer = 0.5f;
    bleedTotalDamage = (maxHp * 0.05f) / 10.0f;
  }

  virtual float GetRemainingBleedDamage() const {
    if (!isBleeding || bleedTimer <= 0.0f)
      return 0.0f;
    return bleedTotalDamage * (bleedTimer / 0.5f);
  }

  virtual void UpdateBossStatus(float dt) {
    if (staticTimer > 0) {
      staticTimer -= dt;
      if (staticTimer <= 0) {
        staticStacks = 0;
        lastElementHit = ElementMode::NONE;
      }
    }

    if (recentDamageTimer > 0) {
      recentDamageTimer -= dt;
      if (recentDamageTimer <= 0)
        recentDamage = 0;
    }

    if (hitFlashTimer > 0) hitFlashTimer -= dt;
    if (stunTimer > 0) stunTimer -= dt;
    if (slowTimer > 0) slowTimer -= dt;
  }

  virtual bool IsInvulnerable() const { return false; }
};
