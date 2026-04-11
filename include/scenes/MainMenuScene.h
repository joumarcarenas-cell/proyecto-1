#pragma once

// =====================================================================
// MainMenuScene.h - Menú Principal del Juego
// =====================================================================

#include "Scene.h"
#include <raylib.h>

namespace Scenes {

class MainMenuScene : public Scene {
public:
    void Init()           override;
    void Update(float dt) override;
    void Draw()           override;
    void Unload()         override;

private:
    // Botones del menú (Rectangles para hit-testing con el mouse)
    Rectangle m_btnPlay{};
    Rectangle m_btnSettings{};
    Rectangle m_btnQuit{};

    // Animación de fondo
    float m_bgTime = 0.0f;

    // Helpers de dibujo
    void DrawButton(Rectangle btn, const char* label, Color baseColor, bool hovered) const;
};

} // namespace Scenes
