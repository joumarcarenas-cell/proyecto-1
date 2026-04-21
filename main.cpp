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
#include "include/ElementalMage.h"
#include "include/graphics/VFXSystem.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

// Escenas
#include "include/scenes/CharacterSelectScene.h"
#include "include/scenes/GameplayScene.h"
#include "include/scenes/MainMenuScene.h"
#include "include/scenes/PauseScene.h"
#include "include/scenes/SceneManager.h"
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
Reaper g_reaper({2000, 2000});
Ropera g_ropera({2000, 2000});
ElementalMage g_mage({2000, 2000});
Player *g_activePlayer = &g_reaper; // Apuntado por las escenas

// Variables de estado compartidas entre Reaper.cpp / GameplayScene.cpp
bool isTimeStopped = false;
bool showCursorInGame = true; // Controla si el cursor se ve en partida
float hitstopTimer = 0.0f;
float screenShake = 0.0f; 
float g_timeScale = 1.0f; 
double g_gameTime = 0.0;  
bool g_showHitboxes = true;

// Información de versión para verificación (F5)
const char* G_BUILD_VERSION = "v1.2.4 - " __DATE__ " " __TIME__;

// =====================================================================
// Helper para cargar GIFs como SpriteSheets universales
// =====================================================================
Texture2D LoadGifAsSpritesheet(const char* fileName, int* outFrames) {
    Image anim = LoadImageAnim(fileName, outFrames);
    if (anim.data == nullptr || *outFrames <= 0) return Texture2D{0};
    
    Image atlas = GenImageColor(anim.width * (*outFrames), anim.height, BLANK);
    for (int i = 0; i < *outFrames; i++) {
        Image frameImg = anim;
        frameImg.data = ((unsigned char*)anim.data) + (anim.width * anim.height * 4 * i);
        ImageDraw(&atlas, frameImg, 
                 Rectangle{0, 0, (float)anim.width, (float)anim.height}, 
                 Rectangle{(float)(anim.width * i), 0, (float)anim.width, (float)anim.height}, 
                 WHITE);
    }
    Texture2D tex = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    UnloadImage(anim);
    return tex;
}

// =====================================================================
// IMPLEMENTACIÓN DEL RESOURCE MANAGER (debe estar en un .cpp)
// =====================================================================
Texture2D ResourceManager::texVida;
Texture2D ResourceManager::texEnergia;
Texture2D ResourceManager::texBerserker;
Texture2D ResourceManager::texBoomerang;
Texture2D ResourceManager::texUltimate;
Texture2D ResourceManager::texSuelo;
Texture2D ResourceManager::texPared;
Texture2D ResourceManager::texPasto;
Texture2D ResourceManager::texPlayer;
Texture2D ResourceManager::texEnemy;
int ResourceManager::texEnemyFrames = 0;
Texture2D ResourceManager::texEnemyGolem;
Texture2D ResourceManager::roperaIdle;
Texture2D ResourceManager::roperaRun;
Texture2D ResourceManager::roperaDash;
Texture2D ResourceManager::roperaAttack1;
Texture2D ResourceManager::roperaAttack3;
Texture2D ResourceManager::roperaHeavy;
Texture2D ResourceManager::ropera8Idle;
Texture2D ResourceManager::ropera8Run;
Texture2D ResourceManager::ropera8Attack;
Texture2D ResourceManager::roperaHit;
Texture2D ResourceManager::roperaDeath;
Texture2D ResourceManager::roperaTajoDoble;

Texture2D ResourceManager::texPropsRocks;
Texture2D ResourceManager::texPropsFoliage;

int ResourceManager::roperaTajoFrames = 0;

int ResourceManager::roperaIdleFrames = 0;
int ResourceManager::roperaRunFrames = 0;
int ResourceManager::roperaDashFrames = 0;
int ResourceManager::roperaAttack1Frames = 0;
int ResourceManager::roperaAttack3Frames = 0;
int ResourceManager::roperaHeavyFrames = 0;

Texture2D ResourceManager::reaperQ;
Texture2D ResourceManager::reaperE;
Texture2D ResourceManager::reaperR;

Texture2D ResourceManager::texVfxSpark;
Texture2D ResourceManager::texVfxSmoke;
Texture2D ResourceManager::texVfxSplash;
Texture2D ResourceManager::texVfxGlow;

void ResourceManager::Load() {
  texVida = LoadTexture("assets/vida.png");
  texEnergia = LoadTexture("assets/energia.png");
  texBerserker = LoadTexture("assets/berserker.png");
  texBoomerang = LoadTexture("assets/boomerang.png");
  texUltimate = LoadTexture("assets/ultimate.png");

  texSuelo = LoadTexture("assets/suelo.png");
  SetTextureWrap(texSuelo, TEXTURE_WRAP_REPEAT);
  texPared = LoadTexture("assets/pared.png");
  SetTextureWrap(texPared, TEXTURE_WRAP_REPEAT);

  // Tile de pasto isométrico: WRAP_REPEAT evita líneas en los bordes del tile
  texPasto = LoadTexture("assets/pasto.jpg");
  SetTextureFilter(texPasto, TEXTURE_FILTER_BILINEAR); // suavizado en escala

  texPlayer = LoadTexture("assets/player.png");

  texEnemy = LoadGifAsSpritesheet("assets/golem.gif", &texEnemyFrames);
  texEnemyGolem = texEnemy; // maintaining legacy reference if needed

  roperaIdle = LoadGifAsSpritesheet("assets/Ropera/idle.gif", &roperaIdleFrames);
  roperaRun = LoadGifAsSpritesheet("assets/Ropera/run.gif", &roperaRunFrames);
  roperaDash = LoadGifAsSpritesheet("assets/Ropera/dash.gif", &roperaDashFrames);
  roperaAttack1 = LoadGifAsSpritesheet("assets/Ropera/attack 1.gif", &roperaAttack1Frames);
  roperaAttack3 = LoadGifAsSpritesheet("assets/Ropera/attack 2.gif", &roperaAttack3Frames);
  roperaHeavy = LoadGifAsSpritesheet("assets/Ropera/charge attack.gif", &roperaHeavyFrames);

  roperaHit = LoadTexture("assets/Ropera/14 - hit.png");
  roperaDeath = LoadTexture("assets/Ropera/15 - death.png");

  roperaTajoDoble = LoadGifAsSpritesheet("assets/Ropera/habilidades/tajo doble.gif", &roperaTajoFrames);

  // Reaper animations / icons
  reaperQ = LoadTexture("assets/Reaper/habilidad Q.png");
  reaperE = LoadTexture("assets/Reaper/habilidad E.png");
  reaperR = LoadTexture("assets/Reaper/habilidad R.png");

  // VFX Assets - Usan subcarpeta assets/vfx/
  texVfxSpark = LoadTexture("assets/vfx/spark.png");
  texVfxSmoke = LoadTexture("assets/vfx/smoke.png");
  texVfxSplash = LoadTexture("assets/vfx/splash.png");
  texVfxGlow = LoadTexture("assets/vfx/glow.png");

  // Ropera 8-Direction Sheets
  ropera8Idle = LoadTexture("assets/Ropera/ropera.png");
  ropera8Run = LoadTexture("assets/Ropera/ropera_run.png");
  ropera8Attack = LoadTexture("assets/Ropera/ropera_attack.png");

  // Props
  texPropsRocks = LoadTexture("assets/props/rocks.png");
  texPropsFoliage = LoadTexture("assets/props/foliage.png");
}

void ResourceManager::Unload() {
  UnloadTexture(texVida);
  UnloadTexture(texEnergia);
  UnloadTexture(texBerserker);
  UnloadTexture(texBoomerang);
  UnloadTexture(texUltimate);
  UnloadTexture(texSuelo);
  UnloadTexture(texPared);
  UnloadTexture(texPasto);
  UnloadTexture(texPlayer);
  UnloadTexture(texEnemy);
  if (texEnemyGolem.id != texEnemy.id)
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

  UnloadTexture(reaperQ);
  UnloadTexture(reaperE);
  UnloadTexture(reaperR);

  UnloadTexture(texVfxSpark);
  UnloadTexture(texVfxSmoke);
  UnloadTexture(texVfxSplash);
  UnloadTexture(texVfxGlow);

  UnloadTexture(ropera8Idle);
  UnloadTexture(ropera8Run);
  UnloadTexture(ropera8Attack);

  UnloadTexture(texPropsRocks);
  UnloadTexture(texPropsFoliage);
}

// =====================================================================
// MAIN
// =====================================================================
int main() {
  InitWindow(1920, 1080, "Proyecto 1 - Scene Manager");
  SetTargetFPS(60);

  // Cargar todos los assets de GPU (texturas, etc.)
  ResourceManager::Load();

  // ─── INICIO DEL JUEGO: arrancar en MainMenuScene ─────────────────
  Scenes::SceneManager::Get().ChangeScene(
      std::make_unique<Scenes::MainMenuScene>());

  // ─── BUCLE PRINCIPAL ─────────────────────────────────────────────
  // El SceneManager se encarga de delegar Update y Draw a la escena activa
  // (y al overlay de pausa si existe). Aquí solo gestionamos lo indispensable.
  while (!WindowShouldClose() && !Scenes::SceneManager::Get().ShouldExit()) {
    float dt = GetFrameTime() * g_timeScale;
    g_gameTime += (double)dt;

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