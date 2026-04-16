// =====================================================================
// MainMenuScene.cpp - Implementación del Menú Principal
// =====================================================================
#include "../include/scenes/MainMenuScene.h"
#include "../include/scenes/CharacterSelectScene.h"
#include "../include/scenes/SceneManager.h"
#include "../include/scenes/SettingsScene.h"
#include <cmath>
#include <memory>
#include <raylib.h>
#include <raymath.h>

// ─── Externas (definidas en main.cpp, fuera de cualquier namespace) ─────────
extern Reaper g_reaper;
extern Ropera g_ropera;
extern ElementalMage g_mage;
extern Player *g_activePlayer;

namespace Scenes {

// ─── Init ────────────────────────────────────────────────────────────────────
void MainMenuScene::Init() {
  m_bgTime = 0.0f;

  float cx = GetScreenWidth() * 0.5f;
  float cy = GetScreenHeight() * 0.5f;
  float bw = 280.0f, bh = 60.0f;

  m_btnPlay = {cx - bw * 0.5f, cy - 20.0f, bw, bh};
  m_btnSettings = {cx - bw * 0.5f, cy + bh + 20.0f, bw, bh};
  m_btnQuit = {cx - bw * 0.5f, cy + bh * 2 + 60.0f, bw, bh};

  ShowCursor();
}

// ─── Update ──────────────────────────────────────────────────────────────────
void MainMenuScene::Update(float dt) {
  m_bgTime += dt;

  Vector2 mouse = GetMousePosition();

  // ── JUGAR → CharacterSelectScene ─────────────────────────────────────────
  if (CheckCollisionPointRec(mouse, m_btnPlay) &&
      IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    SceneManager::Get().ChangeScene(std::make_unique<CharacterSelectScene>(
        g_reaper, g_ropera, g_mage, g_activePlayer));
    return;
  }

  // ── AJUSTES → SettingsScene ───────────────────────────────────────────────
  if (CheckCollisionPointRec(mouse, m_btnSettings) &&
      IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    // onBack regresa al MainMenu
    SceneManager::Get().ChangeScene(
        std::make_unique<SettingsScene>(g_activePlayer, []() {
          SceneManager::Get().ChangeScene(std::make_unique<MainMenuScene>());
        }));
    return;
  }

  // ── SALIR ────────────────────────────────────────────────────────────────
  if (CheckCollisionPointRec(mouse, m_btnQuit) &&
      IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    SceneManager::Get().Quit();
  }
}

// ─── Draw
// ─────────────────────────────────────────────────────────────────────
void MainMenuScene::Draw() {
  // Fondo animado con degradado
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  DrawRectangle(0, 0, sw, sh, {10, 12, 20, 255});

  // Partículas de fondo (ondas de color)
  for (int i = 0; i < 6; i++) {
    float angle = m_bgTime * 0.3f + i * (PI / 3.0f);
    float r = 350.0f + 80.0f * sinf(m_bgTime * 0.5f + i);
    float cx = sw * 0.5f + cosf(angle) * 300.0f;
    float cy = sh * 0.5f + sinf(angle) * 180.0f;
    Color c = {(unsigned char)(80.0f + 40.0f * sinf((float)i)), 0,
               (unsigned char)(160.0f + 60.0f * cosf((float)i + m_bgTime)), 30};
    DrawCircleGradient((int)cx, (int)cy, r, c, {0, 0, 0, 0});
  }

  // Título
  const char *title = "PROYECTO 1";
  const char *subtitle = "Un juego de lucha isometrico";
  int tsz = 72, subSz = 24;
  float titleY = GetScreenHeight() * 0.22f;

  DrawText(title, sw / 2 - MeasureText(title, tsz) / 2, (int)titleY, tsz, GOLD);
  DrawText(subtitle, sw / 2 - MeasureText(subtitle, subSz) / 2,
           (int)(titleY + tsz + 10), subSz, LIGHTGRAY);

  // Línea decorativa debajo del título
  float lineY = titleY + tsz + subSz + 28.0f;
  DrawLineEx({sw * 0.3f, lineY}, {sw * 0.7f, lineY}, 2.0f, Fade(GOLD, 0.5f));

  // Botones
  Vector2 mouse = GetMousePosition();
  DrawButton(m_btnPlay, "JUGAR", {100, 0, 160, 255},
             CheckCollisionPointRec(mouse, m_btnPlay));
  DrawButton(m_btnSettings, "AJUSTES", {40, 40, 80, 255},
             CheckCollisionPointRec(mouse, m_btnSettings));
  DrawButton(m_btnQuit, "SALIR", {90, 20, 20, 255},
             CheckCollisionPointRec(mouse, m_btnQuit));

  // Versión
  DrawText("v0.1 - alpha", 10, sh - 24, 16, Fade(GRAY, 0.6f));
}

// ─── Unload
// ───────────────────────────────────────────────────────────────────
void MainMenuScene::Unload() {
  // No hay recursos exclusivos de esta escena.
}

// ─── Helper: DrawButton
// ───────────────────────────────────────────────────────
void MainMenuScene::DrawButton(Rectangle btn, const char *label,
                               Color baseColor, bool hovered) const {
  Color bg = hovered ? ColorBrightness(baseColor, 0.35f) : baseColor;

  // Sombra del botón
  DrawRectangle((int)btn.x + 4, (int)btn.y + 4, (int)btn.width, (int)btn.height,
                Fade(BLACK, 0.3f));

  // Efecto de brillo exterior en hover
  if (hovered) {
    DrawRectangleLinesEx(
        {btn.x - 5, btn.y - 5, btn.width + 10, btn.height + 10}, 1.5f,
        Fade(bg, 0.4f));
  }

  DrawRectangleRec(btn, bg);
  DrawRectangleLinesEx(btn, 2.5f, hovered ? WHITE : Fade(WHITE, 0.4f));

  int fontSize = 28;
  // Sombra del texto
  int tx = (int)(btn.x + btn.width / 2) - MeasureText(label, fontSize) / 2;
  int ty = (int)(btn.y + btn.height / 2) - fontSize / 2;

  DrawText(label, tx + 2, ty + 2, fontSize, Fade(BLACK, 0.5f));
  DrawText(label, tx, ty, fontSize, WHITE);
}

} // namespace Scenes
