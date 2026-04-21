#pragma once
#include "Boss.h"
#include "Player.h"
#include "raymath.h"
#include "include/graphics/VFXSystem.h"

extern float g_timeScale;
extern float screenShake;

// =====================================================================
// EtherCorrupto - Boss Final de la Demo Técnica
// =====================================================================
// Un ser de energía corrompida que ataca en fases.
// TODO: implementar patrones de ataque complejos cuando el artista
//       entregue assets y se defina el diseño final.
//
// FASE 1 (HP > 50%): Combate melee + proyectiles orbitales lentos
// FASE 2 (HP ≤ 50%): Berserker - velocidad y daño aumentan,
//                    aparecen rafagas de energía en área
// =====================================================================

class EtherCorrupto : public Boss {
public:
    // ── Estado interno ───────────────────────────────────────────────
    enum class ECPhase { PHASE1, PHASE2 };
    ECPhase phase = ECPhase::PHASE1;
    bool phaseTransitioned = false;

    float attackCooldown = 0.0f;
    float baseAttackCooldown = 1.8f;
    float stateTimer = 0.0f;
    bool hasHit = false;

    // Proyectiles orbitales (decorativos + colisión)
    struct OrbitalProjectile {
        float angle = 0.0f;  // ángulo actual en grados
        float orbitRadius = 0.0f;
        bool active = false;
    } orbitals[4];

    float orbitalSpeed = 90.0f; // grados por segundo

    EtherCorrupto(Vector2 pos) {
        spawnPos = pos;
        position = pos;
        radius  = 40.0f;
        maxHp   = 3200.0f;
        hp      = maxHp;
        color   = {80, 0, 180, 255}; // Violeta profundo

        // Inicializar orbitales equidistantes
        for (int i = 0; i < 4; i++) {
            orbitals[i].angle       = (float)i * 90.0f;
            orbitals[i].orbitRadius = 80.0f;
            orbitals[i].active      = true;
        }
    }

    void Update() override {
        if (hp <= 0 && !isDead && !isDying) {
            isDying = true;
            deathAnimTimer = 1.5f;
            // VFX grandiosa de muerte
            for (int i = 0; i < 30; i++) {
                Graphics::VFXSystem::GetInstance().SpawnParticle(
                    position,
                    {(float)GetRandomValue(-400, 400), (float)GetRandomValue(-400, 400)},
                    1.2f, color);
            }
        }

        if (isDying) {
            deathAnimTimer -= GetFrameTime() * g_timeScale;
            velocity = Vector2Scale(velocity, 0.80f);
            // Pulso de expansión durante la muerte
            screenShake = fmaxf(screenShake, 3.0f * (deathAnimTimer / 1.5f));
            if (deathAnimTimer <= 0) {
                isDying = false;
                isDead  = true;
            }
        }

        if (isDead) return;

        float dt = GetFrameTime() * g_timeScale;

        // Mover proyectiles orbitales
        for (int i = 0; i < 4; i++) {
            if (!orbitals[i].active) continue;
            orbitals[i].angle += orbitalSpeed * dt;
            if (orbitals[i].angle >= 360.0f) orbitals[i].angle -= 360.0f;
        }

        // Fisica base
        Vector2 next = Vector2Add(position, Vector2Scale(velocity, dt));
        position = Arena::GetClampedPos(next, radius);
        velocity = Vector2Scale(velocity, 0.88f);
    }

    void Draw() override {
        if (isDead) return;

        float t = (float)GetTime();
        float pulse = 0.5f + 0.5f * sinf(t * 3.0f);
        Color drawColor = isDying ? Fade(color, 0.5f) : color;

        // Aura exterior pulsante
        drawColor.a = (unsigned char)(180 + 60.0f * pulse);
        float auraRadius = radius * (1.2f + 0.15f * pulse);
        DrawCircleV({position.x, position.y - 25}, auraRadius, Fade(color, 0.25f * pulse));
        DrawCircleLinesV({position.x, position.y - 25}, auraRadius, Fade({180, 100, 255, 255}, 0.7f));

        // Cuerpo principal
        DrawCircleV({position.x, position.y - 25}, radius, drawColor);
        DrawCircleLinesV({position.x, position.y - 25}, radius * 0.6f, Fade(WHITE, 0.5f));

        // Indicador de dirección
        DrawLineEx(
            {position.x, position.y - 25},
            Vector2Add({position.x, position.y - 25}, Vector2Scale(facing, radius * 1.4f)),
            4.0f, Fade(WHITE, 0.8f));

        // Orbitales
        for (int i = 0; i < 4; i++) {
            if (!orbitals[i].active) continue;
            float rad = orbitals[i].angle * DEG2RAD;
            Vector2 orbPos = {
                position.x + cosf(rad) * orbitals[i].orbitRadius,
                position.y - 25 + sinf(rad) * orbitals[i].orbitRadius * 0.5f
            };
            DrawCircleV(orbPos, 10.0f, Fade({200, 100, 255, 255}, 0.85f));
            DrawCircleLinesV(orbPos, 13.0f, Fade(WHITE, 0.5f));
        }

        // Barra de vida flotante
        if (!isDead && !isDying) {
            float hpPct = fmaxf(0.0f, hp / maxHp);
            float bx = position.x - 36, by = position.y - 68;
            DrawRectangle((int)bx,       (int)by, 72, 8, {20, 0, 40, 220});
            DrawRectangle((int)bx,       (int)by, (int)(72 * hpPct), 8,
                          (phase == ECPhase::PHASE2) ? RED : Color{160, 0, 255, 255});
            DrawRectangleLinesEx({bx, by, 72, 8}, 1.5f, {180, 100, 255, 200});

            // Indicador de fase
            if (phase == ECPhase::PHASE2) {
                const char* p2txt = "FASE II";
                DrawText(p2txt, (int)(position.x - MeasureText(p2txt, 10) / 2), (int)(by - 14), 10, RED);
            }
        }
    }

    void UpdateAI(Player& player) override {
        if (isDead || isDying) return;
        float dt = GetFrameTime() * g_timeScale;

        // Transición a Fase 2
        if (!phaseTransitioned && hp <= maxHp * 0.5f) {
            phaseTransitioned = true;
            phase = ECPhase::PHASE2;
            baseAttackCooldown = 1.1f;
            orbitalSpeed = 160.0f;
            color = {200, 0, 100, 255}; // Rojo oscuro en berserker
            screenShake = fmaxf(screenShake, 4.0f);
            for (int i = 0; i < 20; i++) {
                Graphics::VFXSystem::GetInstance().SpawnParticle(
                    position,
                    {(float)GetRandomValue(-300, 300), (float)GetRandomValue(-300, 300)},
                    0.8f, {255, 50, 100, 255});
            }
        }

        if (attackCooldown > 0) attackCooldown -= dt;

        Vector2 diff = Vector2Subtract(player.position, position);
        float dist = Vector2Length(diff);
        if (dist > 0) facing = Vector2Normalize(diff);

        // Movimiento de acercamiento
        float speed = (phase == ECPhase::PHASE2) ? 320.0f : 220.0f;
        if (dist > 120.0f) {
            velocity = Vector2Add(velocity, Vector2Scale(facing, speed * 5.0f * dt));
        }

        // Colisión de orbitales con el jugador (daño de contacto)
        for (int i = 0; i < 4; i++) {
                if (!orbitals[i].active) continue;
                float rad = orbitals[i].angle * DEG2RAD;
                Vector2 orbPos = {
                    position.x + cosf(rad) * orbitals[i].orbitRadius,
                    position.y - 25 + sinf(rad) * orbitals[i].orbitRadius * 0.5f
                };
                if (CheckCollisionCircles(orbPos, 10.0f, player.position, player.radius)) {
                    float dmg = (phase == ECPhase::PHASE2) ? 22.0f : 14.0f;
                    player.hp -= dmg * dt; // DoT de contacto
                }
            }
    }

    void ScaleDifficulty(int wave) override {
        float mult = 1.0f + (wave - 3) * 0.3f;
        maxHp *= mult;
        hp     = maxHp;
    }

    // Método para verificar colisiones de orbitales (llamado desde GameplayScene)
    bool CheckOrbitalHit(Vector2 playerPos, float playerRadius, float& dmgOut) {
        if (isDead || isDying) return false;
        for (int i = 0; i < 4; i++) {
            if (!orbitals[i].active) continue;
            float rad = orbitals[i].angle * DEG2RAD;
            Vector2 orbPos = {
                position.x + cosf(rad) * orbitals[i].orbitRadius,
                position.y - 25 + sinf(rad) * orbitals[i].orbitRadius * 0.5f
            };
            if (CheckCollisionCircles(orbPos, 12.0f, playerPos, playerRadius)) {
                dmgOut = (phase == ECPhase::PHASE2) ? 18.0f : 10.0f;
                return true;
            }
        }
        return false;
    }
};
