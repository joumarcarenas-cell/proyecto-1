#pragma once
#include "../ResourceManager.h"
#include "RenderManager.h"
#include <algorithm>
#include <raylib.h>
#include <raymath.h>
#include <vector>

namespace Graphics {

enum class RenderType {
  CIRCLE,
  RHOMB,
  GLOW_LINE,
  SPRITE,
  SDF_CIRCLE,
  SDF_STAR,
  ISO_RING,
  ISO_CIRCLE
};
enum class EasingType {
  LINEAR,
  EASE_OUT_QUAD,
  EASE_OUT_ELASTIC,
  POP_IN_OUT,
  EASE_OUT_EXPO
};

// Fast pseudo-noise for organic motion (Turbulence)
inline float FastNoise2D(float x, float y) {
  return sinf(x * 12.9898f + y * 78.233f) * 43758.5453f -
         floorf(sinf(x * 12.9898f + y * 78.233f) * 43758.5453f);
}

struct Particle {
  Vector2 pos;
  Vector2 vel;
  Vector2 acc = {0, 0}; // Acceleration
  float life;
  float maxLife;
  float size;
  float endSize = -1.0f; // Si es >= 0, interpola hacia este tamaño
  Color startCol;
  Color endCol;
  RenderType type = RenderType::CIRCLE;
  BlendMode blendMode = BLEND_ALPHA;
  EasingType easing = EasingType::POP_IN_OUT;
  Texture2D *texture = nullptr; // For SPRITE type
  float rotation = 0;
  float rotationVel = 0;
  float angularDrag = 1.0f;

  // New Physics & Juice
  float gravity = 0.0f;
  float friction = 0.92f;
  float noiseStrength = 0.0f; // Turbulence power
  float noiseScale = 5.0f;
  bool useBounce = false;
  float bounciness = 0.6f;
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
    p.easing = EasingType::POP_IN_OUT; // Default behavior
    particles.push_back(p);
  }

  // Nuevo Spawn Premium (Fisica Avanzada + Easing + Ruido)
  void SpawnPremium(Vector2 pos, Vector2 vel, Vector2 acc, float life,
                    Color startCol, Color endCol, float startSize,
                    float endSize, RenderType type = RenderType::CIRCLE,
                    BlendMode blend = BLEND_ALPHA,
                    EasingType easing = EasingType::EASE_OUT_QUAD,
                    float friction = 0.92f, float noiseStrength = 0.0f,
                    float rotVel = 0.0f, bool bounce = false) {
    Particle p;
    p.pos = pos;
    p.vel = vel;
    p.acc = acc;
    p.life = p.maxLife = life;
    p.size = startSize;
    p.endSize = endSize;
    p.startCol = startCol;
    p.endCol = endCol;
    p.type = type;
    p.blendMode = blend;
    p.easing = easing;
    p.friction = friction;
    p.noiseStrength = noiseStrength;
    p.rotationVel = rotVel;
    p.useBounce = bounce;
    if (rotVel != 0.0f)
      p.angularDrag = 0.95f;
    particles.push_back(p);
  }

  // Spawn completo para efectos "Juicy"
  void SpawnFull(Vector2 pos, Vector2 vel, float life, Color startCol,
                 Color endCol, float size, RenderType type, BlendMode blend,
                 float gravity = 0, float friction = 0.92f, float rotation = 0,
                 float rotVel = 0, bool bounce = false) {
    Particle p;
    p.pos = pos;
    p.vel = vel;
    p.life = p.maxLife = life;
    p.size = size;
    p.startCol = startCol;
    p.endCol = endCol;
    p.type = type;
    p.blendMode = blend;
    p.gravity = gravity;
    p.friction = friction;
    p.rotation = rotation;
    p.rotationVel = rotVel;
    p.useBounce = bounce;
    particles.push_back(p);
  }

  // Spawn con Gradiente y Textura (Legacy/Compat)
  void SpawnHybrid(Vector2 pos, Vector2 vel, float life, Color startCol,
                   Color endCol, float size, RenderType type, BlendMode blend,
                   float rotation = 0, float rotationVel = 0,
                   Texture2D *tex = nullptr) {
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

  void Clear() {
    particles.clear();
    ghosts.clear();
  }

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

  void SpawnGhost(Vector2 pos, Rectangle src, float life, Color col, bool flipX,
                  float scale, Vector2 origin, Texture2D tex) {
    ghosts.push_back({pos, src, life, life, col, flipX, scale, origin, tex});
  }

  void Update(float dt) {
    float time = (float)GetTime();
    for (auto &p : particles) {
      // Aplicar aceleracion
      p.vel.x += p.acc.x * dt;
      p.vel.y += p.acc.y * dt;

      // Aplicar ruido (Turbulencia organica)
      if (p.noiseStrength > 0.0f) {
        float nx =
            FastNoise2D(p.pos.x * 0.01f * p.noiseScale, time) * 2.0f - 1.0f;
        float ny =
            FastNoise2D(p.pos.y * 0.01f * p.noiseScale, time + 100.0f) * 2.0f -
            1.0f;
        p.vel.x += nx * p.noiseStrength * dt;
        p.vel.y += ny * p.noiseStrength * dt;
      }

      // Aplicar gravedad
      p.vel.y += p.gravity * dt;

      // Movimiento
      p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt));

      // Friccion lineal
      p.vel = Vector2Scale(p.vel, p.friction);

      // Rebote simple en el suelo (arena isometrica)
      if (p.useBounce && p.pos.y > 2800.0f) {
        p.pos.y = 2800.0f;
        p.vel.y *= -p.bounciness;
        p.vel.x *= 0.8f;
      }

      // Friccion angular
      p.rotation += p.rotationVel * dt;
      p.rotationVel *= p.angularDrag;
      p.life -= dt;
    }
    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
                       [](const Particle &p) { return p.life <= 0; }),
        particles.end());

    for (auto &g : ghosts) {
      g.life -= dt;
    }
    ghosts.erase(std::remove_if(ghosts.begin(), ghosts.end(),
                                [](const Ghost &g) { return g.life <= 0; }),
                 ghosts.end());
  }

  void SubmitDraws() const {
    if (!shaderLoaded) {
      sdfShader = LoadShader(nullptr, "assets/shaders/vfx_sdf.fs");
      locShapeType = GetShaderLocation(sdfShader, "shapeType");
      locSoftness = GetShaderLocation(sdfShader, "softness");
      Image blankImg = GenImageColor(4, 4, WHITE);
      blankTex = LoadTextureFromImage(blankImg);
      UnloadImage(blankImg);
      shaderLoaded = true;
    }

    Shader localShader = sdfShader;
    int localLocShape = locShapeType;
    int localLocSoft = locSoftness;
    Texture2D localTex = blankTex;

    // 1. Dibujar Ghost Trails primero
    for (const auto &g : ghosts) {
      Graphics::RenderManager::GetInstance().Submit(
          g.pos.y - 1.0f,
          [g]() {
            float t = g.life / g.maxLife;
            Color c = Fade(g.color, t * 0.52f);
            Rectangle dest = {g.pos.x, g.pos.y, g.src.width * g.scale,
                              g.src.height * g.scale};
            if (g.tex.id == 0) {
              DrawCircleV(g.pos, 25.0f * g.scale, c);
            } else {
              DrawTexturePro(g.tex, g.src, dest, g.origin, 0.0f, c);
            }
          },
          Graphics::RenderLayer::VFX);
    }

    // 2. Dibujar Particulas con sus BlendModes y Tipos
    for (const auto &p : particles) {
      Graphics::RenderManager::GetInstance().Submit(
          p.pos.y,
          [p, localShader, localLocShape, localLocSoft, localTex]() {
            float t = p.life / p.maxLife;
            float progress = 1.0f - t; // 0 (inicio) a 1 (fin)

            float alphaCurve = t;
            float sizeCurve = 1.0f;

            // Funciones matematicas de Easing
            switch (p.easing) {
            case EasingType::LINEAR:
              alphaCurve = t;
              sizeCurve = t;
              break;
            case EasingType::EASE_OUT_QUAD:
              alphaCurve = 1.0f - (progress * progress);
              sizeCurve = 1.0f - (progress * progress);
              break;
            case EasingType::EASE_OUT_EXPO:
              alphaCurve = (progress == 1.0f)
                               ? 0.0f
                               : 1.0f - powf(2.0f, -10.0f * progress);
              sizeCurve = alphaCurve;
              break;
            case EasingType::POP_IN_OUT:
              if (t > 0.9f)
                sizeCurve = (1.0f - t) / 0.1f; // pop de entrada
              else
                sizeCurve = t / 0.9f; // encoge hacia el final
              alphaCurve = (t > 0.8f) ? 1.0f : (t / 0.8f);
              break;
            case EasingType::EASE_OUT_ELASTIC: {
              float c4 = (2.0f * PI) / 3.0f;
              float elastic =
                  (progress == 0.0f)
                      ? 0.0f
                      : ((progress == 1.0f)
                             ? 1.0f
                             : (powf(2.0f, -10.0f * progress) *
                                    sinf((progress * 10.0f - 0.75f) * c4) +
                                1.0f));
              sizeCurve = 1.0f - elastic; // invertir para escalado final
              alphaCurve = t;
              break;
            }
            }

            Color currentCol = {
                (unsigned char)Lerp(p.endCol.r, p.startCol.r, t),
                (unsigned char)Lerp(p.endCol.g, p.startCol.g, t),
                (unsigned char)Lerp(p.endCol.b, p.startCol.b, t),
                (unsigned char)Lerp(p.endCol.a, p.startCol.a, alphaCurve)};

            float s = p.size;
            if (p.endSize >= 0.0f) {
              // Si endSize esta definido, interpola usando la curva matemática
              s = Lerp(p.endSize, p.size, sizeCurve);
            } else {
              // Comportamiento legado (multiplicador)
              s = p.size * sizeCurve;
            }

            if (p.type == RenderType::SPRITE && p.texture != nullptr) {
              Rectangle src = {0, 0, (float)p.texture->width,
                               (float)p.texture->height};
              Rectangle dest = {p.pos.x, p.pos.y, s * 2, s * 2};
              Vector2 origin = {s, s};
              DrawTexturePro(*p.texture, src, dest, origin, p.rotation,
                             currentCol);
            } else if (p.type == RenderType::RHOMB) {
              DrawPoly(p.pos, 4, s, p.rotation, currentCol);
            } else if (p.type == RenderType::SDF_CIRCLE ||
                       p.type == RenderType::SDF_STAR) {
              BeginShaderMode(localShader);
              int shapeType = (p.type == RenderType::SDF_CIRCLE)
                                  ? 0
                                  : 3; // 0 = Circle, 3 = Star en vfx_sdf.fs
              float softness = 0.15f;
              SetShaderValue(localShader, localLocShape, &shapeType,
                             SHADER_UNIFORM_INT);
              SetShaderValue(localShader, localLocSoft, &softness,
                             SHADER_UNIFORM_FLOAT);
              Rectangle src = {0, 0, (float)localTex.width,
                               (float)localTex.height};
              Rectangle dest = {p.pos.x, p.pos.y, s * 2.5f,
                                s * 2.5f}; // Un poco más grande para el glow
              Vector2 origin = {dest.width * 0.5f, dest.height * 0.5f};
              DrawTexturePro(localTex, src, dest, origin, p.rotation * RAD2DEG,
                             currentCol);
              EndShaderMode();
            } else if (p.type == RenderType::ISO_RING) {
              DrawEllipseLines((int)p.pos.x, (int)p.pos.y, s, s * 0.5f,
                               currentCol);
            } else if (p.type == RenderType::ISO_CIRCLE) {
              DrawEllipse((int)p.pos.x, (int)p.pos.y, s, s * 0.5f, currentCol);
            } else {
              // Default: Circle
              DrawCircleV(p.pos, s, currentCol);
            }
          },
          Graphics::RenderLayer::VFX, p.blendMode);
    }
  }

private:
  VFXSystem() = default;
  std::vector<Particle> particles;
  std::vector<Ghost> ghosts;

  // High-Def Shader state
  mutable Shader sdfShader = {0};
  mutable Texture2D blankTex = {0};
  mutable int locShapeType = 0;
  mutable int locSoftness = 0;
  mutable bool shaderLoaded = false;
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

  // 1. Chispas romboides aditivas (Anime style) con Easing y Turbulencia
  for (int i = 0; i < sparks; i++) {
    float a = baseAngle + ((float)GetRandomValue(-35, 35)) * DEG2RAD;
    float spd = (float)GetRandomValue(400, 800);
    vfx.SpawnPremium(origin, {cosf(a) * spd, sinf(a) * spd}, {0, -500}, 0.35f,
                     sparkColor, {sparkColor.r, sparkColor.g, sparkColor.b, 0},
                     (float)GetRandomValue(2, 5) * 1.5f, 0.0f,
                     RenderType::RHOMB, BLEND_ADDITIVE,
                     EasingType::EASE_OUT_EXPO, 0.85f, 150.0f,
                     (float)GetRandomValue(-300, 300));
  }

  // 2. Fragmentos / Escombros (Alpha blend) pesados
  for (int i = 0; i < fragments; i++) {
    float a = baseAngle + ((float)GetRandomValue(-120, 120)) * DEG2RAD;
    float spd = (float)GetRandomValue(150, 400);
    vfx.SpawnPremium(origin, {cosf(a) * spd, sinf(a) * spd}, {0, 1200}, 0.5f,
                     fragmentColor,
                     {fragmentColor.r, fragmentColor.g, fragmentColor.b, 0},
                     (float)GetRandomValue(4, 7), 0.0f, RenderType::CIRCLE,
                     BLEND_ALPHA, EasingType::EASE_OUT_QUAD, 0.94f, 0.0f, 0.0f);
  }

  // 3. Flash de impacto central con Sprite (si existe) o circulo aditivo
  if (ResourceManager::texVfxGlow.id != 0) {
    vfx.SpawnHybrid(origin, {0, 0}, 0.1f, WHITE, sparkColor, 20.0f,
                    RenderType::SPRITE, BLEND_ADDITIVE, 0, 0,
                    &ResourceManager::texVfxGlow);
  } else {
    vfx.SpawnParticleEx(origin, {0, 0}, 0.08f, WHITE, 18.0f, RenderType::CIRCLE,
                        BLEND_ADDITIVE);
  }
}

// Estela de dash (particulas cianeadas detras del jugador)
inline void SpawnDashTrail(Vector2 pos) {
  VFXSystem::GetInstance().SpawnParticleEx(pos, {0, 0}, 0.18f,
                                           {0, 220, 200, 255}, 5.0f);
}

// Lineas de velocidad (Streamers) - Para personajes muy rapidos
inline void SpawnSpeedStreamer(Vector2 pos, Vector2 vel) {
  VFXSystem::GetInstance().SpawnFull(
      pos, Vector2Scale(vel, -0.4f), 0.15f, {255, 255, 255, 180},
      {200, 240, 255, 0}, 1.5f, RenderType::RHOMB, BLEND_ADDITIVE, 0, 0.85f,
      atan2f(vel.y, vel.x) * RAD2DEG, 0, false);
}

// Sangre Estilizada (para Modo Garras, etc)
inline void SpawnStyledBlood(Vector2 pos, Vector2 dir) {
  auto &vfx = VFXSystem::GetInstance();
  int count = GetRandomValue(4, 8);
  for (int i = 0; i < count; i++) {
    float angle =
        atan2f(dir.y, dir.x) + (float)GetRandomValue(-40, 40) * DEG2RAD;
    float spd = (float)GetRandomValue(500, 1100);
    float life = 0.4f + (float)GetRandomValue(0, 30) * 0.01f;
    float size = (float)GetRandomValue(4, 8);

    vfx.SpawnPremium(pos, {cosf(angle) * spd, sinf(angle) * spd}, {0, 800},
                     life, // Gravedad incorporada en acc
                     {180, 0, 20, 255}, {40, 0, 10, 0}, size, 0.0f,
                     RenderType::RHOMB, BLEND_ALPHA, EasingType::EASE_OUT_QUAD,
                     0.90f, 250.0f, (float)GetRandomValue(-400, 400),
                     true // Ruido alto para salpicadura, true para bounce
    );
  }
}

// Onda de agua isómetrica (Ripple) - Para impactos
inline void SpawnWaterRipple(Vector2 pos, float maxRadius, Color col) {
  VFXSystem::GetInstance().SpawnFull(
      pos, {0, 0}, 0.5f, Fade(col, 0.6f), Fade(col, 0), maxRadius,
      RenderType::ISO_RING, BLEND_ALPHA, 0, 1.0f, 0, 0, false);
}

// Onda de Choque Kinética (Sonic Boom) - Para impactos pesados
inline void SpawnSonicBoom(Vector2 pos, float maxRadius) {
  VFXSystem::GetInstance().SpawnPremium(
      pos, {0, 0}, {0, 0}, 0.45f, Fade(WHITE, 0.7f), Fade(SKYBLUE, 0), 0.0f,
      maxRadius, RenderType::SDF_CIRCLE, BLEND_ADDITIVE,
      EasingType::EASE_OUT_EXPO, 1.0f, 0.0f, 0.0f);
  // Anillo exterior con rebote elastico
  VFXSystem::GetInstance().SpawnPremium(
      pos, {0, 0}, {0, 0}, 0.55f, Fade(Color{0, 255, 255, 255}, 0.3f),
      Fade(BLUE, 0), maxRadius * 0.8f, maxRadius * 1.2f, RenderType::SDF_CIRCLE,
      BLEND_ADDITIVE, EasingType::EASE_OUT_ELASTIC, 1.0f, 0.0f, 0.0f);
}

// Flash de impacto (en pantalla completa o local)
inline void SpawnHitFlash(Vector2 pos, float radius, Color col) {
  VFXSystem::GetInstance().SpawnParticleEx(pos, {0, 0}, 0.12f, col, radius);
}

} // namespace Graphics
