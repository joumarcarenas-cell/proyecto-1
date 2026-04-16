// =========================================================================
// IsoMap.cpp  —  Implementación del renderizado isométrico con pasto
// =========================================================================
#include "include/IsoMap.h"
#include "rlgl.h"
#include <raylib.h>

namespace IsoMap {

void DrawIsoMap(const Map &map, const Texture2D &texPasto, Vector2 offset) {
  if (texPasto.id == 0)
    return;

  const float destW = TILE_W;
  const float destH = TILE_H;

  // Activamos la textura una sola vez para toda la cuadrícula (muy rápido)
  rlSetTexture(texPasto.id);
  rlBegin(RL_QUADS);
  rlColor4ub(map.tint.r, map.tint.g, map.tint.b, map.tint.a);

  // Iteramos celda por celda
  for (int row = 0; row < MAP_ROWS; ++row) {
    for (int col = 0; col < MAP_COLS; ++col) {

      if (map.cells[row][col] == 0)
        continue;

      // screenPos es la punta NORTE (Top) del rombo
      const Vector2 screenPos = IsoToScreen(col, row, offset);

      // Mapeamos los bordes de la textura cuadrada a las puntas del rombo
      // isométrico. Orden Anti-horario (Top -> Left -> Bottom -> Right).
      // Esto "recorta" visualmente el JPG forzándolo a la forma isométrica.

      // 1. Top Vertex
      rlTexCoord2f(0.0f, 0.0f);
      rlVertex2f(screenPos.x, screenPos.y);

      // 2. Left Vertex
      rlTexCoord2f(0.0f, 1.0f);
      rlVertex2f(screenPos.x - destW * 0.5f, screenPos.y + destH * 0.5f);

      // 3. Bottom Vertex
      rlTexCoord2f(1.0f, 1.0f);
      rlVertex2f(screenPos.x, screenPos.y + destH);

      // 4. Right Vertex
      rlTexCoord2f(1.0f, 0.0f);
      rlVertex2f(screenPos.x + destW * 0.5f, screenPos.y + destH * 0.5f);
    }
  }

  rlEnd();
  rlSetTexture(0);
}

} // namespace IsoMap
