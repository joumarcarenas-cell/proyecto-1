#pragma once
#include <raylib.h>
#include <raymath.h>
#include <vector>
#include <algorithm>
#include "RenderManager.h"

namespace Graphics {

    struct Particle {
        Vector2 pos;
        Vector2 vel;
        float   life;
        float   maxLife;  // para calcular alpha y escala por lifetime
        float   size;     // radio en px al maximo de vida
        Color   col;
    };

    class VFXSystem {
    public:
        static VFXSystem& GetInstance() {
            static VFXSystem instance;
            return instance;
        }

        VFXSystem(const VFXSystem&) = delete;
        VFXSystem& operator=(const VFXSystem&) = delete;

        // Spawn basico (compatible con llamadas existentes en el proyecto)
        void SpawnParticle(Vector2 pos, Vector2 vel, float life, Color col) {
            particles.push_back({pos, vel, life, life, 3.5f, col});
        }

        // Spawn con tamano personalizado (chispas = 2px, flashes = 12px, etc.)
        void SpawnParticleEx(Vector2 pos, Vector2 vel, float life, Color col, float size) {
            particles.push_back({pos, vel, life, life, size, col});
        }

        void Update(float dt) {
            for (auto& p : particles) {
                p.pos  = Vector2Add(p.pos, Vector2Scale(p.vel, dt));
                p.vel  = Vector2Scale(p.vel, 0.92f); // friccion suave
                p.life -= dt;
            }
            particles.erase(
                std::remove_if(particles.begin(), particles.end(),
                               [](const Particle& p) { return p.life <= 0; }),
                particles.end());
        }

        void SubmitDraws() const {
            for (const auto& p : particles) {
                Graphics::RenderManager::GetInstance().Submit(p.pos.y, [p]() {
                    float t     = p.life / p.maxLife;   // 1 = recien nacida, 0 = muerta
                    float alpha = t * t;                // fade cuadratico (mas natural)
                    float r     = p.size * (0.3f + 0.7f * t); // encoge al morir
                    Color c     = Fade(p.col, alpha);
                    DrawCircleV(p.pos, r, c);
                });
            }
        }

        void Clear() { particles.clear(); }

    private:
        VFXSystem() = default;
        std::vector<Particle> particles;
    };

    // ────────────────────────────────────────────────────────────────────
    // Helpers de spawn para efectos comunes (llamar desde GameplayScene)
    // ────────────────────────────────────────────────────────────────────

    // Burst direccional al impactar (chispas concentradas + fragmentos)
    inline void SpawnImpactBurst(Vector2 origin, Vector2 facing,
                                 Color sparkColor, Color fragmentColor,
                                 int sparks = 10, int fragments = 6) {
        auto& vfx = VFXSystem::GetInstance();
        float baseAngle = atan2f(facing.y, facing.x);

        // 1. Chispas concentradas (+-25 grados, doradas/blancas)
        for (int i = 0; i < sparks; i++) {
            float a   = baseAngle + ((float)GetRandomValue(-25, 25)) * DEG2RAD;
            float spd = (float)GetRandomValue(320, 680);
            vfx.SpawnParticleEx(origin, {cosf(a) * spd, sinf(a) * spd},
                                0.22f + (float)GetRandomValue(0, 12) * 0.01f,
                                sparkColor, 2.0f);
        }

        // 2. Fragmentos de impacto (spread amplio)
        for (int i = 0; i < fragments; i++) {
            float a   = baseAngle + ((float)GetRandomValue(-110, 110)) * DEG2RAD;
            float spd = (float)GetRandomValue(100, 380);
            vfx.SpawnParticleEx(origin, {cosf(a) * spd, sinf(a) * spd},
                                0.35f + (float)GetRandomValue(0, 10) * 0.01f,
                                fragmentColor, 4.5f);
        }

        // 3. Flash blanco en punto de impacto (estatico, vida muy corta)
        vfx.SpawnParticleEx(origin, {0, 0}, 0.07f, WHITE, 14.0f);
    }

    // Estela de dash (particulas cianeadas detras del jugador)
    inline void SpawnDashTrail(Vector2 pos) {
        VFXSystem::GetInstance().SpawnParticleEx(pos, {0, 0}, 0.18f,
                                                 {0, 220, 200, 255}, 5.0f);
    }

} // namespace Graphics
