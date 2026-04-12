// =====================================================================
// main.cpp  (Refactorizado con SceneManager)
// =====================================================================
//
// ARQUITECTURA RESULTANTE:
//   main.cpp
//   ├── Define las variables globales de juego (g_reaper, g_boss, etc.)
//   ├── Inicializa Raylib y ResourceManager
//   ├── Inicia la primera escena: MainMenuScene
//   └── Bucle principal: delega Update/Draw al SceneManager
//
//  Flujo de escenas:
//   MainMenuScene ──JUGAR──► CharacterSelectScene ──ELEGIR──► GameplayScene
//                │                                               │
//                └───AJUSTES──► SettingsScene ◄──PAUSA──────────┘
//                                                    (overlay PauseScene)
// =====================================================================

#include "entities.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "include/graphics/VFXSystem.h"

// Escenas
#include "include/scenes/SceneManager.h"
#include "include/scenes/MainMenuScene.h"
#include "include/scenes/CharacterSelectScene.h"
#include "include/scenes/GameplayScene.h"
#include "include/scenes/PauseScene.h"
#include "include/scenes/SettingsScene.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

// =====================================================================
// VARIABLES GLOBALES DE JUEGO
// Estas viven aquí para poder compartirse entre escenas vía extern.
// =====================================================================

// Instancias de personaje (persisten durante toda la ejecución).
Reaper  g_reaper({2000, 2000});
Ropera  g_ropera({2000, 2000});
Player* g_activePlayer = &g_reaper;   // Apuntado por las escenas

// Instancia del boss (persiste; se reinicia al cambiar de partida).
Enemy   g_boss({2300, 2000});

// Variables de estado compartidas entre Reaper.cpp / GameplayScene.cpp
bool  isTimeStopped = false;
bool  showCursorInGame = false; // Nueva: Controla si el cursor se ve en partida
float hitstopTimer  = 0.0f;
float screenShake   = 0.0f;          // También escrito por Enemy::UpdateAI

// =====================================================================
// IMPLEMENTACIÓN DEL RESOURCE MANAGER (debe estar en un .cpp)
// =====================================================================
Texture2D ResourceManager::texVida;
Texture2D ResourceManager::texEnergia;
Texture2D ResourceManager::texBerserker;
Texture2D ResourceManager::texBoomerang;
Texture2D ResourceManager::texUltimate;
Texture2D ResourceManager::texPlayer;
Texture2D ResourceManager::texEnemy;
Texture2D ResourceManager::texEnemyGolem;
Texture2D ResourceManager::roperaIdle;
Texture2D ResourceManager::roperaRun;
Texture2D ResourceManager::roperaDash;
Texture2D ResourceManager::roperaAttack1;
Texture2D ResourceManager::roperaAttack3;
Texture2D ResourceManager::roperaHeavy;
Texture2D ResourceManager::roperaHit;
Texture2D ResourceManager::roperaDeath;
Texture2D ResourceManager::roperaTajoDoble;
Image ResourceManager::roperaTajoDobleIm;
int ResourceManager::roperaTajoFrames = 0;

Image ResourceManager::roperaIdleIm;     int ResourceManager::roperaIdleFrames = 0;
Image ResourceManager::roperaRunIm;      int ResourceManager::roperaRunFrames = 0;
Image ResourceManager::roperaDashIm;     int ResourceManager::roperaDashFrames = 0;
Image ResourceManager::roperaAttack1Im;  int ResourceManager::roperaAttack1Frames = 0;
Image ResourceManager::roperaAttack3Im;  int ResourceManager::roperaAttack3Frames = 0;
Image ResourceManager::roperaHeavyIm;    int ResourceManager::roperaHeavyFrames = 0;

Texture2D ResourceManager::reaperQ;
Texture2D ResourceManager::reaperE;
Texture2D ResourceManager::reaperR;

void ResourceManager::Load() {
    texVida      = LoadTexture("assets/vida.png");
    texEnergia   = LoadTexture("assets/energia.png");
    texBerserker = LoadTexture("assets/berserker.png");
    texBoomerang = LoadTexture("assets/boomerang.png");
    texUltimate  = LoadTexture("assets/ultimate.png");
    texPlayer    = LoadTexture("assets/player.png");
    
    texEnemyGolem   = LoadTexture("assets/golem.png");
    
    roperaIdleIm    = LoadImageAnim("assets/Ropera/idle.gif", &roperaIdleFrames);
    roperaIdle      = LoadTextureFromImage(roperaIdleIm);
    roperaRunIm     = LoadImageAnim("assets/Ropera/run.gif", &roperaRunFrames);
    roperaRun       = LoadTextureFromImage(roperaRunIm);
    roperaDashIm    = LoadImageAnim("assets/Ropera/dash.gif", &roperaDashFrames);
    roperaDash      = LoadTextureFromImage(roperaDashIm);
    roperaAttack1Im = LoadImageAnim("assets/Ropera/attack 1.gif", &roperaAttack1Frames);
    roperaAttack1   = LoadTextureFromImage(roperaAttack1Im);
    roperaAttack3Im = LoadImageAnim("assets/Ropera/attack 2.gif", &roperaAttack3Frames);
    roperaAttack3   = LoadTextureFromImage(roperaAttack3Im);
    roperaHeavyIm   = LoadImageAnim("assets/Ropera/charge attack.gif", &roperaHeavyFrames);
    roperaHeavy     = LoadTextureFromImage(roperaHeavyIm);

    roperaHit       = LoadTexture("assets/Ropera/14 - hit.png");
    roperaDeath     = LoadTexture("assets/Ropera/15 - death.png");
    
    roperaTajoDobleIm = LoadImageAnim("assets/Ropera/habilidades/tajo doble.gif", &roperaTajoFrames);
    roperaTajoDoble = LoadTextureFromImage(roperaTajoDobleIm);

    reaperQ = LoadTexture("assets/Reaper/habilidad Q.png");
    reaperE = LoadTexture("assets/Reaper/habilidad E.png");
    reaperR = LoadTexture("assets/Reaper/habilidad R.png");
}

void ResourceManager::Unload() {
    UnloadTexture(texVida);
    UnloadTexture(texEnergia);
    UnloadTexture(texBerserker);
    UnloadTexture(texBoomerang);
    UnloadTexture(texUltimate);
    UnloadTexture(texPlayer);
    UnloadTexture(texEnemy);
    
    UnloadTexture(texEnemyGolem);
    UnloadTexture(roperaIdle);
    UnloadTexture(roperaRun);
    UnloadTexture(roperaDash);
    UnloadTexture(roperaAttack1);
    UnloadTexture(roperaAttack3);
    UnloadTexture(roperaHeavy);
    UnloadTexture(roperaHit);
    UnloadTexture(roperaDeath);
    UnloadTexture(roperaTajoDoble);
    UnloadImage(roperaTajoDobleIm);

    UnloadImage(roperaIdleIm);
    UnloadImage(roperaRunIm);
    UnloadImage(roperaDashIm);
    UnloadImage(roperaAttack1Im);
    UnloadImage(roperaAttack3Im);
    UnloadImage(roperaHeavyIm);

    UnloadTexture(reaperQ);
    UnloadTexture(reaperE);
    UnloadTexture(reaperR);
}

// =====================================================================
// MAIN
// =====================================================================
int main() {
    InitWindow(1920, 1080, "Proyecto 1 - Scene Manager");
    SetTargetFPS(60);

    // Configurar boss
    g_boss.maxHp = 2000.0f;
    g_boss.hp    = 2000.0f;

    // Cargar todos los assets de GPU (texturas, etc.)
    ResourceManager::Load();

    // ─── INICIO DEL JUEGO: arrancar en MainMenuScene ─────────────────
    Scenes::SceneManager::Get().ChangeScene(
        std::make_unique<Scenes::MainMenuScene>());

    // ─── BUCLE PRINCIPAL ─────────────────────────────────────────────
    // El SceneManager se encarga de delegar Update y Draw a la escena activa
    // (y al overlay de pausa si existe). Aquí solo gestionamos lo indispensable.
    while (!WindowShouldClose() && !Scenes::SceneManager::Get().ShouldExit()) {
        float dt = GetFrameTime();

        // 1. Actualizar la lógica de la escena activa (+ overlay si hay)
        Scenes::SceneManager::Get().Update(dt);

        // 2. Renderizar
        BeginDrawing();
            ClearBackground({22, 26, 36, 255});
            Scenes::SceneManager::Get().Draw();
        EndDrawing();
    }

    // ─── LIMPIEZA FINAL ───────────────────────────────────────────────
    // Shutdown descarga la escena activa y el overlay correctamente.
    Scenes::SceneManager::Get().Shutdown();
    ResourceManager::Unload();
    CloseWindow();
    return 0;
}