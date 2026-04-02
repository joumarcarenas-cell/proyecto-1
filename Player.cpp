#include "entities.h"
#include "rlgl.h"
#include <cmath>

void Player::Update() {
    // Dash cooldown
    if (dashCooldown > 0) dashCooldown -= GetFrameTime();

    // Movimiento básico
    Vector2 move = {0, 0};
    if (IsKeyDown(KEY_W)) move.y -= 1;
    if (IsKeyDown(KEY_S)) move.y += 1;
    if (IsKeyDown(KEY_A)) move.x -= 1;
    if (IsKeyDown(KEY_D)) move.x += 1;

    if (Vector2Length(move) > 0) {
        move = Vector2Normalize(move);
        // Desplazamiento base más la influencia de posibles físicas (velocity)
        Vector2 nextPos = Vector2Add(position, Vector2Scale(move, 400.0f * GetFrameTime()));
        
        // Limites del mapa en Diamante Isometrico (Tray) 
        // Centro 2000,2000 con radios 1400, 700
        float dx = std::abs(nextPos.x - 2000.0f) / 1400.0f;
        float dy = std::abs(nextPos.y - 2000.0f) / 700.0f;
        if (dx + dy <= 1.0f) {
            position = nextPos;
        } else {
            // Deslizamiento en bordes
            float dx_only = std::abs(nextPos.x - 2000.0f) / 1400.0f;
            float dy_orig = std::abs(position.y - 2000.0f) / 700.0f;
            if (dx_only + dy_orig <= 1.0f) position.x = nextPos.x;
            else {
                float dx_orig = std::abs(position.x - 2000.0f) / 1400.0f;
                float dy_only = std::abs(nextPos.y - 2000.0f) / 700.0f;
                if (dx_orig + dy_only <= 1.0f) position.y = nextPos.y;
            }
        }
    }

    // Dirección de ataque y vista sigue al mouse
    Vector2 aimDiff = Vector2Subtract(targetAim, position);
    if (Vector2Length(aimDiff) > 0) {
        facing = Vector2Normalize(aimDiff); 
    }

    // Habilidad: Dash (Impulso veloz)
    if (IsKeyPressed(KEY_SPACE) && dashCooldown <= 0) {
        // En un ARPG el dash se lanza hacia donde caminas, si estás parado, hacia tu cursor.
        Vector2 dashDir = (Vector2Length(move) > 0) ? move : facing;
        velocity = Vector2Add(velocity, Vector2Scale(dashDir, 1200.0f)); // Reducido para no volar
        dashCooldown = 0.7f; 
    }

    // Físicas (empujones externos y frenado del dash)
    position = Vector2Add(position, Vector2Scale(velocity, GetFrameTime()));
    float dPx = std::abs(position.x - 2000.0f) / 1400.0f;
    float dPy = std::abs(position.y - 2000.0f) / 700.0f;
    if (dPx + dPy > 1.0f) {
        velocity = {0,0};
        Vector2 toCenter = Vector2Subtract({2000.0f, 2000.0f}, position);
        position = Vector2Add(position, Vector2Scale(Vector2Normalize(toCenter), 5.0f));
    }
    velocity = Vector2Scale(velocity, 0.85f);

    // Sistema de Combo
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isAttacking) {
        isAttacking = true;
        hasHit = false;
        attackTimer = 0.2f; // Duración del ataque
        comboTimer = 0.8f;  // Ventana para siguiente golpe
    }

    if (isAttacking) {
        attackTimer -= GetFrameTime();
        if (attackTimer <= 0) {
            isAttacking = false;
            comboStep = (comboStep + 1) % 4;
        }
    } else if (comboTimer > 0) {
        comboTimer -= GetFrameTime();
        if (comboTimer <= 0) comboStep = 0;
    }
}

bool Player::CheckAttackCollision(Enemy& enemy) {
    Vector2 diff = Vector2Subtract(enemy.position, position);
    float dist = Vector2Length(diff);
    
    if (dist < combo[comboStep].range + enemy.radius) {
        float angleToEnemy = atan2f(diff.y, diff.x) * RAD2DEG;
        float angleFacing = atan2f(facing.y, facing.x) * RAD2DEG;
        float angleDiff = fabsf(fmodf(angleToEnemy - angleFacing + 540, 360) - 180);
        
        return angleDiff <= combo[comboStep].angleWidth / 2;
    }
    return false;
}

void Player::Draw() {
    // Sombra
    DrawEllipse((int)position.x, (int)position.y, radius, radius * 0.5f, Fade(BLACK, 0.4f));
    // Cuerpo
    DrawCircleV({position.x, position.y - 20}, radius, color);
    DrawHealthBar(40, 5);

    if (isAttacking) {
        // Dibujar el "cono" de ataque distorsionado en el plano
        float startAngle = atan2f(facing.y, facing.x) * RAD2DEG - (combo[comboStep].angleWidth / 2);
        
        rlPushMatrix();
            rlTranslatef(position.x, position.y, 0); // Nos movemos al píe del jugador
            rlScalef(1.0f, 0.5f, 1.0f); // Achatamiento isométrico (mitad de altura)
            
            // Todos los ataques usan DrawCircleSector limpio
            DrawCircleSector({0, 0}, combo[comboStep].range,
                startAngle, startAngle + combo[comboStep].angleWidth,
                32, Fade(YELLOW, 0.55f));
        rlPopMatrix();
    }
}
