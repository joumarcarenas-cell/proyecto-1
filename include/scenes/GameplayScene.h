#pragma once

// =====================================================================
// GameplayScene.h - Escena Principal de Combate
// =====================================================================
// Contiene toda la lógica que actualmente vive en el switch-case
// GamePhase::RUNNING de main.cpp, encapsulada limpiamente.
// =====================================================================

#include "Scene.h"
#include "../../entities.h"
#include "../graphics/VFXSystem.h"
#include <raylib.h>
#include <vector>

namespace Scenes {

class GameplayScene : public Scene {
public:
    // Recibe referencias a los objetos de juego que viven en main.
    GameplayScene(Player*& activePlayer, Enemy& boss)
        : m_activePlayer(activePlayer), m_boss(boss) {}

    void Init()           override;
    void Update(float dt) override;
    void Draw()           override;
    void Unload()         override;

    // Acceso público para que PauseScene pueda leer la cámara si necesita
    // dibujar el mundo de fondo desenfocado.
    const Camera2D& GetCamera() const { return m_camera; }

private:
    Player*& m_activePlayer;
    Enemy&   m_boss;

    Camera2D m_camera{};

    // Vida fantasma (HUD con lerp)
    float m_playerGhostHp = 0.0f;
    float m_bossGhostHp   = 0.0f;

    // Textos de daño flotantes
    std::vector<DamageText> m_damageTexts;

    // ─── Helpers ───────────────────────────────────────────────────
    void UpdateCamera(float dt);
    void UpdateBleedDoT(float dt);
    void UpdateRocks(float dt);
    void UpdateCollisions();
    void UpdateDeathCheck();

    void DrawWorld();      // 2D isométrico (BeginMode2D / EndMode2D)
    void DrawHUD();        // HUD superpuesto en coordenadas de pantalla
    void DrawArena();      // Suelo + paredes del diamante isométrico

    static void DrawQuad(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, Color c);
};

} // namespace Scenes
