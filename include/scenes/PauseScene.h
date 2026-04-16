#pragma once

// =====================================================================
// PauseScene.h - Overlay de Pausa (NO descarga GameplayScene)
// =====================================================================
// Se activa con SceneManager::Get().PushOverlay(...)
// La GameplayScene permanece en memoria; esta se dibuja encima.
// =====================================================================

#include "Scene.h"
#include <raylib.h>

namespace Scenes {

class PauseScene : public Scene {
public:
  void Init() override;
  void Update(float dt) override;
  void Draw() override;
  void Unload() override;

private:
  Rectangle m_btnResume{};
  Rectangle m_btnSettings{};
  Rectangle m_btnMainMenu{};
};

} // namespace Scenes
