#pragma once

// =====================================================================
// SceneManager.h - Singleton Gestor de Escenas
// =====================================================================
// Maneja la escena activa y las transiciones entre escenas usando
// unique_ptr para garantizar que no haya fugas de memoria.
//
// INTEGRACIÓN EN main.cpp:
//
//   while (!WindowShouldClose()) {
//       float dt = GetFrameTime();
//       SceneManager::Get().Update(dt);
//       BeginDrawing();
//           ClearBackground({25, 30, 40, 255});
//           SceneManager::Get().Draw();
//       EndDrawing();
//   }
//   SceneManager::Get().Shutdown();
//
// Para cambiar de escena desde cualquier lugar del código:
//   SceneManager::Get().ChangeScene(std::make_unique<GameplayScene>(player, boss));
// =====================================================================

#include "Scene.h"
#include <memory>

namespace Scenes {

class SceneManager {
public:
    // Acceso global al singleton.
    static SceneManager& Get() {
        static SceneManager instance;
        return instance;
    }

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // Cambia a una nueva escena:
    // 1. Llama a Unload() de la escena anterior (libera sus recursos).
    // 2. El unique_ptr destruye automáticamente la escena anterior.
    // 3. Llama a Init() de la nueva escena.
    void ChangeScene(std::unique_ptr<Scene> newScene) {
        // Si hay escena activa, la descargamos limpiamente.
        if (m_currentScene) {
            m_currentScene->Unload();
        }
        // El move transfiere la propiedad; el destructor del unique_ptr
        // anterior libera la memoria en cuanto m_currentScene se reasigna.
        m_currentScene = std::move(newScene);
        if (m_currentScene) {
            m_currentScene->Init();
        }
    }

    // Permite superponer una escena de pausa SIN destruir la anterior.
    // La GameplayScene sigue en memoria; la PauseScene se dibuja encima.
    void PushOverlay(std::unique_ptr<Scene> overlay) {
        if (m_overlay) {
            m_overlay->Unload();
        }
        m_overlay = std::move(overlay);
        if (m_overlay) {
            m_overlay->Init();
        }
    }

    // Elimina el overlay de pausa y regresa a la escena base.
    void PopOverlay() {
        if (m_overlay) {
            m_overlay->Unload();
            m_overlay.reset();
        }
    }

    bool HasOverlay() const { return m_overlay != nullptr; }

    // Delegados del bucle principal.
    void Update(float dt) {
        if (m_overlay) {
            m_overlay->Update(dt);
        } else if (m_currentScene) {
            m_currentScene->Update(dt);
        }
    }

    void Draw() {
        // Siempre dibujamos la escena base (se ve debajo del overlay).
        if (m_currentScene) {
            m_currentScene->Draw();
        }
        // Si existe un overlay (ej: PauseScene), se dibuja encima.
        if (m_overlay) {
            m_overlay->Draw();
        }
    }

    // Limpieza final al cerrar la ventana.
    void Shutdown() {
        PopOverlay();
        if (m_currentScene) {
            m_currentScene->Unload();
            m_currentScene.reset();
        }
    }

    // Gestionar la salida del juego
    void Quit() { m_shouldExit = true; }
    bool ShouldExit() const { return m_shouldExit; }

private:
    SceneManager() = default;

    std::unique_ptr<Scene> m_currentScene; // Escena principal activa
    std::unique_ptr<Scene> m_overlay;     // Escena superpuesta (pausa, etc.)
    bool m_shouldExit = false;
};

} // namespace Scenes
