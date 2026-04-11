// =====================================================================
// GameplayScene.cpp - Escena Principal de Combate
// =====================================================================
// Encapsula toda la lógica que antes vivía en GamePhase::RUNNING,
// GamePhase::GAME_OVER y GamePhase::VICTORY dentro de main.cpp.
// =====================================================================
#include "include/scenes/GameplayScene.h"
#include "include/scenes/SceneManager.h"
#include "include/scenes/PauseScene.h"
#include "include/scenes/MainMenuScene.h"
#include "include/scenes/CharacterSelectScene.h"
#include "include/graphics/RenderManager.h"
#include "entities.h"
#include "rlgl.h"
#include <raylib.h>
#include <raymath.h>
#include <cmath>
#include <algorithm>
#include <memory>

// ─── Variables globales compartidas: DEFINIDAS en main.cpp ───────────────────
// Se declaran extern aquí para que el linker las enlace con las de main.cpp.
extern bool  isTimeStopped;
extern float hitstopTimer;
extern float screenShake;

// Las variables de entidad también viven en main.cpp (fuera de namespace Scenes)
extern Reaper  g_reaper;
extern Ropera  g_ropera;
extern Player* g_activePlayer;

namespace Scenes {

// ─── Init ─────────────────────────────────────────────────────────────────
void GameplayScene::Init() {
    m_playerGhostHp = m_activePlayer->hp;
    m_bossGhostHp   = m_boss.hp;
    m_damageTexts.clear();

    // Cámara centrada en el jugador
    m_camera.target   = m_activePlayer->position;
    m_camera.offset   = { (float)GetScreenWidth()  * 0.5f,
                           (float)GetScreenHeight() * 0.5f };
    m_camera.rotation = 0.0f;
    m_camera.zoom     = 1.35f;

    isTimeStopped = false;
    hitstopTimer  = 0.0f;
    screenShake   = 0.0f;

    Graphics::VFXSystem::GetInstance().Clear();
    HideCursor();
}

// ─── Update ───────────────────────────────────────────────────────────────
void GameplayScene::Update(float dt) {
    // ── Pausa ──────────────────────────────────────────────────────────────
    if (IsKeyPressed(KEY_K) || IsKeyPressed(KEY_ESCAPE)) {
        SceneManager::Get().PushOverlay(std::make_unique<PauseScene>());
        return;
    }

    // ── Reinicio rápido ────────────────────────────────────────────────────
    if (IsKeyPressed(KEY_P)) {
        m_activePlayer->Reset({2000, 2000});
        m_boss.hp         = m_boss.maxHp;
        m_boss.position   = m_boss.spawnPos;
        m_boss.velocity   = {0, 0};
        m_boss.aiState    = Enemy::AIState::IDLE;
        m_boss.stateTimer = 1.0f;
        m_boss.isDead     = false;
        m_boss.bleedTimer = 0;
        m_boss.isBleeding = false;
        m_boss.recentDamage = 0.0f;
        m_boss.rocksSpawned = 0;
        m_boss.rocksToSpawn = 0;
        for (int i = 0; i < 5; i++) m_boss.rocks[i].active = false;
        Graphics::VFXSystem::GetInstance().Clear();
        m_damageTexts.clear();
        m_playerGhostHp = m_activePlayer->hp;
        m_bossGhostHp   = m_boss.hp;
        isTimeStopped   = false;
        return;
    }

    // ── Timers globales ────────────────────────────────────────────────────
    if (hitstopTimer > 0) hitstopTimer -= dt;
    if (screenShake  > 0) screenShake  -= dt;

    UpdateCamera(dt);

    // Capturar posición del mouse en coordenadas de mundo
    m_activePlayer->targetAim = GetScreenToWorld2D(GetMousePosition(), m_camera);

    // ── Lógica de juego (no durante hitstop) ──────────────────────────────
    if (hitstopTimer <= 0) {
        m_activePlayer->Update();
        m_activePlayer->HandleSkills(m_boss);
    }

    UpdateBleedDoT(dt);

    if (!m_boss.isDead && !isTimeStopped) {
        m_boss.UpdateAI(*m_activePlayer);
    }
    if (!isTimeStopped) {
        m_boss.Update();
        UpdateRocks(dt);
    }

    // ── Separación de cuerpos ──────────────────────────────────────────────
    if (!m_boss.isDead) {
        float dist    = Vector2Distance(m_activePlayer->position, m_boss.position);
        float minDist = 20.0f + m_boss.radius;
        if (!m_activePlayer->IsImmune() && dist < minDist && dist > 0) {
            Vector2 push  = Vector2Normalize(Vector2Subtract(m_activePlayer->position,
                                                              m_boss.position));
            float   ov    = minDist - dist;
            m_activePlayer->position = Vector2Add(m_activePlayer->position,
                                                   Vector2Scale(push, ov * 0.5f));
            m_boss.position = Vector2Subtract(m_boss.position,
                                               Vector2Scale(push, ov * 0.5f));
        }
    }

    // ── Ghost HP lerp ──────────────────────────────────────────────────────
    m_playerGhostHp = Lerp(m_playerGhostHp, m_activePlayer->hp, 3.0f * dt);
    m_bossGhostHp   = Lerp(m_bossGhostHp,   m_boss.hp,          3.0f * dt);

    // ── Fase Furia del Boss ────────────────────────────────────────────────
    if (m_boss.hp <= m_boss.maxHp * 0.5f && !m_boss.isDead) {
        m_boss.aggressionLevel    = 1.6f;
        m_boss.baseAttackCooldown = m_boss.baseAttackCooldown * 0.8f;
    }

    // ── Colisiones ────────────────────────────────────────────────────────
    UpdateCollisions();

    // ── Partículas y textos flotantes ─────────────────────────────────────
    Graphics::VFXSystem::GetInstance().Update(dt);
    for (auto& t : m_damageTexts) t.Update(dt);
    m_damageTexts.erase(
        std::remove_if(m_damageTexts.begin(), m_damageTexts.end(),
                       [](const DamageText& t) { return t.life <= 0; }),
        m_damageTexts.end());

    // ── Comprobación de fin de partida ────────────────────────────────────
    UpdateDeathCheck();
}

// ─── Draw ─────────────────────────────────────────────────────────────────
void GameplayScene::Draw() {
    DrawWorld();
    DrawHUD();
}

// ─── Unload ───────────────────────────────────────────────────────────────
void GameplayScene::Unload() {
    m_damageTexts.clear();
    Graphics::VFXSystem::GetInstance().Clear();
    ShowCursor();
}

// ─── UpdateCamera ─────────────────────────────────────────────────────────
void GameplayScene::UpdateCamera(float dt) {
    m_camera.offset = { (float)GetScreenWidth()  * 0.5f,
                        (float)GetScreenHeight() * 0.5f };
    float lerpCoeff = 10.0f * dt;
    m_camera.target.x += (m_activePlayer->position.x - m_camera.target.x) * lerpCoeff;
    m_camera.target.y += ((m_activePlayer->position.y - 40.0f) - m_camera.target.y) * lerpCoeff;
}

// ─── UpdateBleedDoT ───────────────────────────────────────────────────────
void GameplayScene::UpdateBleedDoT(float dt) {
    if (!m_boss.isBleeding || m_boss.isDead || isTimeStopped) return;

    m_boss.bleedTimer     -= dt;
    m_boss.bleedTickTimer -= dt;

    if (m_boss.bleedTickTimer <= 0) {
        m_boss.bleedTickTimer = 1.0f;
        float tickDmg = (m_boss.maxHp * 0.05f) / 10.0f;
        m_boss.hp -= tickDmg;

        m_damageTexts.push_back({
            m_boss.position,
            { (float)GetRandomValue(-25, 25), -60.0f },
            1.2f, 1.2f, (int)tickDmg, {255, 30, 30, 255}
        });
        for (int i = 0; i < 6; i++) {
            Graphics::VFXSystem::GetInstance().SpawnParticle(
                m_boss.position,
                { (float)GetRandomValue(-100, 100), (float)GetRandomValue(-100, 100) },
                0.6f, {255, 0, 0, 255});
        }
    }
    if (m_boss.bleedTimer <= 0) {
        m_boss.bleedTimer = 0;
        m_boss.isBleeding = false;
    }
}

// ─── UpdateRocks ──────────────────────────────────────────────────────────
void GameplayScene::UpdateRocks(float dt) {
    for (int i = 0; i < m_boss.rocksSpawned; i++) {
        if (!m_boss.rocks[i].active) continue;
        m_boss.rocks[i].fallTimer -= dt;
        if (m_boss.rocks[i].fallTimer <= 0) {
            m_boss.rocks[i].active = false;

            Vector2 diff  = Vector2Subtract(m_activePlayer->position, m_boss.rocks[i].position);
            float   dist  = sqrtf(diff.x * diff.x + (diff.y * 2.0f) * (diff.y * 2.0f));
            if (dist <= 60.0f + m_activePlayer->radius && !m_activePlayer->IsImmune()) {
                m_activePlayer->hp -= 20.0f;
                screenShake = fmaxf(screenShake, 1.2f);
            }
            for (int k = 0; k < 10; k++) {
                Graphics::VFXSystem::GetInstance().SpawnParticle(
                    m_boss.rocks[i].position,
                    { (float)GetRandomValue(-200, 200), (float)GetRandomValue(-200, 200) },
                    0.5f, DARKGRAY);
            }
        }
    }
}

// ─── UpdateCollisions ─────────────────────────────────────────────────────
void GameplayScene::UpdateCollisions() {
    if (hitstopTimer > 0 || m_boss.isDead) return;

    float prevHp = m_boss.hp;
    m_activePlayer->CheckCollisions(m_boss);
    float dmg = prevHp - m_boss.hp;
    if (dmg <= 0) return;

    // Stagger
    m_boss.recentDamage      += dmg;
    m_boss.recentDamageTimer  = 1.2f;
    if (m_boss.recentDamage >= 80.0f &&
        m_boss.aiState != Enemy::AIState::STAGGERED)
    {
        m_boss.aiState        = Enemy::AIState::STAGGERED;
        m_boss.stateTimer     = 1.2f;
        m_boss.recentDamage   = 0.0f;
        m_boss.attackPhaseTimer = 0.0f;
        screenShake = fmaxf(screenShake, 2.0f);
    }

    m_damageTexts.push_back({
        m_boss.position,
        { (float)GetRandomValue(-40, 40), -120.0f },
        1.0f, 1.0f, (int)dmg, {200, 0, 255, 255}
    });
    screenShake = fmaxf(screenShake, dmg * 0.015f);

    for (int i = 0; i < 16; i++) {
        float angle = atan2f(m_activePlayer->facing.y, m_activePlayer->facing.x)
                    + (float)GetRandomValue(-70, 70) * DEG2RAD;
        Graphics::VFXSystem::GetInstance().SpawnParticle(
            m_boss.position,
            { cosf(angle) * 400.0f, sinf(angle) * 400.0f },
            0.5f, {200, 0, 255, 255});
    }
}

// ─── UpdateDeathCheck ─────────────────────────────────────────────────────
void GameplayScene::UpdateDeathCheck() {
    // El jugador muere → pantalla Game Over (gestionada en Draw vía overlay simple)
    // El boss muere  → pantalla de Victoria
    // Por ahora los estados se detectan en Draw() y se muestran inline.
    // Para más robustez, se puede hacer ChangeScene a una GameOverScene.
}

// ─── DrawWorld ────────────────────────────────────────────────────────────
void GameplayScene::DrawWorld() {
    Camera2D shakeCam = m_camera;
    if (screenShake > 0) {
        shakeCam.offset.x += (float)GetRandomValue(-1, 1) * screenShake;
        shakeCam.offset.y += (float)GetRandomValue(-1, 1) * screenShake;
    }

    BeginMode2D(shakeCam);
    DrawArena();

    // Z-sorting mediante RenderManager
    Graphics::RenderManager::GetInstance().Submit(
        m_activePlayer->GetZDepth(),
        [this]() { m_activePlayer->Draw(); });
    Graphics::RenderManager::GetInstance().Submit(
        m_boss.GetZDepth(),
        [this]() { m_boss.Draw(); });

    Graphics::VFXSystem::GetInstance().SubmitDraws();
    Graphics::RenderManager::GetInstance().Render();

    // Textos de daño flotantes (en espacio mundo)
    for (auto& t : m_damageTexts) t.Draw();

    // Indicador de sangrado
    if (m_boss.isBleeding && !m_boss.isDead) {
        float pulse = 0.5f + 0.4f * sinf((float)GetTime() * 8.0f);
        rlPushMatrix();
        rlTranslatef(m_boss.position.x, m_boss.position.y, 0);
        rlScalef(1.0f, 0.5f, 1.0f);
        DrawCircleLines(0, 0, m_boss.radius + 8.0f,  Fade({220, 30, 30, 255}, pulse));
        DrawCircleLines(0, 0, m_boss.radius + 12.0f, Fade({220, 30, 30, 255}, pulse * 0.5f));
        rlPopMatrix();
        DrawText(TextFormat("BLEED %.1fs", m_boss.bleedTimer),
                 (int)m_boss.position.x - 35,
                 (int)m_boss.position.y - m_boss.radius - 65,
                 14, {220, 50, 50, 255});
    }

    EndMode2D();
}

// ─── DrawArena ────────────────────────────────────────────────────────────
void GameplayScene::DrawArena() {
    Vector2 pN = {2000, 1300}, pS = {2000, 2700};
    Vector2 pE = {3400, 2000}, pW = {600,  2000};
    Color ground = {245, 245, 245, 255};
    Color wall   = {180, 185, 195, 255};
    Color border = {60,  65,  75,  255};
    float wh = 120.0f;

    DrawQuad(pW, pN, {pN.x, pN.y - wh}, {pW.x, pW.y - wh}, wall);
    DrawQuad(pN, pE, {pE.x, pE.y - wh}, {pN.x, pN.y - wh},
             ColorBrightness(wall, -0.1f));
    DrawTriangle(pW, pN, pE, ground);
    DrawTriangle(pW, pE, pS, ground);
    DrawLineEx(pN, pE, 5.0f, border);
    DrawLineEx(pE, pS, 5.0f, border);
    DrawLineEx(pS, pW, 5.0f, border);
    DrawLineEx(pW, pN, 5.0f, border);
    DrawLineEx({pW.x, pW.y - wh}, {pN.x, pN.y - wh}, 5.0f, border);
    DrawLineEx({pN.x, pN.y - wh}, {pE.x, pE.y - wh}, 5.0f, border);
    DrawLineEx(pW, {pW.x, pW.y - wh}, 5.0f, border);
    DrawLineEx(pN, {pN.x, pN.y - wh}, 5.0f, border);
    DrawLineEx(pE, {pE.x, pE.y - wh}, 5.0f, border);
}

// ─── DrawHUD ──────────────────────────────────────────────────────────────
void GameplayScene::DrawHUD() {
    bool showHUD = (m_activePlayer->hp > 0 && !m_boss.isDead) ||
                   (m_activePlayer->hp > 0);

    if (showHUD) {
        float pct    = m_activePlayer->hp / m_activePlayer->maxHp;
        float ghost  = m_playerGhostHp    / m_activePlayer->maxHp;
        float hudX   = 20.0f, hudY = 20.0f;
        float barW   = 650.0f,  barH = 45.0f;

        // Fondo de la barra
        DrawRectangle((int)hudX, (int)hudY, (int)barW, (int)barH, {30, 30, 30, 255});
        // Ghost HP (Lerp suave en blanco)
        DrawRectangle((int)hudX, (int)hudY, (int)(barW * ghost), (int)barH, {250, 250, 250, 100});
        
        // Vida con relleno sólido y textura (Clipped)
        if (ResourceManager::texVida.id != 0) {
            // Relleno sólido central
            DrawRectangle((int)hudX, (int)hudY, (int)(barW * pct), (int)barH, {0, 200, 50, 255}); 
            
            Rectangle src = { 0, 0, (float)ResourceManager::texVida.width * pct, (float)ResourceManager::texVida.height };
            Rectangle dst = { hudX, hudY, barW * pct, barH };
            // Dibujamos la textura encima (quizás tenga brillos o bordes)
            DrawTexturePro(ResourceManager::texVida, src, dst, {0,0}, 0.0f, WHITE);
        } else {
            DrawRectangle((int)hudX, (int)hudY, (int)(barW * pct), (int)barH, {0, 228, 48, 255});
        }
        DrawRectangleLinesEx({hudX, hudY, barW, barH}, 2.0f, BLACK);

        // Energía
        float ePct  = m_activePlayer->energy / m_activePlayer->maxEnergy;
        float enerY = hudY + barH + 8.0f;
        float enerW = 550.0f;
        float enerH = 22.0f;
        
        DrawRectangle((int)hudX, (int)enerY, (int)enerW, (int)enerH, {30, 30, 30, 255});
        if (ResourceManager::texEnergia.id != 0) {
            // Relleno sólido amarillo-naranjoso
            DrawRectangle((int)hudX, (int)enerY, (int)(enerW * ePct), (int)enerH, {255, 180, 0, 255});

            Rectangle src = { 0, 0, (float)ResourceManager::texEnergia.width * ePct, (float)ResourceManager::texEnergia.height };
            Rectangle dst = { hudX, enerY, enerW * ePct, enerH };
            DrawTexturePro(ResourceManager::texEnergia, src, dst, {0,0}, 0.0f, WHITE);
        } else {
            DrawRectangle((int)hudX, (int)enerY, (int)(enerW * ePct), (int)enerH, {255, 180, 0, 255});
        }
        DrawRectangleLinesEx({hudX, enerY, enerW, enerH}, 2.0f, BLACK);

        DrawText(m_activePlayer->GetName().c_str(),
                 (int)hudX, (int)(enerY + 20), 18, m_activePlayer->GetHUDColor());
        DrawText(TextFormat("%d / %d", (int)m_activePlayer->hp, (int)m_activePlayer->maxHp),
                 (int)(hudX + barW - 100), (int)(hudY + 8), 16, WHITE);

        // Boss bar
        float bw = 400.0f, bh = 22.0f;
        float bx = GetScreenWidth() - bw - 25.0f, by = 25.0f;
        float bPct   = m_boss.hp / m_boss.maxHp;
        float bGhost = m_bossGhostHp / m_boss.maxHp;
        Color bCol   = (m_boss.hp <= m_boss.maxHp * 0.5f) ? RED : MAROON;

        DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, {40, 40, 40, 255});
        DrawRectangle((int)bx, (int)by, (int)(bw * bGhost), (int)bh, Fade(WHITE, 0.4f));
        DrawRectangle((int)bx, (int)by, (int)(bw * bPct),   (int)bh, bCol);
        DrawRectangleLines((int)bx, (int)by, (int)bw, (int)bh, {20, 20, 20, 255});

        const char* bName = (m_boss.hp <= m_boss.maxHp * 0.5f)
                              ? "BOSS ENFURECIDO" : "GUARDIAN DE LA ARENA";
        DrawText(bName, (int)(bx + bw) - MeasureText(bName, 18),
                 (int)(by + bh + 5), 18, GOLD);

        // Habilidades
        auto abs = m_activePlayer->GetAbilities();
        float sh  = (float)GetScreenHeight();
        float sc  = 1.8f;
        float abW = 64.0f * sc, abH = 64.0f * sc;
        for (size_t i = 0; i < abs.size(); i++) {
            float ax = 20.0f + i * (abW + 12.0f);
            float ay = sh - abH - 20.0f;
            Color abBg = abs[i].ready ? Fade({60, 0, 80, 255}, 0.9f) : Fade(BLACK, 0.7f);
            DrawRectangle((int)ax, (int)ay, (int)abW, (int)abH, abBg);
            DrawRectangleLines((int)ax, (int)ay, (int)abW, (int)abH,
                               abs[i].ready ? abs[i].color : DARKGRAY);
            DrawText(abs[i].label.c_str(), (int)ax + 4, (int)ay + 6, 14, WHITE);
            if (abs[i].cooldown > 0) {
                float cdPct = fminf(abs[i].cooldown / abs[i].maxCooldown, 1.0f);
                DrawRectangle((int)ax, (int)(ay + abH - 8),
                              (int)(abW * (1.0f - cdPct)), 8, abs[i].color);
                DrawText(TextFormat("%.1f", abs[i].cooldown),
                         (int)(ax + abW / 2) - 15, (int)(ay + abH / 2) - 10, 20, WHITE);
            } else {
                DrawRectangle((int)ax, (int)(ay + abH - 8), (int)abW, 8, abs[i].color);
            }
        }

        // Status especiales
        std::string status = m_activePlayer->GetSpecialStatus();
        if (!status.empty()) {
            DrawText(status.c_str(),
                     GetScreenWidth() / 2 - MeasureText(status.c_str(), 28) / 2,
                     30, 28, {255, 120, 0, 255});
        }
        if (m_activePlayer->IsBuffed()) {
            DrawText(TextFormat("BUFF %.1fs", m_activePlayer->GetBuffTimer()),
                     GetScreenWidth() / 2 - 60, 62, 20, {180, 0, 255, 255});
        }
    }

    // ── Game Over ──────────────────────────────────────────────────────────
    if (m_activePlayer->hp <= 0.0f) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.85f));
        
        // Título con sombra desplazada
        const char* deadMsg = "HAS CAIDO EN COMBATE";
        int deadFs = 60;
        int dMsgX = GetScreenWidth() / 2 - MeasureText(deadMsg, deadFs) / 2;
        DrawText(deadMsg, dMsgX + 4, 184, deadFs, Fade(BLACK, 0.7f));
        DrawText(deadMsg, dMsgX, 180, deadFs, RED);

        // Subtítulo
        const char* subDead = "Tu alma ha sido reclamada por el abismo";
        DrawText(subDead, GetScreenWidth()/2 - MeasureText(subDead, 20)/2, 260, 20, GRAY);

        Rectangle btnRevive = {(float)GetScreenWidth() / 2 - 160, 380, 320, 60};
        Rectangle btnChar   = {(float)GetScreenWidth() / 2 - 160, 460, 320, 60};
        Rectangle btnMain   = {(float)GetScreenWidth() / 2 - 160, 540, 320, 60};

        auto DrawDeadBtn = [&](Rectangle btn, const char* label, Color col) {
            bool hov = CheckCollisionPointRec(GetMousePosition(), btn);
            DrawRectangleRec(btn, hov ? ColorBrightness(col, 0.2f) : col);
            DrawRectangleLinesEx(btn, 2, hov ? WHITE : Fade(WHITE, 0.3f));
            int fs = 22;
            DrawText(label, (int)(btn.x + btn.width/2 - MeasureText(label, fs)/2), (int)(btn.y + btn.height/2 - fs/2), fs, WHITE);
            return hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        };

        if (DrawDeadBtn(btnRevive, "INTENTAR DE NUEVO", {50, 10, 10, 255})) {
            m_activePlayer->Reset({2000, 2000});
            m_playerGhostHp = m_activePlayer->hp;
        }

        if (DrawDeadBtn(btnChar, "CAMBIAR PERSONAJE", {40, 40, 40, 255})) {
            SceneManager::Get().ChangeScene(std::make_unique<CharacterSelectScene>(g_reaper, g_ropera, g_activePlayer));
        }

        if (DrawDeadBtn(btnMain, "VOLVER AL MENU", {20, 20, 20, 255})) {
            SceneManager::Get().ChangeScene(std::make_unique<MainMenuScene>());
        }
    }

    // ── Victoria ───────────────────────────────────────────────────────────
    if (m_boss.hp <= 0.0f) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.65f));
        
        // Efecto de rayos/brillo circular al fondo
        DrawCircleGradient(GetScreenWidth()/2, GetScreenHeight()/2, 400, Fade(GOLD, 0.15f), {0,0,0,0});

        const char* vicTitle = "!EL GUARDIAN HA CAIDO!";
        int vicFs = 64;
        int vx = GetScreenWidth() / 2 - MeasureText(vicTitle, vicFs) / 2;
        DrawText(vicTitle, vx + 4, 204, vicFs, Fade(BLACK, 0.6f));
        DrawText(vicTitle, vx, 200, vicFs, GOLD);

        const char* vicSub = "Has reclamado el dominio de la arena";
        DrawText(vicSub, GetScreenWidth()/2 - MeasureText(vicSub, 20)/2, 280, 20, LIGHTGRAY);

        Rectangle btnMenu = {(float)GetScreenWidth() / 2 - 150, 450, 300, 65};
        bool hov = CheckCollisionPointRec(GetMousePosition(), btnMenu);
        
        DrawRectangleRec(btnMenu, hov ? YELLOW : GOLD);
        DrawRectangleLinesEx(btnMenu, 3, BLACK);
        
        int fs = 24;
        DrawText("MENU PRINCIPAL", (int)(btnMenu.x + btnMenu.width/2 - MeasureText("MENU PRINCIPAL", fs)/2), (int)(btnMenu.y + btnMenu.height/2 - fs/2), fs, BLACK);
        
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            SceneManager::Get().ChangeScene(std::make_unique<MainMenuScene>());
        }
    }

    // Hint de pausa
    DrawText("K / ESC: Pausa", 10, GetScreenHeight() - 24, 15, Fade(GRAY, 0.5f));
}

// ─── DrawQuad (static helper) ─────────────────────────────────────────────
void GameplayScene::DrawQuad(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, Color c) {
    DrawTriangle(p1, p2, p3, c);
    DrawTriangle(p1, p3, p2, c);
    DrawTriangle(p1, p3, p4, c);
    DrawTriangle(p1, p4, p3, c);
}

} // namespace Scenes
