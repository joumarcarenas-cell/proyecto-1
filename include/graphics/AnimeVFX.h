#pragma once
// =====================================================================
// AnimeVFX.h  -  Sistema Visual "Estilo Anime" para la arena
// =====================================================================
// Auto-contenido: solo necesita <raylib.h>, <raymath.h> y <rlgl.h>.
// Para usarlo, incluir este header en GameplayScene.cpp.
//
// SISTEMAS QUE CONTIENE:
//   1. PostProcessPipeline  - Shader Bloom + Aberracion + Ripple + Flash
//   2. AnimeTrailSystem     - Weapon Trails geometricos (ribbon strips)
//   3. SpeedLineSystem      - Lineas de velocidad durante dashes
//   4. AmbientParticleSystem- Polvo y motas de aire en el escenario
// =====================================================================
#pragma once
#include <raylib.h>
#include <raymath.h>
#include "rlgl.h"
#include "VFXSystem.h"
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>

namespace AnimeVFX {

// =============================================================
// 1. POST-PROCESS PIPELINE
// =============================================================
struct Ripple {
    Vector2 worldPos;  // posicion en espacio de mundo
    float   age;       // 0=inicio, 1=final
    float   maxAge;    // duracion total (s)
    bool    active = false;
};

class PostProcessPipeline {
public:
    static PostProcessPipeline& Get() {
        static PostProcessPipeline inst;
        return inst;
    }

    void Init(int w, int h) {
        m_target = LoadRenderTexture(w, h);
        m_shader = LoadShader(nullptr, "assets/shaders/anime_vfx.fs");

        // Localizar uniforms
        m_locResolution     = GetShaderLocation(m_shader, "resolution");
        m_locTime           = GetShaderLocation(m_shader, "time");
        m_locShake          = GetShaderLocation(m_shader, "shakeIntensity");
        m_locHitstop        = GetShaderLocation(m_shader, "hitstop");
        m_locFlashAlpha     = GetShaderLocation(m_shader, "flashAlpha");
        m_locRipplePos      = GetShaderLocation(m_shader, "ripplePos");
        m_locRippleAge      = GetShaderLocation(m_shader, "rippleAge");
        m_locRippleCount    = GetShaderLocation(m_shader, "rippleCount");

        m_locExposure       = GetShaderLocation(m_shader, "exposure");
        m_locSaturation     = GetShaderLocation(m_shader, "saturation");
        m_locContrast       = GetShaderLocation(m_shader, "contrast");
        m_locGrain          = GetShaderLocation(m_shader, "grainIntensity");

        m_locImpactType     = GetShaderLocation(m_shader, "impactType");
        m_locGlitch         = GetShaderLocation(m_shader, "glitchIntensity");

        float res[2] = { (float)w, (float)h };
        SetShaderValue(m_shader, m_locResolution, res, SHADER_UNIFORM_VEC2);
    }

    void Unload() {
        if (m_target.id != 0) UnloadRenderTexture(m_target);
        if (m_shader.id != 0) UnloadShader(m_shader);
    }

    bool IsReady() const { return m_shader.id != 0 && m_target.id != 0; }

    RenderTexture2D& GetTarget() { return m_target; }

    // Activar un shockwave desde posicion de mundo
    void SpawnRipple(Vector2 worldPos, float duration = 0.55f) {
        for (auto& r : m_ripples) {
            if (!r.active) {
                r = { worldPos, 0.0f, duration, true };
                return;
            }
        }
    }

    // Activar flash de pantalla (blanco sutil)
    void SpawnFlash(float duration = 0.05f) {
        m_flashTimer = duration;
    }

    // Activar Impact Frame (0=off, 1=inverted, 2=grayscale)
    void SpawnImpactFrame(int type, float duration = 0.06f) {
        m_impactType = type;
        m_impactTimer = duration;
    }

    // Activar Glitch visual (scanlines)
    void SpawnGlitch(float duration = 0.15f) {
        m_glitchTimer = duration;
    }

    // Setters para Color Grading dinámico
    void SetExposure(float v)   { m_exposure = v; }
    void SetSaturation(float v) { m_saturation = v; }
    void SetContrast(float v)   { m_contrast = v; }
    void SetGrain(float v)      { m_grainIntensity = v; }

    float GetExposure() const   { return m_exposure; }
    float GetSaturation() const { return m_saturation; }
    float GetContrast() const   { return m_contrast; }

    void Update(float dt, float shakeIntensity, float hitstopTimer) {
        m_time          += dt;
        m_shakeIntensity = shakeIntensity;
        m_hitstopTimer   = hitstopTimer;

        if (m_flashTimer > 0) m_flashTimer -= dt;
        if (m_impactTimer > 0) {
            m_impactTimer -= dt;
            if (m_impactTimer <= 0) m_impactType = 0;
        }
        if (m_glitchTimer > 0) m_glitchTimer -= dt;

        for (auto& r : m_ripples) {
            if (r.active) {
                r.age += dt / r.maxAge;
                if (r.age >= 1.0f) r.active = false;
            }
        }
    }

    // Llamar FUERA del BeginMode2D, despues de dibujar el mundo en el buffer
    void DrawToScreen(Camera2D cam) {
        if (!IsReady()) {
            // Fallback: dibujar directamente sin shader
            DrawTextureRec(
                m_target.texture,
                { 0, 0, (float)m_target.texture.width, -(float)m_target.texture.height },
                { 0, 0 }, WHITE);
            return;
        }

        // Empaquetar ripples
        float posArray[8]   = {};  // hasta 4 ripples * 2 floats
        float ageArray[4]   = {};
        int   activeCount   = 0;

        for (const auto& r : m_ripples) {
            if (!r.active || activeCount >= 4) continue;
            // Convertir worldPos a UV (0..1)
            Vector2 screen = GetWorldToScreen2D(r.worldPos, cam);
            posArray[activeCount * 2 + 0] = screen.x / (float)m_target.texture.width;
            posArray[activeCount * 2 + 1] = 1.0f - screen.y / (float)m_target.texture.height;
            ageArray[activeCount] = r.age;
            activeCount++;
        }

        // Subir uniforms
        SetShaderValue(m_shader, m_locTime,          &m_time,          SHADER_UNIFORM_FLOAT);
        SetShaderValue(m_shader, m_locShake,         &m_shakeIntensity,SHADER_UNIFORM_FLOAT);
        SetShaderValue(m_shader, m_locHitstop,       &m_hitstopTimer,  SHADER_UNIFORM_FLOAT);

        float fa = (m_flashTimer > 0) ? (m_flashTimer / 0.05f) * 0.85f : 0.0f;
        SetShaderValue(m_shader, m_locFlashAlpha,    &fa,              SHADER_UNIFORM_FLOAT);
        SetShaderValueV(m_shader, m_locRipplePos,    posArray, SHADER_UNIFORM_VEC2, 4);
        SetShaderValueV(m_shader, m_locRippleAge,    ageArray, SHADER_UNIFORM_FLOAT, 4);
        SetShaderValue(m_shader, m_locRippleCount,   &activeCount,     SHADER_UNIFORM_INT);

        // Impact & Glitch
        float glitchVal = (m_glitchTimer > 0) ? (m_glitchTimer / 0.15f) : 0.0f;
        SetShaderValue(m_shader, m_locImpactType,    &m_impactType,    SHADER_UNIFORM_INT);
        SetShaderValue(m_shader, m_locGlitch,        &glitchVal,       SHADER_UNIFORM_FLOAT);

        // Color Grading
        SetShaderValue(m_shader, m_locExposure,      &m_exposure,      SHADER_UNIFORM_FLOAT);
        SetShaderValue(m_shader, m_locSaturation,    &m_saturation,    SHADER_UNIFORM_FLOAT);
        SetShaderValue(m_shader, m_locContrast,      &m_contrast,      SHADER_UNIFORM_FLOAT);
        SetShaderValue(m_shader, m_locGrain,         &m_grainIntensity,SHADER_UNIFORM_FLOAT);

        BeginShaderMode(m_shader);
        DrawTextureRec(
            m_target.texture,
            { 0, 0, (float)m_target.texture.width, -(float)m_target.texture.height },
            { 0, 0 }, WHITE);
        EndShaderMode();
    }

private:
    RenderTexture2D m_target  = {};
    Shader          m_shader  = {};
    int m_locResolution = -1, m_locTime = -1, m_locShake = -1;
    int m_locHitstop = -1, m_locFlashAlpha = -1;
    int m_locRipplePos = -1, m_locRippleAge = -1, m_locRippleCount = -1;
    int m_locExposure = -1, m_locSaturation = -1, m_locContrast = -1, m_locGrain = -1;
    int m_locImpactType = -1, m_locGlitch = -1;

    std::array<Ripple, 4> m_ripples = {};
    float m_time          = 0;
    float m_shakeIntensity= 0;
    float m_hitstopTimer  = 0;
    float m_flashTimer    = 0;
    float m_impactTimer   = 0;
    float m_glitchTimer   = 0;
    int   m_impactType    = 0;

    // Default Grading Values (Natural with slight pop)
    float m_exposure       = 1.0f;
    float m_saturation     = 1.08f; // Reduced (was 1.15)
    float m_contrast       = 1.06f; // Reduced (was 1.12)
    float m_grainIntensity = 0.012f; // Reduced (was 0.025)
};

// =============================================================
// 2. ANIME TRAIL SYSTEM (Weapon Ribbons)
// =============================================================
struct TrailPoint {
    Vector2 base;
    Vector2 tip;
    float   alpha;  // 1=fresco, 0=muerto
};

struct AnimeTrail {
    static constexpr int MAX_POINTS = 32;
    TrailPoint points[MAX_POINTS];
    int        count   = 0;
    Color      color   = WHITE;
    float      width   = 28.0f;  // ancho en base de la estela
    bool       active  = false;

    void Push(Vector2 base, Vector2 tip) {
        // Desplaza el historial y agrega al frente
        for (int i = MAX_POINTS - 1; i > 0; i--)
            points[i] = points[i - 1];
        points[0] = { base, tip, 1.0f };
        if (count < MAX_POINTS) count++;

        // Decaer alphas
        for (int i = 1; i < count; i++)
            points[i].alpha = (float)(count - i) / (float)count;
    }

    void Clear() { count = 0; active = false; }

    void Draw() const {
        if (count < 2) return;

        rlSetTexture(0);
        rlBegin(RL_TRIANGLES);

        for (int i = 0; i < count - 1; i++) {
            const TrailPoint& a = points[i];
            const TrailPoint& b = points[i + 1];

            float ta = a.alpha;
            float tb = b.alpha;

            // Tapeado: la cola se estrecha (estilo "afilado")
            float scaleA = ta;
            float scaleB = tb;

            Vector2 midA_base = Vector2Lerp(a.tip, a.base, 1.0f - scaleA);
            Vector2 midB_base = Vector2Lerp(b.tip, b.base, 1.0f - scaleB);

            // Primer triangulo
            rlColor4ub(color.r, color.g, color.b, (unsigned char)(ta * 200));
            rlVertex2f(a.tip.x, a.tip.y);
            rlColor4ub(color.r, color.g, color.b, (unsigned char)(ta * 80));
            rlVertex2f(midA_base.x, midA_base.y);
            rlColor4ub(color.r, color.g, color.b, (unsigned char)(tb * 80));
            rlVertex2f(midB_base.x, midB_base.y);

            // Segundo triangulo
            rlColor4ub(color.r, color.g, color.b, (unsigned char)(ta * 200));
            rlVertex2f(a.tip.x, a.tip.y);
            rlColor4ub(color.r, color.g, color.b, (unsigned char)(tb * 80));
            rlVertex2f(midB_base.x, midB_base.y);
            rlColor4ub(color.r, color.g, color.b, (unsigned char)(tb * 200));
            rlVertex2f(b.tip.x, b.tip.y);
        }

        rlEnd();
    }
};

class AnimeTrailSystem {
public:
    static AnimeTrailSystem& Get() {
        static AnimeTrailSystem inst;
        return inst;
    }

    // Registrar o reusar una estela por ID de entidad
    int Register(Color color, float width = 28.0f) {
        int slot = FindFree();
        if (slot < 0) return -1;
        m_trails[slot].Clear();
        m_trails[slot].color  = color;
        m_trails[slot].width  = width;
        m_trails[slot].active = true;
        return slot;
    }

    void Push(int id, Vector2 base, Vector2 tip) {
        if (id < 0 || id >= MAX_TRAILS) return;
        m_trails[id].Push(base, tip);
    }

    void Clear(int id) {
        if (id >= 0 && id < MAX_TRAILS) m_trails[id].Clear();
    }

    void ClearAll() {
        for (auto& t : m_trails) t.Clear();
    }

    void Draw() const {
        for (const auto& t : m_trails)
            if (t.active && t.count > 0)
                t.Draw();
    }

private:
    static constexpr int MAX_TRAILS = 8;
    std::array<AnimeTrail, MAX_TRAILS> m_trails;

    int FindFree() {
        for (int i = 0; i < MAX_TRAILS; i++)
            if (!m_trails[i].active) return i;
        return 0; // reusar el primero si no hay espacio
    }
};

// =============================================================
// 3. SPEED LINE SYSTEM (dash circles)
// =============================================================
struct SpeedLine {
    float angle;     // angulo en radianes
    float len;       // longitud
    float speed;     // velocidad angular
    float alpha;
    float life;
};

class SpeedLineSystem {
public:
    static SpeedLineSystem& Get() {
        static SpeedLineSystem inst;
        return inst;
    }

    // Disparar un burst de lineas (llamar al hacer dash)
    void Burst(Vector2 origin, Color color = WHITE, int count = 22) {
        m_origin = origin;
        m_color  = color;
        m_timer  = 0.22f;

        for (int i = 0; i < count; i++) {
            SpeedLine sl;
            sl.angle = ((float)i / count) * PI * 2.0f;
            sl.len   = (float)GetRandomValue(60, 160);
            sl.speed = ((float)GetRandomValue(3, 7)) * 0.3f;
            sl.alpha = 1.0f;
            sl.life  = 0.18f + (float)GetRandomValue(0, 8) * 0.01f;
            m_lines.push_back(sl);
        }
    }

    void Update(float dt) {
        m_timer -= dt;
        for (auto& sl : m_lines) {
            sl.angle += sl.speed * dt;
            sl.life  -= dt;
            sl.alpha  = fmaxf(0.0f, sl.life / 0.22f);
        }
        m_lines.erase(
            std::remove_if(m_lines.begin(), m_lines.end(),
                [](const SpeedLine& sl){ return sl.life <= 0; }),
            m_lines.end());
    }

    void Draw() const {
        for (const auto& sl : m_lines) {
            Vector2 start = {
                m_origin.x + cosf(sl.angle) * 18.0f,
                m_origin.y + sinf(sl.angle) * 9.0f
            };
            Vector2 end = {
                m_origin.x + cosf(sl.angle) * (18.0f + sl.len),
                m_origin.y + sinf(sl.angle) * (9.0f  + sl.len * 0.5f)
            };
            Color c = m_color;
            c.a = (unsigned char)(sl.alpha * 180);
            DrawLineEx(start, end, 2.2f, c);
        }
    }

    bool IsActive() const { return !m_lines.empty(); }

private:
    std::vector<SpeedLine> m_lines;
    Vector2 m_origin = {};
    Color   m_color  = WHITE;
    float   m_timer  = 0;
};

// =============================================================
// 4. AMBIENT PARTICLE SYSTEM (Polvo / Motas de aire)
// =============================================================
enum class AmbientType { DUST, LEAF };

struct AmbientMote {
    Vector2 pos;
    Vector2 vel;
    float   size;
    float   life;
    float   maxLife;
    float   twist;   
    AmbientType type;
};

class AmbientSystem {
public:
    static AmbientSystem& Get() {
        static AmbientSystem inst;
        return inst;
    }

    void Init(Vector2 arenaCenter, float arenaRadius) {
        m_center = arenaCenter;
        m_radius = arenaRadius;
        // Precarga de motas iniciales
        for (int i = 0; i < TARGET_COUNT; i++) SpawnMote();
    }

    // Devuelve la fuerza de viento actual para otros sistemas
    Vector2 GetWindForce() const {
        float i = m_windIntensity * (0.8f + 0.2f * sinf(m_windPhase * 2.5f));
        return { cosf(m_windPhase) * 60.0f * i, sinf(m_windPhase * 0.7f) * 25.0f * i };
    }

    void Update(float dt, Vector2 playerPos) {
        // --- Motor de Viento Intermitente ---
        m_windStateTimer -= dt;
        if (m_windStateTimer <= 0) {
            int r = GetRandomValue(0, 2);
            if (r == 0) { // Calma
                m_targetIntensity = 0.05f;
                m_windStateTimer = 3.0f + (float)GetRandomValue(0, 4);
            } else if (r == 1) { // Suave
                m_targetIntensity = 0.4f;
                m_windStateTimer = 4.0f + (float)GetRandomValue(0, 6);
            } else { // Brusco
                m_targetIntensity = 1.35f;
                m_windStateTimer = 1.5f + (float)GetRandomValue(0, 3);
            }
        }

        m_windIntensity += (m_targetIntensity - m_windIntensity) * dt * 0.5f;
        m_windPhase += dt * (0.3f + m_windIntensity * 0.6f);

        Vector2 wind = GetWindForce();
        float time = (float)GetTime();

        for (auto& m : m_motes) {
            // Atraccion sutil al jugador
            Vector2 toPlayer = Vector2Subtract(playerPos, m.pos);
            float   d        = Vector2Length(toPlayer);
            if (d < 350.0f) {
                m.vel = Vector2Add(m.vel, Vector2Scale(Vector2Normalize(toPlayer), 8.0f * dt));
            }

            // Aplicar viento
            float windResponse = (m.type == AmbientType::LEAF) ? 1.25f : 0.8f;
            m.vel.x += wind.x * windResponse * dt;
            m.vel.y += wind.y * windResponse * dt;
            
            // Flutter aleatorio para hojas
            if (m.type == AmbientType::LEAF) {
                m.vel.x += sinf(time * 3.5f + m.pos.y) * 15.0f * dt;
                m.twist += dt * (2.5f + m_windIntensity * 5.0f);
            } else {
                m.twist += dt * (0.8f + m_windIntensity * 1.5f);
            }

            m.vel    = Vector2Scale(m.vel, 0.965f); // friccion
            m.pos    = Vector2Add(m.pos, Vector2Scale(m.vel, dt));
            m.life  -= dt;
        }

        m_motes.erase(
            std::remove_if(m_motes.begin(), m_motes.end(),
                [](const AmbientMote& m){ return m.life <= 0; }),
            m_motes.end());

        while ((int)m_motes.size() < TARGET_COUNT) SpawnMote();
    }

    void Draw() const {
        for (const auto& m : m_motes) {
            float a = (m.life / m.maxLife);
            float alpha = (a > 0.8) ? (1.0f - a)/0.2f : (a / 0.8f);
            alpha = alpha * alpha;

            Color baseCol = (m.type == AmbientType::LEAF) ? Color{ 40, 140, 60, 255 } : Color{ 230, 225, 210, 255 };
            if (m.type == AmbientType::LEAF && m.size > 3.0f) baseCol = { 100, 130, 40, 255 }; // Hojas cafes/verdes
            
            float hw = m.size * 0.5f;
            float hh = (m.type == AmbientType::LEAF) ? m.size * 1.8f : m.size * 1.5f;
            
            DrawRectanglePro(
                { m.pos.x, m.pos.y, hw * 2, hh * 2 },
                { hw, hh },
                m.twist * RAD2DEG,
                Fade(baseCol, alpha * 0.45f)
            );
        }
    }

private:
    static constexpr int TARGET_COUNT = 65; // Mas densidad para el 30x30
    std::vector<AmbientMote> m_motes;
    Vector2 m_center    = { 2100, 2100 };
    float   m_radius    = 1800.0f;
    float   m_windPhase = 0;
    float   m_windIntensity = 0.2f;
    float   m_targetIntensity = 0.2f;
    float   m_windStateTimer = 2.0f;

    void SpawnMote() {
        float angle = ((float)GetRandomValue(0, 360)) * DEG2RAD;
        float dist  = (float)GetRandomValue(0, (int)(m_radius * 0.95f));
        AmbientMote m;
        m.pos  = { m_center.x + cosf(angle) * dist, m_center.y + sinf(angle) * dist * 0.5f };
        m.vel  = { (float)GetRandomValue(-15, 15), (float)GetRandomValue(-8, 8) };
        m.size = (float)GetRandomValue(3, 8) * 0.5f;
        float lt = 4.0f + (float)GetRandomValue(0, 80) * 0.1f;
        m.life    = lt;
        m.maxLife = lt;
        m.twist   = (float)GetRandomValue(0, 360) * DEG2RAD;
        m.type    = (GetRandomValue(0, 10) > 4) ? AmbientType::LEAF : AmbientType::DUST;
        m_motes.push_back(m);
    }
};

// =============================================================
// 5. ANIME EMITTER (Helpers para efectos de combate)
// =============================================================
class AnimeEmitter {
public:
    // Rayo procedimental con jitter (recursivo o iterativo)
    static void SpawnLightning(Vector2 start, Vector2 end, Color color, float width = 2.5f) {
        auto& vfx = Graphics::VFXSystem::GetInstance();
        Vector2 dir = Vector2Subtract(end, start);
        float dist = Vector2Length(dir);
        if (dist < 1.0f) return;

        Vector2 unit = Vector2Normalize(dir);
        Vector2 perp = { -unit.y, unit.x };

        int segments = (int)(dist / 15.0f);
        if (segments < 2) segments = 2;

        Vector2 prev = start;
        for (int i = 1; i <= segments; i++) {
            float t = (float)i / segments;
            Vector2 next = Vector2Add(start, Vector2Scale(unit, dist * t));
            
            if (i < segments) {
                float offset = (float)GetRandomValue(-15, 15);
                next = Vector2Add(next, Vector2Scale(perp, offset));
            }

            // Spawnear una partícula tipo "línea" (usando un rombo muy estirado)
            Vector2 mid = Vector2Lerp(prev, next, 0.5f);
            Vector2 segmentDir = Vector2Subtract(next, prev);
            float segmentLen = Vector2Length(segmentDir);
            float angle = atan2f(segmentDir.y, segmentDir.x);

            vfx.SpawnHybrid(mid, {0,0}, 0.12f, WHITE, color, segmentLen * 0.5f, 
                           Graphics::RenderType::RHOMB, BLEND_ADDITIVE, angle * RAD2DEG);
            prev = next;
        }

        // Glow en el impacto
        vfx.SpawnParticleEx(end, {0,0}, 0.2f, color, 25.0f, Graphics::RenderType::CIRCLE, BLEND_ADDITIVE);
        vfx.SpawnParticleEx(end, {0,0}, 0.1f, WHITE, 12.0f, Graphics::RenderType::CIRCLE, BLEND_ADDITIVE);
    }

    static void SpawnAnimeImpact(Vector2 pos, Color col) {
        auto& vfx = Graphics::VFXSystem::GetInstance();
        // Burst circular de diamantes
        for (int i = 0; i < 12; i++) {
            float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
            float speed = (float)GetRandomValue(200, 500);
            vfx.SpawnParticleEx(pos, {cosf(angle) * speed, sinf(angle) * speed}, 
                               0.3f, col, (float)GetRandomValue(4, 8), 
                               Graphics::RenderType::RHOMB, BLEND_ADDITIVE);
        }
    }
};

} // namespace AnimeVFX
