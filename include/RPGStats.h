#pragma once
// =====================================================================
// RPGStats.h  —  Sistema de Progresión RPG
// =====================================================================
// Estructura independiente que puede añadirse a cualquier Player
// sin modificar Entity ni la jerarquía existente.
//
// Uso:
//   1. El Player expone un miembro público  RPGStats rpg;
//   2. Al matar un Boss: rpg.GainXP(xpAmount)  →  devuelve true si levelup
//   3. Multiplicador de daño:  rpg.DamageMultiplier()
// =====================================================================

#include <raylib.h>
#include <string>

struct RPGStats {
  // ── Nivel ──────────────────────────────────────────────────────────
  int nivel = 1;
  float xpActual = 0.0f;
  float xpRequerida = 100.0f; // Crece exponencialmente por nivel

  // ── Puntos distribuibles ───────────────────────────────────────────
  int puntosDisponibles = 0;

  // ── 5 Estadísticas principales ──────────────────────────────────────────
  int statHP = 0;      // +20 maxHp por punto   (max 50)
  int statEnergia = 0; // +10 maxEnergy por punto (max 50)
  int statFuerza = 0;  // +5% daño base por punto (max 50)
  int statMente = 0;   // +5% daño mágico por punto (max 50)
  int statSuerte = 0;  // Probabilidad crítica    (max 50 → 25% chance)

  // ── Límite máximo por stat ──────────────────────────────────────────
  static constexpr int STAT_MAX = 50;

  // ── Sistema de críticos ──────────────────────────────────────────
  // Multiplicador aplicado al daño cuando un golpe es crítico
  static constexpr float CRIT_MULTIPLIER = 1.35f;

  // Probabilidad de crítico: lineal 0.5% por punto de Suerte
  //   statSuerte=0  →  0.00%  chance
  //   statSuerte=25 → 12.50%  chance
  //   statSuerte=50 → 25.00%  chance  (máximo absoluto)
  float CritChance() const {
    // 0.005f = 0.5% por punto; cap explícito en 0.25f aunque stat no exceda 50
    float chance = statSuerte * 0.005f;
    if (chance > 0.25f) chance = 0.25f;
    return chance;
  }

  // ── Escalado de XP requerida por nivel ─────────────────────────────
  // xpRequerida = 100 * 1.35^(nivel-1)  → curva suave pero progresiva
  static float CalcXpRequired(int lv) {
    float base = 100.0f;
    float factor = 1.35f;
    for (int i = 1; i < lv; ++i)
      base *= factor;
    return base;
  }

  // ── Intentar ganar XP; devuelve true si subió de nivel ─────────────
  bool GainXP(float amount) {
    xpActual += amount;
    if (xpActual >= xpRequerida) {
      xpActual -= xpRequerida;
      nivel++;
      xpRequerida = CalcXpRequired(nivel);
      return true; // LEVEL UP
    }
    return false;
  }

  // ── Multiplicadores de daño (Físico, Mágico, Mixto) ────────────────────────
  // Base: +5% automático por nivel
  float DamageMultiplierPhysical() const {
    float autoBonus = 1.0f + (nivel - 1) * 0.05f;
    float statBonus = 1.0f + statFuerza * 0.05f;
    return autoBonus * statBonus;
  }

  float DamageMultiplierMagical() const {
    float autoBonus = 1.0f + (nivel - 1) * 0.05f;
    float statBonus = 1.0f + statMente * 0.05f;
    return autoBonus * statBonus;
  }

  float DamageMultiplierMixed() const {
    float autoBonus = 1.0f + (nivel - 1) * 0.05f;
    float fBonus = 1.0f + statFuerza * 0.05f;
    float mBonus = 1.0f + statMente * 0.05f;
    return autoBonus * ((fBonus + mBonus) * 0.5f);
  }

  // ── Bonus plano de HP máximo ─────────────────────────────────────────
  float MaxHpBonus() const { return statHP * 20.0f; }

  // ── Bonus plano de Energía máxima ────────────────────────────────────
  float MaxEnergyBonus() const { return statEnergia * 10.0f; }

  // ── Poner un punto en un stat (respeta el cap STAT_MAX) ───────────────
  bool SpendPoint(int &stat) {
    if (puntosDisponibles <= 0 || stat >= STAT_MAX)
      return false;
    puntosDisponibles--;
    stat++;
    return true;
  }

  // ── Tirar el dado de crítico ──────────────────────────────────────────
  // Llama a GetRandomValue de Raylib (requiere #include <raylib.h> en el .cpp
  // que lo invoque; esta cabecera solo declara la firma).
  // Devuelve true si el ataque es crítico este frame.
  bool RollCrit() const {
    // Resolución: 1 en 10 000 para máxima precisión del porcentaje
    int threshold = (int)(CritChance() * 10000.0f); // e.g. 50pts → 2500
    return GetRandomValue(0, 9999) < threshold;
  }
};
