#pragma once
#include <raylib.h>

struct ResourceManager {
    static Texture2D texVida;
    static Texture2D texEnergia;
    static Texture2D texBerserker;
    static Texture2D texBoomerang;
    static Texture2D texUltimate;
    
    // --- SPRITES DE ENTIDADES ---
    static Texture2D texPlayer;
    static Texture2D texEnemy;
    static Texture2D texEnemyGolem; // para el boss
    
    // Ropera animations (GIF based)
    static Texture2D roperaIdle;     static Image roperaIdleIm;     static int roperaIdleFrames;
    static Texture2D roperaRun;      static Image roperaRunIm;      static int roperaRunFrames;
    static Texture2D roperaDash;     static Image roperaDashIm;     static int roperaDashFrames;
    static Texture2D roperaAttack1;  static Image roperaAttack1Im;  static int roperaAttack1Frames;
    static Texture2D roperaAttack3;  static Image roperaAttack3Im;  static int roperaAttack3Frames;
    static Texture2D roperaHeavy;    static Image roperaHeavyIm;    static int roperaHeavyFrames;
    static Texture2D roperaHit;
    static Texture2D roperaDeath;
    static Texture2D roperaTajoDoble; static Image roperaTajoDobleIm; static int roperaTajoFrames;

    // Reaper animations / icons
    static Texture2D reaperQ;
    static Texture2D reaperE;
    static Texture2D reaperR;

    static void Load();
    static void Unload();
};
