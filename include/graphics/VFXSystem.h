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
        float life;
        Color col;
    };

    class VFXSystem {
    public:
        static VFXSystem& GetInstance() {
            static VFXSystem instance;
            return instance;
        }

        VFXSystem(const VFXSystem&) = delete;
        VFXSystem& operator=(const VFXSystem&) = delete;

        void SpawnParticle(Vector2 pos, Vector2 vel, float life, Color col) {
            particles.push_back({pos, vel, life, col});
        }

        void Update(float dt) {
            for (auto &p : particles) {
                p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt));
                p.life -= dt;
            }
            particles.erase(
                std::remove_if(particles.begin(), particles.end(),
                            [](const Particle &p) { return p.life <= 0; }),
                particles.end());
        }

        void SubmitDraws() const {
            for (const auto &p : particles) {
                Graphics::RenderManager::GetInstance().Submit(p.pos.y, [p]() { DrawCircleV(p.pos, 3, p.col); });
            }
        }

        void Clear() {
            particles.clear();
        }

    private:
        VFXSystem() = default;
        std::vector<Particle> particles;
    };
}
