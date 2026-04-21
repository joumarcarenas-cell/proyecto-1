// =========================================================================
// IsoMap.h  —  Sistema de Mapa de Suelo Isométrico con Tiles de Pasto
// =========================================================================
//
// FÓRMULAS DE CONVERSIÓN ISOMÉTRICA:
//   screenX = (col - row) * (TILE_W / 2) + offsetX
//   screenY = (col + row) * (TILE_H / 2) + offsetY
//
// USO:
//   // En Init():
//   IsoMap::InitDefaultMap(myMap);
//
//   // En Draw() (dentro de BeginMode2D / EndMode2D):
//   Vector2 camOffset = { ... };
//   IsoMap::DrawIsoMap(myMap, ResourceManager::texPasto, camOffset);
//
// =========================================================================
#pragma once

#include <array>
#include <cstdint>
#include <raylib.h>

// ── Constantes de tile ────────────────────────────────────────────────────
// Ajusta estos valores para que coincidan con las dimensiones reales
// del sprite de pasto (diamond / rombo isométrico).
namespace IsoMap {

constexpr int MAP_COLS = 30;     // Número de columnas de la matriz
constexpr int MAP_ROWS = 30;     // Número de filas   de la matriz
constexpr float TILE_W = 140.0f; // Ancho  del tile
constexpr float TILE_H = 70.0f;  // Alto   del tile

// ── Tipo de celda del mapa ─────────────────────────────────────────────
// 0 = vacío (no se renderiza), 1 = pasto, 2+ = reservado para
// otros tipos de suelo en el futuro.
using TileID = uint8_t;

// ── Definición del mapa ────────────────────────────────────────────────
struct Map {
  std::array<std::array<TileID, MAP_COLS>, MAP_ROWS> cells{};
  // Tono de color global aplicado a todos los tiles (WHITE = sin tinte)
  Color tint = WHITE;
};

// ── Inicializa el mapa con pasto sólido (todas las celdas = 1) ─────────
inline void InitDefaultMap(Map &map) {
  for (auto &row : map.cells)
    row.fill(1);
  map.tint = WHITE;
}

// ── Convierte coordenadas de matriz (col, row) a posición de pantalla ──
// offset: desplazamiento de cámara / centrado en pantalla
inline Vector2 IsoToScreen(int col, int row, Vector2 offset) {
  return {(col - row) * (TILE_W * 0.5f) + offset.x,
          (col + row) * (TILE_H * 0.5f) + offset.y};
}

// ── Función principal de renderizado ───────────────────────────────────
// Dibuja el mapa isométrico completo usando la textura indicada.
//
// Parámetros:
//   map      — Datos del mapa (celdas TileID).
//   texPasto — Textura de pasto cargada previamente (rombo / diamond).
//   offset   — Desplazamiento de pantalla para centrar o seguir al jugador.
//
// Rendimiento:
//   • Itera filas-externas / columnas-internas para respetar el orden Z
//     isométrico natural (fila 0 queda al frente, fila N al fondo).
//   • Solo llama a DrawTexturePro cuando TileID != 0 (evita draws vacíos).
//   • sourceRec y destRec se calculan una sola vez por tile.
void DrawIsoMap(const Map &map, const Texture2D &texPasto, Vector2 offset);

// Helper procedimental para dibujar enredaderas en las paredes (usado por DrawArena)
void DrawVines(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, float seed);

} // namespace IsoMap
