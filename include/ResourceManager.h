#pragma once
#include <raylib.h>

struct ResourceManager {
  static Texture2D texVida;
  static Texture2D texEnergia;
  static Texture2D texBerserker;
  static Texture2D texBoomerang;
  static Texture2D texUltimate;

  // --- ESCENARIO ---
  static Texture2D texSuelo;
  static Texture2D texPared;
  static Texture2D texPasto; // Tile de pasto para el mapa isométrico
  static Texture2D texPropsRocks;
  static Texture2D texPropsFoliage;

  // --- SPRITES DE ENTIDADES ---
  static Texture2D texPlayer;
  static Texture2D texEnemy;
  static int texEnemyFrames;
  static Texture2D texEnemyGolem; // para el boss

  // Ropera animations
  static Texture2D roperaIdle;
  static int roperaIdleFrames;
  static Texture2D roperaRun;
  static int roperaRunFrames;
  static Texture2D roperaDash;
  static int roperaDashFrames;
  static Texture2D roperaAttack1;
  static int roperaAttack1Frames;
  static Texture2D roperaAttack3;
  static int roperaAttack3Frames;
  static Texture2D roperaHeavy;
  static int roperaHeavyFrames;
  static Texture2D roperaHit;
  static Texture2D roperaDeath;
  static Texture2D roperaTajoDoble;
  static int roperaTajoFrames;

  // Ropera 8-Direction Sheets
  static Texture2D ropera8Idle;
  static Texture2D ropera8Run;
  static Texture2D ropera8Attack;

  // Reaper animations / icons
  static Texture2D reaperQ;
  static Texture2D reaperE;
  static Texture2D reaperR;

  // VFX Textures (Assets in assets/vfx/)
  static Texture2D texVfxSpark;
  static Texture2D texVfxSmoke;
  static Texture2D texVfxSplash;
  static Texture2D texVfxGlow;

  static void Load();
  static void Unload();
};
