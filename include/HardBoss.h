#pragma once
#include "Boss.h"
#include "Player.h"
#include "raymath.h"

class HardBoss : public Boss {
public:
  HardBoss(Vector2 pos) {
    spawnPos = pos;
    position = pos;
    radius = 45.0f;
    maxHp = 1500.0f;
    hp = maxHp;
    color = {255, 50, 200, 255};
  }

  void Update() override {
    if (hp <= 0 && !isDead && !isDying) {
      isDying = true;
      deathAnimTimer = 1.0f;
    }

    if (isDying) {
      deathAnimTimer -= GetFrameTime() * g_timeScale;
      velocity = Vector2Scale(velocity, 0.85f);
      if (deathAnimTimer <= 0) {
        isDying = false;
        isDead = true;
      }
    }

    if (isDead) return;

    Vector2 next = Vector2Add(position, Vector2Scale(velocity, GetFrameTime() * g_timeScale));
    position = next; // No collisions with arena here just for draft
    velocity = Vector2Scale(velocity, 0.90f);
  }

  void Draw() override {
    if (isDead) return;
    Color drawColor = isDying ? Fade(color, 0.5f) : color;
    DrawCircleV(position, radius, drawColor);
    DrawLineV(position, Vector2Add(position, Vector2Scale(facing, radius * 1.5f)), WHITE);
  }

  void UpdateAI(Player &player) override {
    if (isDead || isDying) return;

    // TODO: Implementar patrones complejos
    Vector2 diff = Vector2Subtract(player.position, position);
    if (Vector2Length(diff) > 0) {
      facing = Vector2Normalize(diff);
      if (Vector2Length(diff) > 100.0f) {
        velocity = Vector2Add(velocity, Vector2Scale(facing, 800.0f * GetFrameTime() * g_timeScale));
      }
    }
  }

  void ScaleDifficulty(int wave) override {
    maxHp *= (1.0f + wave * 0.25f);
    hp = maxHp;
  }
};
