#pragma once
#include "RenderManager.h"
#include "../ResourceManager.h"
#include <algorithm>
#include <raylib.h>
#include <raymath.h>
#include <vector>

namespace Graphics {

enum class RenderType { CIRCLE, RHOMB, GLOW_LINE, SPRITE };

// ─── SpriteAnimOverlay: Spritesheet animado en el mundo ──────────────
struct SpriteAnimOverlay {
  Texture2D* sheet   = nullptr;
  int   frameCount   = 1;     // columnas del spritesheet
  int   frameRows    = 1;     // filas (para sprites direccionales)
  int   row          = 0;     // fila a usar
  float frameDur     = 0.07f;
  float timer        = 0.0f;
  int   currentFrame = 0;
  Vector2 pos;
  float rotation     = 0.0f;  // grados — orientado al facing
  float scale        = 1.0f;
  Color tint         = WHITE;
  bool  loop         = false;
  bool  additive     = false;
  float life         = 0.0f;  // tiempo total restante
  float maxLife      = 1.0f;
};

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

  // New Physics & Juice
  float gravity = 0.0f;
  float friction = 0.92f;
  bool  useBounce = false;
  float bounciness = 0.6f;
  float sizeMultiplier = 1.0f; // controlled by time
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

  // Spawn completo para efectos "Juicy"
  void SpawnFull(Vector2 pos, Vector2 vel, float life, Color startCol, Color endCol,
                 float size, RenderType type, BlendMode blend, 
                 float gravity = 0, float friction = 0.92f, 
                 float rotation = 0, float rotVel = 0, bool bounce = false) {
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

  void Clear() { particles.clear(); ghosts.clear(); overlays.clear(); }

  // ─── SPRITE ANIM OVERLAYS ─────────────────────────────────────────
  void SpawnOverlay(SpriteAnimOverlay o) {
    if (o.sheet == nullptr || o.sheet->id == 0) return;
    o.life = o.maxLife;
    o.currentFrame = 0;
    o.timer = 0.0f;
    overlays.push_back(o);
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

  void SpawnGhost(Vector2 pos, Rectangle src, float life, Color col, bool flipX, float scale, Vector2 origin, Texture2D tex) {
    ghosts.push_back({pos, src, life, life, col, flipX, scale, origin, tex});
  }

  void Update(float dt) {
    for (auto &p : particles) {
      p.vel.y += p.gravity * dt;
      p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt));
      p.vel = Vector2Scale(p.vel, p.friction);
      if (p.useBounce && p.pos.y > 2800.0f) {
          p.pos.y = 2800.0f;
          p.vel.y *= -p.bounciness;
          p.vel.x *= 0.8f;
      }
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

    // Actualizar SpriteAnimOverlays
    for (auto &o : overlays) {
      o.life -= dt;
      o.timer += dt;
      if (o.timer >= o.frameDur) {
        o.timer = 0.0f;
        o.currentFrame++;
        if (o.currentFrame >= o.frameCount) {
          if (o.loop) o.currentFrame = 0;
          else        o.currentFrame = o.frameCount - 1;
        }
      }
    }
    overlays.erase(
        std::remove_if(overlays.begin(), overlays.end(),
                       [](const SpriteAnimOverlay &o) { return o.life <= 0; }),
        overlays.end());
  }

  void SubmitDraws() const {
    // 1. Dibujar Ghost Trails primero
    for (const auto &g : ghosts) {
      Graphics::RenderManager::GetInstance().Submit(
          g.pos.y - 1.0f,
          [g]() {
            float t = g.life / g.maxLife;
            Color c = Fade(g.color, t * 0.52f);
            Rectangle dest = { g.pos.x, g.pos.y, g.src.width * g.scale, g.src.height * g.scale };
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
          [p]() {
            float t = p.life / p.maxLife; 
            
            // Curva de Alpha: inicia fuerte, desvanece suave
            float alphaCurve = (t > 0.8f) ? 1.0f : (t / 0.8f);
            alphaCurve = alphaCurve * alphaCurve;
            
            // Curva de Tamaño: pop-in sutil + encogimiento
            float sizeCurve = 1.0f;
            if (t > 0.9f) sizeCurve = (1.0f - t) / 0.1f; // pop de entrada
            else sizeCurve = t / 0.9f; // encoge hacia el final
            
            Color currentCol = {
              (unsigned char)Lerp(p.endCol.r, p.startCol.r, t),
              (unsigned char)Lerp(p.endCol.g, p.startCol.g, t),
              (unsigned char)Lerp(p.endCol.b, p.startCol.b, t),
              (unsigned char)Lerp(p.endCol.a, p.startCol.a, alphaCurve)
            };

            float s = p.size * sizeCurve;

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

    // 3. SpriteAnimOverlays
    for (const auto &o : overlays) {
      Graphics::RenderManager::GetInstance().Submit(
          o.pos.y,
          [o]() {
            if (o.sheet == nullptr || o.sheet->id == 0) return;
            float t = o.maxLife > 0 ? (o.life / o.maxLife) : 1.0f;
            float alpha = (t < 0.3f) ? (t / 0.3f) : 1.0f;
            Color tinted = Fade(o.tint, alpha);
            float fw = (float)o.sheet->width  / o.frameCount;
            float fh = (float)o.sheet->height / o.frameRows;
            Rectangle src = { (float)o.currentFrame * fw, (float)o.row * fh, fw, fh };
            float w = fw * o.scale;
            float h = fh * o.scale;
            Rectangle dest = { o.pos.x, o.pos.y, w, h };
            Vector2 origin = { w * 0.5f, h * 0.5f };
            if (o.additive) BeginBlendMode(BLEND_ADDITIVE);
            DrawTexturePro(*o.sheet, src, dest, origin, o.rotation, tinted);
            if (o.additive) EndBlendMode();
          },
          Graphics::RenderLayer::VFX);
    }
  }

private:
  VFXSystem() = default;
  std::vector<Particle> particles;
  std::vector<Ghost> ghosts;
  std::vector<SpriteAnimOverlay> overlays;
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

// Lineas de velocidad (Streamers) - Para personajes muy rapidos
inline void SpawnSpeedStreamer(Vector2 pos, Vector2 vel) {
    VFXSystem::GetInstance().SpawnFull(
        pos, Vector2Scale(vel, -0.4f), 0.15f, 
        {255, 255, 255, 180}, {200, 240, 255, 0}, 
        1.5f, RenderType::RHOMB, BLEND_ADDITIVE, 
        0, 0.85f, atan2f(vel.y, vel.x) * RAD2DEG, 0, false
    );
}

// Sangre Estilizada (para Modo Garras, etc)
inline void SpawnStyledBlood(Vector2 pos, Vector2 dir) {
    auto &vfx = VFXSystem::GetInstance();
    int count = GetRandomValue(3, 6);
    for (int i = 0; i < count; i++) {
        float angle = atan2f(dir.y, dir.x) + (float)GetRandomValue(-40, 40) * DEG2RAD;
        float spd = (float)GetRandomValue(400, 900);
        vfx.SpawnFull(
            pos, {cosf(angle) * spd, sinf(angle) * spd},
            0.4f + (float)GetRandomValue(0, 20) * 0.01f,
            {160, 0, 20, 255}, {60, 0, 10, 0}, 
            (float)GetRandomValue(3, 6), RenderType::RHOMB, BLEND_ALPHA,
            400.0f, 0.94f, (float)GetRandomValue(0, 360), (float)GetRandomValue(-200, 200), true
        );
    }
}

// Onda de agua (Ripple) - Para impactos de agua o efectos de frio
inline void SpawnWaterRipple(Vector2 pos, float maxRadius, Color col) {
    VFXSystem::GetInstance().SpawnFull(
        pos, {0,0}, 0.5f, Fade(col, 0.6f), Fade(col, 0),
        maxRadius, RenderType::CIRCLE, BLEND_ALPHA,
        0, 1.0f, 0, 0, false
    );
}

// Onda de Choque Kinética (Sonic Boom) - Para impactos pesados
inline void SpawnSonicBoom(Vector2 pos, float maxRadius) {
    VFXSystem::GetInstance().SpawnFull(
        pos, {0,0}, 0.35f, Fade(WHITE, 0.5f), Fade(SKYBLUE, 0),
        maxRadius, RenderType::CIRCLE, BLEND_ADDITIVE,
        0, 1.0f, 0, 0, false
    );
}

// Flash de impacto (en pantalla completa o local)
inline void SpawnHitFlash(Vector2 pos, float radius, Color col) {
    VFXSystem::GetInstance().SpawnParticleEx(pos, {0, 0}, 0.12f, col, radius);
}

// ─── NUEVOS HELPERS CON SPRITESHEET VFX ──────────────────────────────

// Tajo básico de la Ropera (weapon hit + chispas)
// comboStep: 0=Estocada, 1=Tajo lateral, 2=Gran Estocada
inline void SpawnSlashVFX(Vector2 pos, Vector2 facing, int comboStep) {
    auto &vfx = VFXSystem::GetInstance();
    float angle = atan2f(facing.y, facing.x) * RAD2DEG;

    // Overlay spritesheet: 10_weaponhit_spritesheet (chispa metálica)
    if (ResourceManager::vfxWeaponHit.id != 0) {
        SpriteAnimOverlay o;
        o.sheet      = &ResourceManager::vfxWeaponHit;
        o.frameCount = 8;
        o.frameRows  = 1;
        o.frameDur   = 0.04f;
        o.pos        = Vector2Add(pos, Vector2Scale(facing, comboStep == 2 ? 60.0f : 40.0f));
        o.rotation   = angle;
        o.scale      = (comboStep == 2) ? 1.8f : 1.2f;
        o.tint       = (comboStep == 2) ? Color{255, 220, 120, 255} : WHITE;
        o.loop       = false;
        o.additive   = true;
        o.maxLife    = 0.32f;
        vfx.SpawnOverlay(o);
    }

    // Overlay slash 48x48 (tajo visual grande en la posición del boss)
    if (ResourceManager::vfxSlashLight.id != 0) {
        SpriteAnimOverlay s;
        s.sheet      = &ResourceManager::vfxSlashLight;
        s.frameCount = 12;
        s.frameRows  = 1;
        s.frameDur   = 0.035f;
        s.pos        = Vector2Add(pos, Vector2Scale(facing, 50.0f));
        s.rotation   = angle + (comboStep == 1 ? 90.0f : 0.0f);
        s.scale      = (comboStep == 2) ? 2.2f : 1.5f;
        s.tint       = (comboStep == 1) ? Color{180, 255, 255, 230} : Color{255, 255, 200, 230};
        s.loop       = false;
        s.additive   = true;
        s.maxLife    = 0.42f;
        vfx.SpawnOverlay(s);
    }

    // Chispas de respaldo (siempre)
    Color sc = (comboStep == 2) ? Color{255, 200, 80, 255} : Color{200, 255, 255, 255};
    for (int i = 0; i < 6; i++) {
        float a = atan2f(facing.y, facing.x) + (float)GetRandomValue(-30, 30) * DEG2RAD;
        float spd = (float)GetRandomValue(200, 500);
        vfx.SpawnParticleEx(Vector2Add(pos, Vector2Scale(facing, 40.0f)),
            {cosf(a) * spd, sinf(a) * spd}, 0.18f, sc, 3.5f, RenderType::RHOMB, BLEND_ADDITIVE);
    }
}

// Tajo pesado y Q de la Ropera (slash grande 128x128)
inline void SpawnHeavySlashVFX(Vector2 pos, Vector2 facing, bool isUlt) {
    auto &vfx = VFXSystem::GetInstance();
    float angle = atan2f(facing.y, facing.x) * RAD2DEG;

    if (ResourceManager::vfxSlashHeavy.id != 0) {
        SpriteAnimOverlay o;
        o.sheet      = &ResourceManager::vfxSlashHeavy;
        o.frameCount = 4;
        o.frameRows  = 1;
        o.frameDur   = 0.05f;
        o.pos        = Vector2Add(pos, Vector2Scale(facing, 70.0f));
        o.rotation   = angle;
        o.scale      = isUlt ? 3.0f : 2.0f;
        o.tint       = isUlt ? Color{255, 160, 40, 255} : Color{100, 255, 220, 255};
        o.loop       = false;
        o.additive   = true;
        o.maxLife    = 0.22f;
        vfx.SpawnOverlay(o);
    }

    // Weapon hit spark encima
    if (ResourceManager::vfxWeaponHit.id != 0) {
        SpriteAnimOverlay w;
        w.sheet      = &ResourceManager::vfxWeaponHit;
        w.frameCount = 8;
        w.frameRows  = 1;
        w.frameDur   = 0.03f;
        w.pos        = Vector2Add(pos, Vector2Scale(facing, 80.0f));
        w.rotation   = angle + 45.0f;
        w.scale      = isUlt ? 2.5f : 1.8f;
        w.tint       = isUlt ? Color{255, 200, 80, 255} : WHITE;
        w.loop       = false;
        w.additive   = true;
        w.maxLife    = 0.28f;
        vfx.SpawnOverlay(w);
    }
}

// Perfect Dodge Counter (Holy VFX — 3 fases sobre el jugador)
inline void SpawnHolyCounterVFX(Vector2 playerPos) {
    auto &vfx = VFXSystem::GetInstance();

    if (ResourceManager::vfxHolyInitial.id != 0) {
        SpriteAnimOverlay o;
        o.sheet      = &ResourceManager::vfxHolyInitial;
        o.frameCount = 4;
        o.frameRows  = 1;
        o.frameDur   = 0.06f;
        o.pos        = {playerPos.x, playerPos.y - 30.0f};
        o.rotation   = 0.0f;
        o.scale      = 3.0f;
        o.tint       = {255, 240, 180, 255};
        o.loop       = false;
        o.additive   = true;
        o.maxLife    = 0.25f;
        vfx.SpawnOverlay(o);
    }

    if (ResourceManager::vfxHolyLoop.id != 0) {
        SpriteAnimOverlay l;
        l.sheet      = &ResourceManager::vfxHolyLoop;
        l.frameCount = 6;
        l.frameRows  = 1;
        l.frameDur   = 0.07f;
        l.pos        = {playerPos.x, playerPos.y - 30.0f};
        l.rotation   = 0.0f;
        l.scale      = 3.5f;
        l.tint       = {255, 255, 200, 220};
        l.loop       = true;
        l.additive   = true;
        l.maxLife    = 0.55f;
        vfx.SpawnOverlay(l);
    }

    // Chispas doradas de realce
    for (int i = 0; i < 12; i++) {
        float a = (float)i * (360.0f / 12.0f) * DEG2RAD;
        float spd = (float)GetRandomValue(150, 350);
        vfx.SpawnFull(
            {playerPos.x, playerPos.y - 20.0f},
            {cosf(a) * spd, sinf(a) * spd},
            0.5f, {255, 240, 100, 255}, {255, 200, 50, 0},
            4.5f, RenderType::RHOMB, BLEND_ADDITIVE,
            0, 0.90f, (float)GetRandomValue(0, 360), 200.0f, false
        );
    }
}

// Impacto del counter en el boss (Holy Impact)
inline void SpawnHolyImpactVFX(Vector2 bossPos) {
    auto &vfx = VFXSystem::GetInstance();

    if (ResourceManager::vfxHolyImpact.id != 0) {
        SpriteAnimOverlay o;
        o.sheet      = &ResourceManager::vfxHolyImpact;
        o.frameCount = 5;
        o.frameRows  = 1;
        o.frameDur   = 0.05f;
        o.pos        = bossPos;
        o.rotation   = (float)GetRandomValue(0, 360);
        o.scale      = 4.0f;
        o.tint       = {255, 255, 220, 255};
        o.loop       = false;
        o.additive   = true;
        o.maxLife    = 0.28f;
        vfx.SpawnOverlay(o);
    }

    // Onda de choque dorada
    SpawnSonicBoom(bossPos, 200.0f);
    for (int i = 0; i < 8; i++) {
        float a = (float)i * 45.0f * DEG2RAD;
        vfx.SpawnParticleEx(bossPos, {cosf(a) * 500.0f, sinf(a) * 500.0f},
            0.3f, {255, 230, 100, 255}, 5.0f, RenderType::RHOMB, BLEND_ADDITIVE);
    }
}

// Impacto mágico (Mago: proyectiles, magic8, etc.)
inline void SpawnMagicHitVFX(Vector2 pos, Color tint) {
    auto &vfx = VFXSystem::GetInstance();

    if (ResourceManager::vfxMagicHit.id != 0) {
        SpriteAnimOverlay o;
        o.sheet      = &ResourceManager::vfxMagicHit;
        o.frameCount = 8;
        o.frameRows  = 1;
        o.frameDur   = 0.04f;
        o.pos        = pos;
        o.rotation   = (float)GetRandomValue(0, 360);
        o.scale      = 2.0f;
        o.tint       = tint;
        o.loop       = false;
        o.additive   = true;
        o.maxLife    = 0.35f;
        vfx.SpawnOverlay(o);
    }

    // Partículas mágicas de apoyo
    for (int i = 0; i < 8; i++) {
        float a = (float)GetRandomValue(0, 360) * DEG2RAD;
        float spd = (float)GetRandomValue(200, 500);
        vfx.SpawnParticleEx(pos, {cosf(a)*spd, sinf(a)*spd},
            0.25f, tint, 3.5f, RenderType::RHOMB, BLEND_ADDITIVE);
    }
}

// Tornado / Vortex del Mago (loop durante el tornado)
inline void SpawnVortexParticle(Vector2 pos) {
    auto &vfx = VFXSystem::GetInstance();

    if (ResourceManager::vfxVortex.id != 0) {
        SpriteAnimOverlay o;
        o.sheet      = &ResourceManager::vfxVortex;
        o.frameCount = 8;
        o.frameRows  = 1;
        o.frameDur   = 0.06f;
        o.pos        = pos;
        o.rotation   = (float)GetRandomValue(0, 360);
        o.scale      = 3.5f;
        o.tint       = {100, 200, 255, 200};
        o.loop       = true;
        o.additive   = true;
        o.maxLife    = 0.5f;
        vfx.SpawnOverlay(o);
    }
}

// Congelación del Mago (hit de hielo sobre el boss)
inline void SpawnFreezeVFX(Vector2 pos) {
    auto &vfx = VFXSystem::GetInstance();

    if (ResourceManager::vfxFreezing.id != 0) {
        SpriteAnimOverlay o;
        o.sheet      = &ResourceManager::vfxFreezing;
        o.frameCount = 8;
        o.frameRows  = 1;
        o.frameDur   = 0.06f;
        o.pos        = pos;
        o.rotation   = (float)GetRandomValue(-20, 20);
        o.scale      = 4.5f;
        o.tint       = {150, 230, 255, 220};
        o.loop       = false;
        o.additive   = false;
        o.maxLife    = 0.55f;
        vfx.SpawnOverlay(o);
    }

    // Cristales de hielo de apoyo
    for (int i = 0; i < 8; i++) {
        float a = (float)i * 45.0f * DEG2RAD;
        vfx.SpawnFull(pos,
            {cosf(a) * 220.0f, sinf(a) * 220.0f}, 0.6f,
            {200, 240, 255, 255}, {100, 180, 255, 0},
            5.5f, RenderType::RHOMB, BLEND_ADDITIVE, 0, 0.90f,
            (float)GetRandomValue(0, 360), 150.0f, false);
    }

    SpawnWaterRipple(pos, 120.0f, {150, 220, 255, 255});
}

// Q Fantasma del Segador (phantom slash spritesheet)
inline void SpawnPhantomSlashVFX(Vector2 pos, Vector2 facing, int slashIdx) {
    auto &vfx = VFXSystem::GetInstance();
    float angle = atan2f(facing.y, facing.x) * RAD2DEG + (slashIdx == 0 ? -45.0f : 45.0f);

    if (ResourceManager::vfxPhantom.id != 0) {
        SpriteAnimOverlay o;
        o.sheet      = &ResourceManager::vfxPhantom;
        o.frameCount = 8;
        o.frameRows  = 1;
        o.frameDur   = 0.05f;
        o.pos        = Vector2Add(pos, Vector2Scale(facing, 80.0f));
        o.rotation   = angle;
        o.scale      = 2.5f;
        o.tint       = {120, 0, 200, 220};
        o.loop       = false;
        o.additive   = true;
        o.maxLife    = 0.42f;
        vfx.SpawnOverlay(o);
    }

    // Niebla espectral
    for (int i = 0; i < 6; i++) {
        float a = angle * DEG2RAD + (float)GetRandomValue(-40, 40) * DEG2RAD;
        float spd = (float)GetRandomValue(150, 350);
        vfx.SpawnFull(
            Vector2Add(pos, Vector2Scale(facing, 60.0f)),
            {cosf(a) * spd, sinf(a) * spd},
            0.4f, {80, 0, 140, 200}, {0, 0, 0, 0},
            8.0f, RenderType::RHOMB, BLEND_ADDITIVE,
            0, 0.88f, (float)GetRandomValue(0, 360), 300.0f, false
        );
    }
}

// Aura de nebulosa del Segador (durante la Ultimate)
inline void SpawnNebulaAura(Vector2 pos) {
    auto &vfx = VFXSystem::GetInstance();

    if (ResourceManager::vfxNebula.id != 0) {
        SpriteAnimOverlay o;
        o.sheet      = &ResourceManager::vfxNebula;
        o.frameCount = 8;
        o.frameRows  = 1;
        o.frameDur   = 0.08f;
        o.pos        = {pos.x, pos.y - 20.0f};
        o.rotation   = (float)GetRandomValue(-15, 15);
        o.scale      = 4.0f;
        o.tint       = {60, 0, 120, 180};
        o.loop       = true;
        o.additive   = true;
        o.maxLife    = 0.6f;
        vfx.SpawnOverlay(o);
    }
}

// Hechizo maligno del Boss Ether Corrupto (Felspell)
inline void SpawnFelspellBurst(Vector2 pos) {
    auto &vfx = VFXSystem::GetInstance();

    if (ResourceManager::vfxFelspell.id != 0) {
        SpriteAnimOverlay o;
        o.sheet      = &ResourceManager::vfxFelspell;
        o.frameCount = 8;
        o.frameRows  = 1;
        o.frameDur   = 0.07f;
        o.pos        = pos;
        o.rotation   = (float)GetRandomValue(0, 360);
        o.scale      = 5.0f;
        o.tint       = {200, 0, 80, 220};
        o.loop       = false;
        o.additive   = true;
        o.maxLife    = 0.6f;
        vfx.SpawnOverlay(o);
    }

    // Partículas oscuras de apoyo
    for (int i = 0; i < 10; i++) {
        float a = (float)GetRandomValue(0, 360) * DEG2RAD;
        float spd = (float)GetRandomValue(250, 600);
        vfx.SpawnFull(pos, {cosf(a)*spd, sinf(a)*spd},
            0.5f, {180, 0, 60, 255}, {40, 0, 10, 0},
            6.0f, RenderType::RHOMB, BLEND_ADDITIVE,
            0, 0.92f, (float)GetRandomValue(0, 360), 250.0f, false);
    }
}

} // namespace Graphics

