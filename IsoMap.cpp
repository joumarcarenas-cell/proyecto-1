// =========================================================================
// IsoMap.cpp  —  Implementación del renderizado isométrico con profundidad
// =========================================================================
#include "include/IsoMap.h"
#include "include/ResourceManager.h"
#include "include/graphics/AnimeVFX.h"
#include <raylib.h>
#include <vector>
#include <cmath>

namespace IsoMap {

// Helper procedimental para dibujar enredaderas en las paredes
void DrawVines(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, float seed) {
    // Obtener fuerza de viento global
    Vector2 wind = AnimeVFX::AmbientSystem::Get().GetWindForce();
    float time = (float)GetTime();

    // Dibujamos pequeños racimos de hojas verdes aleatorios
    int clusters = 4 + (int)(seed * 8) % 5;
    for (int i = 0; i < clusters; i++) {
        float tx = ((float)((int)(seed * (i + 1) * 123) % 100)) / 100.0f;
        float ty = ((float)((int)(seed * (i + 1) * 456) % 100)) / 100.0f;
        
        // Interpolación bilineal simple en el cuadrilátero de la pared
        Vector2 top = Vector2Lerp(p1, p2, tx);
        Vector2 bot = Vector2Lerp(p4, p3, tx);
        Vector2 basePos = Vector2Lerp(top, bot, ty);
        
        // --- Swayer de viento ---
        // Las enredaderas mas altas (ty cerca de 0) se mueven mas
        float swayIntensity = (1.0f - ty) * 0.8f;
        float swayX = wind.x * 0.15f * swayIntensity + sinf(time * 2.0f + seed * i) * 3.0f * swayIntensity;
        float swayY = wind.y * 0.10f * swayIntensity + cosf(time * 1.5f + seed * i) * 2.0f * swayIntensity;
        Vector2 pos = { basePos.x + swayX, basePos.y + swayY };

        float size = 4.0f + (float)((int)(seed * i) % 6);
        Color vineCol = { 30, (unsigned char)(100 + (i * 20) % 80), 40, 255 };
        DrawCircleV(pos, size, vineCol);
        
        // Unir con una "rama" pequeña
        if (i > 0) {
           float offsetFactor = fmaxf(0.0f, ty - 0.2f);
           DrawLineEx(pos, Vector2Lerp(top, bot, offsetFactor), 1.5f, Fade(vineCol, 0.6f));
        }
    }
}

// Helper para dibujar muros texturizados usando rlgl
void DrawWallQuad(Texture2D tex, Vector2 v1, Vector2 v2, Vector2 v3, Vector2 v4, Color tint) {
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        rlTexCoord2f(0.0f, 1.0f); rlVertex2f(v1.x, v1.y);
        rlTexCoord2f(1.0f, 1.0f); rlVertex2f(v2.x, v2.y);
        rlTexCoord2f(1.0f, 0.0f); rlVertex2f(v3.x, v3.y);
        rlTexCoord2f(0.0f, 0.0f); rlVertex2f(v4.x, v4.y);
    rlEnd();
    rlSetTexture(0);
}

void DrawIsoMap(const Map &map, const Texture2D &texPasto, Vector2 offset) {
  if (texPasto.id == 0)
    return;

  const float destW = TILE_W;
  const float destH = TILE_H;
  const float depth = 28.0f; // Espesor del piso

  // ─── 1. DIBUJAR ESPESOR DEL PISO (CAPA DE TIERRA/PROFUNDIDAD) ───
  rlSetTexture(0); // Sin textura para el borde "sucio"
  rlBegin(RL_QUADS);
  for (int row = 0; row < MAP_ROWS; ++row) {
    for (int col = 0; col < MAP_COLS; ++col) {
      if (map.cells[row][col] == 0) continue;
      Vector2 sp = IsoToScreen(col, row, offset);
      
      // Solo dibujamos los bordes que miran al jugador (Sur-Este y Sur-Oeste)
      // Caras verticales de profundidad
      Color dirtCol = { 80, 60, 50, 255 };
      rlColor4ub(dirtCol.r, dirtCol.g, dirtCol.b, dirtCol.a);
      
      // Cara SE
      rlVertex2f(sp.x + destW * 0.5f, sp.y + destH * 0.5f);
      rlVertex2f(sp.x, sp.y + destH);
      rlVertex2f(sp.x, sp.y + destH + depth);
      rlVertex2f(sp.x + destW * 0.5f, sp.y + destH * 0.5f + depth);
      
      // Cara SO
      rlColor4ub(dirtCol.r - 10, dirtCol.g - 10, dirtCol.b - 10, dirtCol.a);
      rlVertex2f(sp.x, sp.y + destH);
      rlVertex2f(sp.x - destW * 0.5f, sp.y + destH * 0.5f);
      rlVertex2f(sp.x - destW * 0.5f, sp.y + destH * 0.5f + depth);
      rlVertex2f(sp.x, sp.y + destH + depth);
    }
  }
  rlEnd();

  // ─── 2. DIBUJAR PASTO (TOP LAYER) ───
  rlSetTexture(texPasto.id);
  rlBegin(RL_QUADS);
  rlColor4ub(map.tint.r, map.tint.g, map.tint.b, map.tint.a);
  for (int row = 0; row < MAP_ROWS; ++row) {
    for (int col = 0; col < MAP_COLS; ++col) {
      if (map.cells[row][col] == 0) continue;
      Vector2 screenPos = IsoToScreen(col, row, offset);
      rlTexCoord2f(0.0f, 0.0f); rlVertex2f(screenPos.x, screenPos.y);
      rlTexCoord2f(0.0f, 1.0f); rlVertex2f(screenPos.x - destW * 0.5f, screenPos.y + destH * 0.5f);
      rlTexCoord2f(1.0f, 1.0f); rlVertex2f(screenPos.x, screenPos.y + destH);
      rlTexCoord2f(1.0f, 0.0f); rlVertex2f(screenPos.x + destW * 0.5f, screenPos.y + destH * 0.5f);
    }
  }
  rlEnd();
  rlSetTexture(0);
}

} // namespace IsoMap
