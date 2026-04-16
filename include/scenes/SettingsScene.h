#pragma once

// =====================================================================
// SettingsScene.h - Menú de Ajustes (Resolución, Controles, Fullscreen)
// =====================================================================

#include "../../entities.h"
#include "Scene.h"
#include <functional>
#include <raylib.h>
#include <string>

namespace Scenes {

class SettingsScene : public Scene {
public:
  // origin: escena a la que se regresa al presionar "Volver"
  // activePlayer: referencia para poder reasignar teclas de control
  // onBack: callback que el caller ejecuta para saber cuándo regresar
  SettingsScene(Player *&activePlayer, std::function<void()> onBack)
      : m_activePlayer(activePlayer), m_onBack(std::move(onBack)) {}

  void Init() override;
  void Update(float dt) override;
  void Draw() override;
  void Unload() override;

private:
  Player *&m_activePlayer;
  std::function<void()> m_onBack; // Callback al presionar "Volver"

  // Rebinding state
  int *m_rebindingKey = nullptr;
  std::string m_rebindingName = "";
  bool m_isRebinding = false;

  // Botones de resolución
  Rectangle m_btnBack{};
  Rectangle m_btnRes720{};
  Rectangle m_btnRes900{};
  Rectangle m_btnRes1080{};
  Rectangle m_btnFullscreen{};
  Rectangle m_btnCursorToggle{}; // Nueva: Botón para alternar cursor

  // Grading buttons
  Rectangle m_btnSatUp{}, m_btnSatDown{};
  Rectangle m_btnConUp{}, m_btnConDown{};
  Rectangle m_btnExpUp{}, m_btnExpDown{};

  // Helper: dibuja y gestiona un botón de reasignación de tecla
  void DrawRebindRow(const char *label, int *key, int y);
  // Helper: dibuja una fila de ajuste numérico (Granding)
  void DrawGradingRow(const char* label, float* value, float step, Rectangle btnUp, Rectangle btnDown, int y);
};

} // namespace Scenes
