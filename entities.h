#ifndef ENTITIES_H
#define ENTITIES_H

#include "raylib.h"
#include "raymath.h"

struct AttackFrame {
    float range;
    float angleWidth; // En grados
    float damage;
};

class Entity {
public:
    Vector2 position;
    Vector2 velocity = {0, 0};
    float hp, maxHp, radius;
    Color color;

    virtual void Update() = 0;
    virtual void Draw() = 0;
    
    void DrawHealthBar(float width, float height) {
        float healthPct = hp / maxHp;
        DrawRectangle((int)(position.x - width/2), (int)(position.y - radius - 50), (int)width, (int)height, RED);
        DrawRectangle((int)(position.x - width/2), (int)(position.y - radius - 50), (int)(width * healthPct), (int)height, GREEN);
    }
};

class Enemy : public Entity {
public:
    Vector2 spawnPos;
    float respawnTimer = 0.0f;
    bool isDead = false;

    Enemy(Vector2 pos) {
        spawnPos = pos;
        position = pos;
        radius = 40.0f; // Doble que el player
        maxHp = 500.0f; // 5x que el player
        hp = maxHp;
        color = MAROON;
    }
    void Update() override {
        if (isDead) {
            respawnTimer -= GetFrameTime();
            if (respawnTimer <= 0.0f) {
                // Reaparece en su punto de spawn con HP completa
                isDead = false;
                hp = maxHp;
                position = spawnPos;
                velocity = {0, 0};
            }
            return; // No procesar físicas mientras está muerto
        }

        position = Vector2Add(position, Vector2Scale(velocity, GetFrameTime()));
        // Limite Diamante Tray
        float dx = std::abs(position.x - 2000.0f) / 1400.0f;
        float dy = std::abs(position.y - 2000.0f) / 700.0f;
        if (dx + dy > 1.0f) {
            velocity = {0, 0};
            Vector2 toCenter = Vector2Subtract({2000.0f, 2000.0f}, position);
            position = Vector2Add(position, Vector2Scale(Vector2Normalize(toCenter), 5.0f));
        }
        velocity = Vector2Scale(velocity, 0.85f);
    }
    void Draw() override {
        if (isDead) return; // No dibujar si está muerto
        // Sombra (pie del personaje anclado a la Z-sort)
        DrawEllipse((int)position.x, (int)position.y, radius, radius * 0.5f, Fade(BLACK, 0.4f));
        // Cuerpo (elevado)
        DrawCircleV({position.x, position.y - 30}, radius, color);
        DrawHealthBar(80, 10);
    }
};

class Player : public Entity {
public:
    int comboStep = 0;
    bool isAttacking = false;
    bool hasHit = false;
    float attackTimer = 0;
    float comboTimer = 0;
    Vector2 facing = {1, 0};
    
    // Habilidades extra
    float dashCooldown = 0.0f;
    Vector2 targetAim = {0, 0};

    // Configuración de los 4 ataques (Copiando los 4 cortes de la imagen)
    // Hit 1: Linea rectilinea delgada (rango muy largo, angulo agudo)
    // Hit 2 & 3: Cortes tipo media luna (rango medio, angulo abanico grande)
    // Hit 4: Cono grueso de impacto (rango largo, angulo piramidal ancho)
    AttackFrame combo[4] = {
        {200.0f, 16.0f,  10.0f},   // Hit 1: 25 * 0.65
        {150.0f, 130.0f, 15.0f},   // Hit 2: 200 * 0.65
        {150.0f, 130.0f, 15.0f},   // Hit 3: 200 * 0.65
        {220.0f, 52.0f,  35.0f}    // Hit 4: 80 * 0.65
    };

    Player(Vector2 pos) {
        position = pos;
        radius = 20.0f;
        maxHp = 100.0f;
        hp = maxHp;
        color = BLUE;
    }

    void Update() override;
    void Draw() override;
    bool CheckAttackCollision(Enemy& enemy);
};

#endif
