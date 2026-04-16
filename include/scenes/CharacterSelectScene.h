#pragma once

// =====================================================================
// CharacterSelectScene.h - Selección de Personaje (estilo MK)
// =====================================================================
// Cuadrícula 2D con cursor navegable por teclado/WASD.
// Soporta 2 jugadores con cursores independientes.
// =====================================================================

#include "../../entities.h"
#include "../ElementalMage.h"
#include "Scene.h"
#include <raylib.h>
#include <string>
#include <vector>

namespace Scenes {

// Descriptor de un slot de personaje en la cuadrícula.
struct CharacterEntry {
  std::string name;     // Nombre a mostrar
  std::string subtitle; // Subtítulo (clase/rol)
  Color accentColor;    // Color temático del personaje
};

class CharacterSelectScene : public Scene {
public:
  // Referencia a los punteros de jugador activo para asignar al confirmar.
  // Los personajes reales viven en main y se pasan aquí.
  CharacterSelectScene(Reaper &reaper, Ropera &ropera, ElementalMage &mage, Player *&activePlayer)
      : m_reaper(reaper), m_ropera(ropera), m_mage(mage), m_activePlayer(activePlayer) {}

  void Init() override;
  void Update(float dt) override;
  void Draw() override;
  void Unload() override;

private:
  static constexpr int COLS = 4; // Columnas de la cuadrícula
  static constexpr int ROWS = 2; // Filas de la cuadrícula

  // Personajes disponibles (índice en la cuadrícula)
  std::vector<CharacterEntry> m_characters;

  // Cursor del jugador 1 (WASD / Flechas)
  int m_cursorCol = 0;
  int m_cursorRow = 0;
  int m_selectedIdx = -1; // -1 = sin confirmar

  // Referencias a instancias reales
  Reaper &m_reaper;
  Ropera &m_ropera;
  ElementalMage &m_mage;
  Player *&m_activePlayer;

  // Animación del cursor
  float m_cursorPulse = 0.0f;

  // Helpers
  int CursorIndex() const { return m_cursorRow * COLS + m_cursorCol; }
  void DrawCharacterCard(int idx, float x, float y, float w, float h,
                         bool isCursor, bool isSelected) const;
};

} // namespace Scenes
