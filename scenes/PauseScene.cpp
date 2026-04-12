// =====================================================================
// PauseScene.cpp - Overlay de Pausa
// =====================================================================
// Se superpone a GameplayScene sin descargarla de memoria.
// Se activa con: SceneManager::Get().PushOverlay(make_unique<PauseScene>())
// Se cierra con: SceneManager::Get().PopOverlay()
// =====================================================================
#include "include/scenes/PauseScene.h"
#include "include/scenes/SceneManager.h"
#include "include/scenes/MainMenuScene.h"
#include "include/scenes/SettingsScene.h"
#include "../entities.h"
#include <raylib.h>
#include <cmath>
#include <memory>

// ─── Externas (definidas en main.cpp, fuera de namespace) ────────────────
extern Player* g_activePlayer;

namespace Scenes {

// ─── Init ─────────────────────────────────────────────────────────────────
void PauseScene::Init() {
    float cx = GetScreenWidth()  * 0.5f;
    float cy = GetScreenHeight() * 0.5f;
    float bw = 280.0f, bh = 55.0f, gap = 16.0f;

    m_btnResume   = { cx - bw * 0.5f, cy - 30.0f,          bw, bh };
    m_btnSettings = { cx - bw * 0.5f, cy - 30.0f + bh + gap,    bw, bh };
    m_btnMainMenu = { cx - bw * 0.5f, cy - 30.0f + (bh + gap)*2, bw, bh };

    ShowCursor();
}

// ─── Update ───────────────────────────────────────────────────────────────
void PauseScene::Update(float /*dt*/) {
    Vector2 mouse = GetMousePosition();

    // ── Reanudar (K, ESC o botón) ─────────────────────────────────────────
    if (IsKeyPressed(KEY_K) || IsKeyPressed(KEY_ESCAPE) ||
        (CheckCollisionPointRec(mouse, m_btnResume) &&
         IsMouseButtonPressed(MOUSE_LEFT_BUTTON)))
    {
        SceneManager::Get().PopOverlay();   // Cierra el overlay; GameplayScene reanuda
        return;
    }

    // ── Ajustes ───────────────────────────────────────────────────────────
    if (CheckCollisionPointRec(mouse, m_btnSettings) &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        // Cerramos la pausa y empujamos Settings como nueva escena overlay.
        // onBack regresa al overlay de pausa de nuevo.
        SceneManager::Get().PopOverlay();
        SceneManager::Get().PushOverlay(
            std::make_unique<SettingsScene>(g_activePlayer, []() {
                SceneManager::Get().PopOverlay();
                SceneManager::Get().PushOverlay(std::make_unique<PauseScene>());
            }));
        return;
    }

    // ── Menú Principal ────────────────────────────────────────────────────
    if (CheckCollisionPointRec(mouse, m_btnMainMenu) &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        // PopOverlay + ChangeScene destruirá la GameplayScene limpiamente.
        SceneManager::Get().PopOverlay();
        SceneManager::Get().ChangeScene(std::make_unique<MainMenuScene>());
        return;
    }
}

// ─── Draw ─────────────────────────────────────────────────────────────────
void PauseScene::Draw() {
    // Fondo semitransparente con efecto de oscurecimiento profundo
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(BLACK, 0.75f));

    // Panel central con degradado sutil
    float pw = 420.0f, ph = 380.0f;
    float px = GetScreenWidth()  * 0.5f - pw * 0.5f;
    float py = GetScreenHeight() * 0.5f - ph * 0.5f;
    
    // Sombra del panel
    DrawRectangle((int)px + 6, (int)py + 6, (int)pw, (int)ph, Fade(BLACK, 0.4f));
    // Fondo del panel
    DrawRectangle((int)px, (int)py, (int)pw, (int)ph, {24, 24, 38, 255});
    // Borde brillante
    DrawRectangleLinesEx({px, py, pw, ph}, 3.0f, GOLD);
    DrawRectangleLinesEx({px - 2, py - 2, pw + 4, ph + 4}, 1.0f, Fade(GOLD, 0.3f));

    // Título con sombra
    const char* title = "PAUSA";
    int titleFs = 48;
    int tx = GetScreenWidth() / 2 - MeasureText(title, titleFs) / 2;
    DrawText(title, tx + 3, (int)(py + 33), titleFs, Fade(BLACK, 0.5f));
    DrawText(title, tx, (int)(py + 30), titleFs, GOLD);

    // Línea decorativa
    DrawLineEx({ px + 40, py + 95 }, { px + pw - 40, py + 95 },
               2.0f, Fade(GOLD, 0.4f));

    // Helper para botones con estilo mejorado
    Vector2 mouse = GetMousePosition();
    auto DrawBtn = [&](Rectangle btn, const char* label, Color col) {
        bool hov = CheckCollisionPointRec(mouse, btn);
        Color bg = hov ? ColorBrightness(col, 0.35f) : col;
        
        // Efecto hover: cuadro exterior de luz
        if (hov) {
            DrawRectangleLinesEx({btn.x - 4, btn.y - 4, btn.width + 8, btn.height + 8}, 1.5f, Fade(bg, 0.4f));
        }

        DrawRectangleRec(btn, bg);
        DrawRectangleLinesEx(btn, 2.0f, hov ? WHITE : Fade(WHITE, 0.35f));
        
        int fs = 24;
        DrawText(label,
                 (int)(btn.x + btn.width * 0.5f) - MeasureText(label, fs) / 2 + 1,
                 (int)(btn.y + btn.height * 0.5f) - fs / 2 + 1,
                 fs, Fade(BLACK, 0.6f));
        DrawText(label,
                 (int)(btn.x + btn.width * 0.5f) - MeasureText(label, fs) / 2,
                 (int)(btn.y + btn.height * 0.5f) - fs / 2,
                 fs, WHITE);
    };

    DrawBtn(m_btnResume,   "REANUDAR JUEGO", {40, 100, 40,  255});
    DrawBtn(m_btnSettings, "AJUSTES",        {50, 50,  100, 255});
    DrawBtn(m_btnMainMenu, "VOLVER AL MENU", {120, 30,  30,  255});

    // Hint teclado
    DrawText("PULSA K O ESC PARA VOLVER AL COMBATE",
             GetScreenWidth() / 2 - MeasureText("PULSA K O ESC PARA VOLVER AL COMBATE", 16) / 2,
             (int)(py + ph + 20), 16, Fade(GOLD, 0.7f));
}

// ─── Unload ───────────────────────────────────────────────────────────────
void PauseScene::Unload() {
    // Sin recursos propios que liberar.
}

} // namespace Scenes
