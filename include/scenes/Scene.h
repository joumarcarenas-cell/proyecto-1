#pragma once

// =====================================================================
// Scene.h - Clase Base Abstracta para todas las Escenas
// =====================================================================
// Toda escena del juego hereda de esta clase e implementa sus 4 métodos
// virtuales: Init, Update, Draw y Unload.
// =====================================================================

namespace Scenes {

class Scene {
public:
  virtual ~Scene() = default;

  // Llamado UNA VEZ al entrar a la escena (carga de recursos, reset de estado).
  virtual void Init() = 0;

  // Lógica de actualización por frame (input, física, IA).
  virtual void Update(float dt) = 0;

  // Renderizado por frame (siempre dentro de BeginDrawing/EndDrawing).
  virtual void Draw() = 0;

  // Llamado UNA VEZ al salir de la escena (libera recursos propios).
  virtual void Unload() = 0;
};

} // namespace Scenes
