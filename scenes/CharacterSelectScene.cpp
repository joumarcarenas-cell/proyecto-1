// =====================================================================
// CharacterSelectScene.cpp - Selección de Personaje estilo MK
// =====================================================================
#include "../include/scenes/CharacterSelectScene.h"
#include "../include/graphics/VFXSystem.h"
#include "../include/scenes/GameplayScene.h"
#include "../include/scenes/MainMenuScene.h"
#include "../include/scenes/SceneManager.h"
#include <cmath>
#include <memory>
#include <raylib.h>
#include <raymath.h>

extern float screenShake;
// g_reaper, g_ropera and g_activePlayer are passed via constructor as
// references

namespace Scenes {

// ─── Init ────────────────────────────────────────────────────────────────────
void CharacterSelectScene::Init() {
  m_cursorCol = 0;
  m_cursorRow = 0;
  m_selectedIdx = -1;
  m_cursorPulse = 0.0f;

  // Población de la cuadrícula:
  // Los índices 0 y 1 son Reaper y Ropera (implementados).
  // El resto son slots de "PRÓXIMAMENTE" para expandir en el futuro.
  m_characters.clear();
  m_characters.push_back(
      {"SEGADOR", "Cosechador de almas", {180, 0, 255, 255}});
  m_characters.push_back({"ROPERA", "REDISEÑANDO...", {0, 220, 180, 100}});
  m_characters.push_back({"MAGO", "Control elemental", {0, 180, 255, 255}});
  m_characters.push_back({"???", "Proximamente", {60, 60, 60, 180}});
  m_characters.push_back({"???", "Proximamente", {60, 60, 60, 180}});
  m_characters.push_back({"???", "Proximamente", {60, 60, 60, 180}});
  m_characters.push_back({"???", "Proximamente", {60, 60, 60, 180}});
  m_characters.push_back({"???", "Proximamente", {60, 60, 60, 180}});

  ShowCursor();
}

// ─── Update ──────────────────────────────────────────────────────────────────
void CharacterSelectScene::Update(float dt) {
  m_cursorPulse += dt * 4.0f;

  // ── Navegación del cursor ─────────────────────────────────────────────────
  if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) {
    m_cursorCol = (m_cursorCol + 1) % COLS;
  }
  if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) {
    m_cursorCol = (m_cursorCol - 1 + COLS) % COLS;
  }
  if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
    m_cursorRow = (m_cursorRow + 1) % ROWS;
  }
  if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
    m_cursorRow = (m_cursorRow - 1 + ROWS) % ROWS;
  }

  int hoveredIdx = CursorIndex();
  bool isValidSlot = (hoveredIdx < (int)m_characters.size() &&
                      m_characters[hoveredIdx].subtitle != "REDISEÑANDO..." &&
                      m_characters[hoveredIdx].name != "???");

  // ── Confirmar selección (Enter o Click) ───────────────────────────────────
  auto confirmSelection = [&]() {
    if (!isValidSlot)
      return;

    m_selectedIdx = hoveredIdx;

    // Asignar el personaje activo según el índice seleccionado
    if (m_selectedIdx == 0) {
      m_reaper.Reset({2000, 2000});
      m_activePlayer = &m_reaper;
    } else if (m_selectedIdx == 1) {
      m_ropera.Reset({2000, 2000});
      m_activePlayer = &m_ropera;
    } else if (m_selectedIdx == 2) {
      m_mage.Reset({2000, 2000});
      m_activePlayer = &m_mage;
    }

    Graphics::VFXSystem::GetInstance().Clear();
    screenShake = 0.0f;

    // Transición a GameplayScene (ahora gestiona su propia cola de jefes)
    SceneManager::Get().ChangeScene(
        std::make_unique<GameplayScene>(m_activePlayer));
  };

  if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
    confirmSelection();
    return;
  }

  // ── Soporte para Mouse (Hover y Click) ──────────────────────────────────
  Vector2 mousePos = GetMousePosition();
  float sw = (float)GetScreenWidth();
  float sh = (float)GetScreenHeight();

  float cardW = 200.0f, cardH = 220.0f;
  float gapX = 20.0f, gapY = 20.0f;
  float gridW = COLS * cardW + (COLS - 1) * gapX;
  float gridX = sw * 0.5f - gridW * 0.5f;
  float gridY = sh * 0.35f;

  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLS; col++) {
      float cx = gridX + col * (cardW + gapX);
      float cy = gridY + row * (cardH + gapY);
      Rectangle card = {cx, cy, cardW, cardH};

      if (CheckCollisionPointRec(mousePos, card)) {
        // Al pasar el mouse, actualizamos el cursor visual
        if (m_cursorCol != col || m_cursorRow != row) {
          m_cursorCol = col;
          m_cursorRow = row;
        }

        // Si hace click, confirmamos
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
          confirmSelection();
          return;
        }
      }
    }
  }

  // ESC vuelve al menú principal
  if (IsKeyPressed(KEY_ESCAPE)) {
    SceneManager::Get().ChangeScene(std::make_unique<MainMenuScene>());
  }
}

// ─── Draw
// ─────────────────────────────────────────────────────────────────────
void CharacterSelectScene::Draw() {
  int sw = GetScreenWidth(), sh = GetScreenHeight();

  // ── Fondo ────────────────────────────────────────────────────────────────
  DrawRectangle(0, 0, sw, sh, {10, 12, 22, 255});

  // Efecto de cuadrícula de perspectiva al fondo
  for (int i = 0; i < 20; i++) {
    float y = (float)sh / 20.0f * (float)i;
    DrawLineEx({0, y}, {(float)sw, y}, 1.0f, Fade({40, 40, 80, 255}, 0.3f));
  }
  for (int i = 0; i < 30; i++) {
    float x = (float)sw / 30.0f * (float)i;
    DrawLineEx({x, 0}, {x, (float)sh}, 1.0f, Fade({40, 40, 80, 255}, 0.3f));
  }

  // ── Título ───────────────────────────────────────────────────────────────
  const char *title = "SELECCION DE PERSONAJE";
  int tsz = 48;
  DrawText(title, sw / 2 - MeasureText(title, tsz) / 2, (int)(sh * 0.08f), tsz,
           GOLD);

  const char *hint =
      "WASD / Flechas para mover  |  ENTER o Click para seleccionar";
  DrawText(hint, sw / 2 - MeasureText(hint, 18) / 2, (int)(sh * 0.18f), 18,
           Fade(LIGHTGRAY, 0.7f));

  // ── Cuadrícula de personajes ──────────────────────────────────────────────
  float cardW = 200.0f, cardH = 220.0f;
  float gapX = 20.0f, gapY = 20.0f;
  float gridW = COLS * cardW + (COLS - 1) * gapX;
  float gridX = sw * 0.5f - gridW * 0.5f;
  float gridY = sh * 0.35f;

  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLS; col++) {
      int idx = row * COLS + col;
      float cx = gridX + col * (cardW + gapX);
      float cy = gridY + row * (cardH + gapY);
      bool isCursor = (col == m_cursorCol && row == m_cursorRow);
      bool isSelected = (idx == m_selectedIdx);
      DrawCharacterCard(idx, cx, cy, cardW, cardH, isCursor, isSelected);
    }
  }

  // ── Preview del personaje bajo el cursor ──────────────────────────────────
  int curIdx = CursorIndex();
  if (curIdx < (int)m_characters.size()) {
    const auto &ch = m_characters[curIdx];
    float previewY = gridY + ROWS * (cardH + gapY) + 20.0f;

    DrawText(ch.name.c_str(), sw / 2 - MeasureText(ch.name.c_str(), 36) / 2,
             (int)previewY, 36, ch.accentColor);
    DrawText(ch.subtitle.c_str(),
             sw / 2 - MeasureText(ch.subtitle.c_str(), 20) / 2,
             (int)(previewY + 44), 20, LIGHTGRAY);
  }

  // ESC hint
  DrawText("ESC: Menu principal", 10, sh - 24, 16, Fade(GRAY, 0.6f));
}

// ─── Unload
// ───────────────────────────────────────────────────────────────────
void CharacterSelectScene::Unload() { m_characters.clear(); }

// ─── Helper: DrawCharacterCard ───────────────────────────────────────────────
void CharacterSelectScene::DrawCharacterCard(int idx, float x, float y, float w,
                                             float h, bool isCursor,
                                             bool isSelected) const {
  bool locked = (idx >= (int)m_characters.size() || 
                 m_characters[idx].name == "???" || 
                 m_characters[idx].subtitle == "REDISEÑANDO...");

  Color accent =
      locked ? Color{60, 60, 60, 180} : m_characters[idx].accentColor;

  // Fondo de la tarjeta
  Color bg = locked ? Color{20, 20, 30, 200} : Color{25, 15, 40, 230};
  if (isCursor && !locked) {
    bg = ColorBrightness(bg, 0.35f);
  }
  DrawRectangle((int)x, (int)y, (int)w, (int)h, bg);

  // Borde (pulso animado cuando es cursor activo)
  float thickness = isCursor ? 3.0f : 1.5f;
  float alpha = isCursor ? (0.6f + 0.4f * sinf(m_cursorPulse)) : 0.4f;
  DrawRectangleLinesEx({x, y, w, h}, thickness, Fade(accent, alpha));

  if (isSelected) {
    DrawRectangleLinesEx({x - 4, y - 4, w + 8, h + 8}, 2.0f, Fade(GOLD, 0.9f));
  }

  if (locked) {
    // Slot bloqueado: icono "?"
    DrawText("?", (int)(x + w * 0.5f) - 16, (int)(y + h * 0.35f), 48,
             Fade(GRAY, 0.4f));
    DrawText("PRONTO", (int)(x + w * 0.5f) - MeasureText("PRONTO", 14) / 2,
             (int)(y + h * 0.72f), 14, Fade(GRAY, 0.4f));
    return;
  }

  const auto &ch = m_characters[idx];

  // Icono del personaje (círculo temático)
  float iconX = x + w * 0.5f;
  float iconY = y + h * 0.38f;
  float iconR = w * 0.25f;
  DrawCircleGradient((int)iconX, (int)iconY, iconR, Fade(accent, 0.9f),
                     Fade({0, 0, 0, 0}, 0.0f));
  DrawCircleV({iconX, iconY}, iconR * 0.65f, accent);
  DrawCircleLines((int)iconX, (int)iconY, iconR * 0.78f, Fade(WHITE, 0.4f));

  // Nombre
  DrawText(ch.name.c_str(),
           (int)(x + w * 0.5f) - MeasureText(ch.name.c_str(), 18) / 2,
           (int)(y + h * 0.7f), 18, WHITE);

  // Subtítulo
  DrawText(ch.subtitle.c_str(),
           (int)(x + w * 0.5f) - MeasureText(ch.subtitle.c_str(), 13) / 2,
           (int)(y + h * 0.84f), 13, Fade(LIGHTGRAY, 0.8f));

  // Indicador de cursor seleccionado
  if (isCursor) {
    DrawText("▼", (int)(x + w * 0.5f) - 8, (int)(y + h - 18), 16,
             Fade(accent, 0.7f + 0.3f * sinf(m_cursorPulse)));
  }
}

} // namespace Scenes
