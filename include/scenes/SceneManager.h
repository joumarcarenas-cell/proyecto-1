#pragma once

// =====================================================================
// SceneManager.h - Singleton Gestor de Escenas
// =====================================================================
// Maneja la escena activa y las transiciones con Fade In/Out usando
// unique_ptr para garantizar que no haya fugas de memoria.
//
// INTEGRACION EN main.cpp (sin cambios necesarios):
//
//   while (!WindowShouldClose()) {
//       float dt = GetFrameTime();
//       SceneManager::Get().Update(dt);
//       BeginDrawing();
//           ClearBackground({25, 30, 40, 255});
//           SceneManager::Get().Draw();   // fade overlay incluido aqui
//       EndDrawing();
//   }
//   SceneManager::Get().Shutdown();
//
// Para cambiar de escena desde cualquier lugar:
//   SceneManager::Get().ChangeScene(std::make_unique<GameplayScene>(...));
// =====================================================================

#include "Scene.h"
#include <memory>
#include <raylib.h>

namespace Scenes {

class SceneManager {
public:
    static SceneManager& Get() {
        static SceneManager instance;
        return instance;
    }

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // ── Cambio de escena con Fade Out -> Swap -> Fade In ─────────────
    // La pantalla se oscurece, se hace el swap cuando esta completamente
    // negra, y luego regresa la imagen. Las escenas no saben nada del fade.
    void ChangeScene(std::unique_ptr<Scene> newScene) {
        if (m_fadeState != FadeState::NONE) {
            // Ya hay un fade en curso: empalar el pedido (el ultimo gana)
            m_pendingScene = std::move(newScene);
            return;
        }
        m_pendingScene = std::move(newScene);
        m_fadeState    = FadeState::FADE_OUT;
        m_fadeAlpha    = 0.0f;
    }

    // Overlay de pausa: SIN fade (la pausa debe ser instantanea)
    void PushOverlay(std::unique_ptr<Scene> overlay) {
        if (m_overlay) m_overlay->Unload();
        m_overlay = std::move(overlay);
        if (m_overlay) m_overlay->Init();
    }

    void PopOverlay() {
        if (m_overlay) {
            m_overlay->Unload();
            m_overlay.reset();
        }
    }

    bool HasOverlay() const { return m_overlay != nullptr; }
    bool IsFading()   const { return m_fadeState != FadeState::NONE; }

    // ── Update ────────────────────────────────────────────────────────
    void Update(float dt) {
        UpdateFade(dt);

        if (m_overlay) {
            m_overlay->Update(dt);
        } else if (m_currentScene) {
            // Congelar el mundo durante FADE_OUT (evita que el juego avance
            // mientras la camara esta entrando en negro)
            if (m_fadeState != FadeState::FADE_OUT)
                m_currentScene->Update(dt);
        }
    }

    // ── Draw ──────────────────────────────────────────────────────────
    void Draw() {
        if (m_currentScene) m_currentScene->Draw();
        if (m_overlay)      m_overlay->Draw();
        DrawFadeOverlay();   // siempre la ultima capa, encima de todo
    }

    // ── Shutdown ──────────────────────────────────────────────────────
    void Shutdown() {
        PopOverlay();
        if (m_currentScene) {
            m_currentScene->Unload();
            m_currentScene.reset();
        }
    }

    void Quit() { m_shouldExit = true; }
    bool ShouldExit() const { return m_shouldExit; }

private:
    SceneManager() = default;

    // ── Maquina de fade ───────────────────────────────────────────────
    enum class FadeState { NONE, FADE_OUT, FADE_IN };

    static constexpr float FADE_DURATION = 0.30f; // segundos por direccion

    void UpdateFade(float dt) {
        if (m_fadeState == FadeState::FADE_OUT) {
            m_fadeAlpha += dt / FADE_DURATION;
            if (m_fadeAlpha >= 1.0f) {
                m_fadeAlpha = 1.0f;
                // Pantalla completamente negra: swap seguro de escena
                if (m_currentScene) m_currentScene->Unload();
                m_currentScene = std::move(m_pendingScene);
                if (m_currentScene) m_currentScene->Init();
                m_fadeState = FadeState::FADE_IN;
            }

        } else if (m_fadeState == FadeState::FADE_IN) {
            m_fadeAlpha -= dt / FADE_DURATION;
            if (m_fadeAlpha <= 0.0f) {
                m_fadeAlpha = 0.0f;
                m_fadeState = FadeState::NONE;
            }
        }
    }

    void DrawFadeOverlay() const {
        if (m_fadeAlpha > 0.0f)
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                          Fade(BLACK, m_fadeAlpha));
    }

    // ── Estado ────────────────────────────────────────────────────────
    std::unique_ptr<Scene> m_currentScene; // escena principal activa
    std::unique_ptr<Scene> m_overlay;      // pausa/settings (encima)
    std::unique_ptr<Scene> m_pendingScene; // esperando el swap tras fade
    FadeState m_fadeState = FadeState::NONE;
    float     m_fadeAlpha = 0.0f;
    bool      m_shouldExit = false;
};

} // namespace Scenes
