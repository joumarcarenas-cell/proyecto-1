#pragma once
#include <raylib.h>

class Entity {
public:
    Vector2 position;
    Vector2 velocity = {0, 0};
    float hp, maxHp, radius;
    Color color;
    
    // --- CC (Crowd Control) ---
    float stunTimer = 0.0f;
    float slowTimer = 0.0f;
    float hitFlashTimer = 0.0f;

    // --- SOPORTE PARA SPRITESHEETS ---
    int frameCols = 1;
    int frameRows = 1;
    int currentFrameX = 0;
    int currentFrameY = 0;
    float frameTimer = 0.0f;
    float frameSpeed = 0.15f; // segundos por frame

    virtual ~Entity() = default;

    virtual void Update() = 0;
    virtual void Draw() = 0;
    
    virtual float GetZDepth() const {
        return position.y;
    }

    void DrawHealthBar(float width, float height) {
        float healthPct = hp / maxHp;
        DrawRectangle((int)(position.x - width/2), (int)(position.y - radius - 50), (int)width, (int)height, RED);
        DrawRectangle((int)(position.x - width/2), (int)(position.y - radius - 50), (int)(width * healthPct), (int)height, GREEN);
    }
};
