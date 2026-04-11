// =====================================================================
// SettingsScene.cpp - Ajustes de resolución y reasignación de controles
// =====================================================================
#include "include/scenes/SettingsScene.h"
#include "include/scenes/SceneManager.h"
#include "entities.h"
#include <raylib.h>
#include <cmath>

namespace Scenes {

// ─── Init ─────────────────────────────────────────────────────────────────
void SettingsScene::Init() {
    m_isRebinding  = false;
    m_rebindingKey = nullptr;
    m_rebindingName = "";

    // Layout de botones
    m_btnBack       = { 20.0f,  20.0f,  130.0f, 40.0f };
    m_btnRes720     = { 300.0f, 110.0f, 100.0f, 40.0f };
    m_btnRes900     = { 410.0f, 110.0f, 100.0f, 40.0f };
    m_btnRes1080    = { 520.0f, 110.0f, 100.0f, 40.0f };
    m_btnFullscreen = { 650.0f, 110.0f, 220.0f, 40.0f };

    ShowCursor();
}

// ─── Update ───────────────────────────────────────────────────────────────
void SettingsScene::Update(float /*dt*/) {
    Vector2 mouse = GetMousePosition();

    // ── Capturar tecla en modo rebinding ─────────────────────────────────
    if (m_isRebinding) {
        int key = GetKeyPressed();
        if (key > 0 && m_rebindingKey) {
            *m_rebindingKey = key;
            m_isRebinding   = false;
            m_rebindingKey  = nullptr;
            m_rebindingName = "";
        }
        return; // Bloquear todo input mientras se reasigna
    }

    // ── Volver (botón o ESC) ──────────────────────────────────────────────
    if (IsKeyPressed(KEY_ESCAPE) ||
        (CheckCollisionPointRec(mouse, m_btnBack) &&
         IsMouseButtonPressed(MOUSE_LEFT_BUTTON)))
    {
        if (m_onBack) m_onBack();
        return;
    }

    // ── Resoluciones ─────────────────────────────────────────────────────
    if (CheckCollisionPointRec(mouse, m_btnRes720) &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        SetWindowSize(1280, 720);

    if (CheckCollisionPointRec(mouse, m_btnRes900) &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        SetWindowSize(1600, 900);

    if (CheckCollisionPointRec(mouse, m_btnRes1080) &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        SetWindowSize(1920, 1080);

    if (CheckCollisionPointRec(mouse, m_btnFullscreen) &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        ToggleFullscreen();
}

// ─── Draw ─────────────────────────────────────────────────────────────────
void SettingsScene::Draw() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    // Fondo
    DrawRectangle(0, 0, sw, sh, {13, 13, 20, 255});

    // Línea de acento lateral izquierdo
    DrawRectangle(0, 0, 5, sh, GOLD);

    // Título
    DrawText("AJUSTES Y CONTROLES", 100, 50, 30, GOLD);
    DrawLineEx({100, 88}, {500, 88}, 1.5f, Fade(GOLD, 0.4f));

    Vector2 mouse = GetMousePosition();

    // ── Botón Volver ────────────────────────────────────────────────────
    bool hovBack = CheckCollisionPointRec(mouse, m_btnBack);
    DrawRectangleRec(m_btnBack, hovBack ? GRAY : DARKGRAY);
    DrawRectangleLinesEx(m_btnBack, 1.5f, Fade(WHITE, 0.4f));
    DrawText("< VOLVER",
             (int)(m_btnBack.x + 12), (int)(m_btnBack.y + 10), 18, WHITE);

    // ── Sección Resolución ───────────────────────────────────────────────
    DrawText("RESOLUCION:", 100, 120, 20, LIGHTGRAY);

    auto DrawResBtn = [&](Rectangle btn, const char* label) {
        bool hov = CheckCollisionPointRec(mouse, btn);
        DrawRectangleRec(btn, hov ? Color{60, 60, 120, 255} : DARKGRAY);
        DrawRectangleLinesEx(btn, 1.5f, hov ? WHITE : Fade(WHITE, 0.3f));
        DrawText(label,
                 (int)(btn.x + btn.width * 0.5f) - MeasureText(label, 18) / 2,
                 (int)(btn.y + 11), 18, WHITE);
    };
    DrawResBtn(m_btnRes720,  "720p");
    DrawResBtn(m_btnRes900,  "900p");
    DrawResBtn(m_btnRes1080, "1080p");

    // Botón Fullscreen
    bool hovFS = CheckCollisionPointRec(mouse, m_btnFullscreen);
    DrawRectangleRec(m_btnFullscreen,
                     hovFS ? Color{120, 30, 30, 255} : MAROON);
    DrawRectangleLinesEx(m_btnFullscreen, 1.5f, hovFS ? WHITE : Fade(WHITE, 0.3f));
    const char* fsLabel = IsWindowFullscreen() ? "MODO VENTANA" : "PANTALLA COMPLETA";
    DrawText(fsLabel,
             (int)(m_btnFullscreen.x + m_btnFullscreen.width * 0.5f) - MeasureText(fsLabel, 18) / 2,
             (int)(m_btnFullscreen.y + 11), 18, WHITE);

    // ── Sección Controles ───────────────────────────────────────────────
    DrawText("CONTROLES (Click para reasignar):", 100, 195, 20, LIGHTGRAY);
    DrawLineEx({100, 222}, {700, 222}, 1.0f, Fade(LIGHTGRAY, 0.25f));

    if (m_activePlayer) {
        DrawRebindRow("Dash",       &m_activePlayer->controls.dash,      255);
        DrawRebindRow("Q - Skill",  &m_activePlayer->controls.boomerang, 300);
        DrawRebindRow("E - Skill",  &m_activePlayer->controls.berserker, 345);
        DrawRebindRow("R - Ultimate",&m_activePlayer->controls.ultimate, 390);
    }

    // Rebinding activo
    if (m_isRebinding) {
        DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.55f));
        DrawText(TextFormat("Pulsa una tecla para asignar [ %s ]",
                            m_rebindingName.c_str()),
                 sw / 2 - MeasureText("Pulsa una tecla...", 22) / 2,
                 sh / 2 - 20, 22, SKYBLUE);
        DrawText("(ESC cancela)",
                 sw / 2 - MeasureText("(ESC cancela)", 16) / 2,
                 sh / 2 + 16, 16, Fade(GRAY, 0.7f));
    }

    DrawText("ESC: Volver", 10, sh - 24, 15, Fade(GRAY, 0.55f));
}

// ─── Unload ───────────────────────────────────────────────────────────────
void SettingsScene::Unload() {
    m_isRebinding  = false;
    m_rebindingKey = nullptr;
}

// ─── Helper: DrawRebindRow ────────────────────────────────────────────────
void SettingsScene::DrawRebindRow(const char* label, int* key, int y) {
    Vector2 mouse = GetMousePosition();
    DrawText(label, 120, y, 20, WHITE);

    Rectangle r   = { 340.0f, (float)y - 5, 160.0f, 30.0f };
    bool      hov = CheckCollisionPointRec(mouse, r);
    bool      isRebindingThis = (m_isRebinding && m_rebindingKey == key);

    Color bg  = isRebindingThis ? Color{30, 60, 120, 255}
              : hov             ? GRAY
                                : DARKGRAY;
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 1.5f, isRebindingThis ? SKYBLUE
                                                    : Fade(WHITE, 0.3f));

    const char* keyLabel = isRebindingThis ? "..."
                                           : TextFormat("KEY %i", *key);
    DrawText(keyLabel, (int)(r.x + 10), y, 18,
             isRebindingThis ? SKYBLUE : GOLD);

    if (hov && !m_isRebinding && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        m_isRebinding   = true;
        m_rebindingKey  = key;
        m_rebindingName = label;
    }
}

} // namespace Scenes
