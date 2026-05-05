#pragma once
#include "Entity.h"
#include "CommonTypes.h"
#include "graphics/VFXSystem.h"

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
  bool desperationResists = false;
  bool desperationImmune = false; // Invulnerabilidad durante la avalancha del 10%
  bool isStaggered = false;
  float staggerTimer = 0.0f;

  // ── Mecánica Pasiva Estática (Elemental Mage) ──────────────────────
  int staticStacks = 0;
  float staticTimer = 0.0f;
  ElementMode lastElementHit = ElementMode::NONE;

  virtual void ApplyElement(ElementMode element) {
    if (desperationResists) return; // Inmune a DoT en fase final
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

  virtual void TakeDamage(float dmg, float poiseDmg, Vector2 pushVel) {
    if (isDead || isDying || IsInvulnerable()) return;
    
    // Si estamos en medio de una fase crítica (avalanche/desperation), protegemos el poise
    // Esto es un comportamiento por defecto razonable para Bosses.
    bool poiseProtected = desperationResists;

    if (!poiseProtected) {
        if (!isStaggered) {
            poiseCurrent -= poiseDmg;
            poiseRegenTimer = 5.0f;
            if (poiseCurrent <= 0) {
                poiseCurrent = poiseMax;
                isStaggered = true;
                staggerTimer = 1.5f;
                // VFX for posture break
                Graphics::SpawnImpactBurst(position, {0, -1}, GOLD, WHITE, 25, 10);
            }
        }
    }

    hp -= dmg;
    velocity = Vector2Add(velocity, pushVel);
    recentDamage += dmg;
    recentDamageTimer = 1.0f;
    hitFlashTimer = 0.15f;
  }

  // ── Mecánica de Poise (Equilibrio / Stagger) ──────────────────────
  float poiseCurrent = 300.0f;
  float poiseMax = 300.0f;
  float poiseRegenTimer = 0.0f;

  // ApplyBleed genérico (sin escalado). Para el Reaper usar ApplyBleedScaled().
  // bleedTotalDamage = daño plano por tick (configurable externamente).
  virtual void ApplyBleed(float flatDmgPerTick = 16.0f) {
    if (desperationResists) return;
    isBleeding      = true;
    bleedTimer      = 10.0f;
    bleedTickTimer  = 0.5f;
    bleedTotalDamage = flatDmgPerTick;
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

    if (isStaggered) {
        staggerTimer -= dt;
        if (staggerTimer <= 0) {
            isStaggered = false;
        }
    } else {
        if (poiseRegenTimer > 0) {
            poiseRegenTimer -= dt;
        } else if (poiseCurrent < poiseMax) {
            poiseCurrent += (poiseMax * 0.1f) * dt; // 10s back to full
            if (poiseCurrent > poiseMax) poiseCurrent = poiseMax;
        }
    }

    if (hitFlashTimer > 0) hitFlashTimer -= dt;
    if (stunTimer > 0) stunTimer -= dt;
    if (slowTimer > 0) slowTimer -= dt;
  }

  virtual bool IsInvulnerable() const { return false; }
  // Eje Z Falso: altura visual (p.ej. salto del Golem)
  virtual float GetFakeZ() const { return 0.0f; }
};
