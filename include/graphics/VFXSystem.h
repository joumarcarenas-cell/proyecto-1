#pragma once
#include "RenderManager.h"
#include "../ResourceManager.h"
#include <algorithm>
#include <raylib.h>
#include <raymath.h>
#include <vector>

namespace Graphics {

enum class RenderType { CIRCLE, RHOMB, GLOW_LINE, SPRITE };

struct Particle {
  Vector2 pos;
  Vector2 vel;
  float life;
  float maxLife; 
  float size;    
  Color startCol;
  Color endCol;
  RenderType type = RenderType::CIRCLE;
  BlendMode blendMode = BLEND_ALPHA;
  Texture2D* texture = nullptr; // For SPRITE type
  float rotation = 0;
  float rotationVel = 0;
};

class VFXSystem {
public:
  static VFXSystem &GetInstance() {
    static VFXSystem instance;
    return instance;
  }

  VFXSystem(const VFXSystem &) = delete;
  VFXSystem &operator=(const VFXSystem &) = delete;

  // Spawn basico (compatible con llamadas existentes)
  void SpawnParticle(Vector2 pos, Vector2 vel, float life, Color col) {
    SpawnParticleEx(pos, vel, life, col, 3.5f);
  }

  // Spawn versátil
  void SpawnParticleEx(Vector2 pos, Vector2 vel, float life, Color col,
                       float size, RenderType type = RenderType::CIRCLE, 
                       BlendMode blend = BLEND_ALPHA) {
    Particle p;
    p.pos = pos;
    p.vel = vel;
    p.life = p.maxLife = life;
    p.size = size;
    p.startCol = col;
    p.endCol = col;
    p.type = type;
    p.blendMode = blend;
    particles.push_back(p);
  }

  // Spawn con Gradiente y Textura
  void SpawnHybrid(Vector2 pos, Vector2 vel, float life, Color startCol, Color endCol,
                   float size, RenderType type, BlendMode blend, 
                   float rotation = 0, float rotationVel = 0, Texture2D* tex = nullptr) {
    Particle p;
    p.pos = pos;
    p.vel = vel;
    p.life = p.maxLife = life;
    p.size = size;
    p.startCol = startCol;
    p.endCol = endCol;
    p.type = type;
    p.blendMode = blend;
    p.rotation = rotation;
    p.rotationVel = rotationVel;
    p.texture = tex;
    particles.push_back(p);
  }

  void Clear() { particles.clear(); ghosts.clear(); }

  // ─── GHOST TRAILS (After-images) ─────────────────────
  struct Ghost {
    Vector2 pos;
    Rectangle src;
    float life;
    float maxLife;
    Color color;
    bool flipX;
    float scale;
    Vector2 origin;
    Texture2D tex;
  };

  void SpawnGhost(Vector2 pos, Rectangle src, float life, Color col, bool flipX, float scale, Vector2 origin, Texture2D tex) {
    ghosts.push_back({pos, src, life, life, col, flipX, scale, origin, tex});
  }

  void Update(float dt) {
    for (auto &p : particles) {
      p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt));
      p.vel = Vector2Scale(p.vel, 0.92f); // friccion suave
      p.rotation += p.rotationVel * dt;
      p.life -= dt;
    }
    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
                       [](const Particle &p) { return p.life <= 0; }),
        particles.end());

    for (auto &g : ghosts) {
      g.life -= dt;
    }
    ghosts.erase(
        std::remove_if(ghosts.begin(), ghosts.end(),
                       [](const Ghost &g) { return g.life <= 0; }),
        ghosts.end());
  }

  void SubmitDraws() const {
    // 1. Dibujar Ghost Trails primero
    for (const auto &g : ghosts) {
      Graphics::RenderManager::GetInstance().Submit(
          g.pos.y - 1.0f,
          [g]() {
            float t = g.life / g.maxLife;
            Color c = Fade(g.color, t * 0.5f);
            Rectangle dest = { g.pos.x, g.pos.y, g.src.width * g.scale, g.src.height * g.scale };
            DrawTexturePro(g.tex, g.src, dest, g.origin, 0.0f, c);
          },
          Graphics::RenderLayer::VFX);
    }

    // 2. Dibujar Particulas con sus BlendModes y Tipos
    for (const auto &p : particles) {
      Graphics::RenderManager::GetInstance().Submit(
          p.pos.y,
          [p]() {
            float t = p.life / p.maxLife; 
            float alpha = t * t;          
            
            // Mezclar colores start -> end
            Color currentCol = {
              (unsigned char)Lerp(p.endCol.r, p.startCol.r, t),
              (unsigned char)Lerp(p.endCol.g, p.startCol.g, t),
              (unsigned char)Lerp(p.endCol.b, p.startCol.b, t),
              (unsigned char)Lerp(p.endCol.a, p.startCol.a, alpha)
            };

            float s = p.size * (0.4f + 0.6f * t);

            if (p.type == RenderType::SPRITE && p.texture != nullptr) {
                Rectangle src = { 0, 0, (float)p.texture->width, (float)p.texture->height };
                Rectangle dest = { p.pos.x, p.pos.y, s * 2, s * 2 };
                Vector2 origin = { s, s };
                DrawTexturePro(*p.texture, src, dest, origin, p.rotation, currentCol);
            } else if (p.type == RenderType::RHOMB) {
                DrawPoly(p.pos, 4, s, p.rotation, currentCol);
            } else {
                // Default: Circle
                DrawCircleV(p.pos, s, currentCol);
            }
          },
          Graphics::RenderLayer::VFX,
          p.blendMode);
    }
  }

private:
  VFXSystem() = default;
  std::vector<Particle> particles;
  std::vector<Ghost> ghosts;
};

// ────────────────────────────────────────────────────────────────────
// Helpers de spawn para efectos comunes (llamar desde GameplayScene)
// ────────────────────────────────────────────────────────────────────

// Burst direccional al impactar (chispas concentradas + fragmentos)
inline void SpawnImpactBurst(Vector2 origin, Vector2 facing, Color sparkColor,
                             Color fragmentColor, int sparks = 10,
                             int fragments = 6) {
  auto &vfx = VFXSystem::GetInstance();
  float baseAngle = atan2f(facing.y, facing.x);

  // 1. Chispas romboides aditivas (Anime style)
  for (int i = 0; i < sparks; i++) {
    float a = baseAngle + ((float)GetRandomValue(-35, 35)) * DEG2RAD;
    float spd = (float)GetRandomValue(400, 800);
    vfx.SpawnParticleEx(origin, {cosf(a) * spd, sinf(a) * spd},
                        0.25f, sparkColor, (float)GetRandomValue(2, 5), 
                        RenderType::RHOMB, BLEND_ADDITIVE);
  }

  // 2. Fragmentos / Escombros (Alpha blend)
  for (int i = 0; i < fragments; i++) {
    float a = baseAngle + ((float)GetRandomValue(-120, 120)) * DEG2RAD;
    float spd = (float)GetRandomValue(150, 400);
    vfx.SpawnParticleEx(origin, {cosf(a) * spd, sinf(a) * spd},
                        0.4f, fragmentColor, (float)GetRandomValue(4, 7), 
                        RenderType::CIRCLE, BLEND_ALPHA);
  }

  // 3. Flash de impacto central con Sprite (si existe) o circulo aditivo
  if (ResourceManager::texVfxGlow.id != 0) {
      vfx.SpawnHybrid(origin, {0,0}, 0.1f, WHITE, sparkColor, 20.0f, 
                     RenderType::SPRITE, BLEND_ADDITIVE, 0, 0, &ResourceManager::texVfxGlow);
  } else {
      vfx.SpawnParticleEx(origin, {0, 0}, 0.08f, WHITE, 18.0f, RenderType::CIRCLE, BLEND_ADDITIVE);
  }
}

// Estela de dash (particulas cianeadas detras del jugador)
inline void SpawnDashTrail(Vector2 pos) {
  VFXSystem::GetInstance().SpawnParticleEx(pos, {0, 0}, 0.18f,
                                           {0, 220, 200, 255}, 5.0f);
}

// Flash de impacto (en pantalla completa o local)
inline void SpawnHitFlash(Vector2 pos, float radius, Color col) {
    VFXSystem::GetInstance().SpawnParticleEx(pos, {0, 0}, 0.12f, col, radius);
}

} // namespace Graphics
