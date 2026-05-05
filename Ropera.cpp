#include "include/Ropera.h"
#include "include/Boss.h"

void Ropera::Update() {
    float dt = GetFrameTime() * g_timeScale;
    
    // Movimiento básico heredado de Player si fuera necesario, 
    // pero aquí lo implementamos manual para el stub.
    Vector2 move = {0, 0};
    if (IsKeyDown(KEY_W)) move.y -= 1;
    if (IsKeyDown(KEY_S)) move.y += 1;
    if (IsKeyDown(KEY_A)) move.x -= 1;
    if (IsKeyDown(KEY_D)) move.x += 1;
    
    if (Vector2Length(move) > 0) {
        move = Vector2Normalize(move);
        Vector2 np = Vector2Add(position, Vector2Scale(move, 400.0f * dt));
        position = Arena::GetClampedPos(np, radius);
        
        Vector2 aim = Vector2Subtract(targetAim, position);
        if (Vector2Length(aim) > 0) facing = Vector2Normalize(aim);
    }
}

void Ropera::Draw() {
    // Dibujo minimalista: círculo con borde
    DrawCircleV({position.x, position.y - 20}, radius, color);
    DrawCircleLines((int)position.x, (int)position.y - 20, radius + 2, WHITE);
    
    // Indicador de dirección
    Vector2 dirPos = Vector2Add({position.x, position.y - 20}, Vector2Scale(facing, radius + 10));
    DrawCircleV(dirPos, 4, GOLD);
}

void Ropera::Reset(Vector2 pos) {
    position = pos;
    hp = maxHp;
    velocity = {0, 0};
    state = RoperaState::NORMAL;
}

void Ropera::HandleSkills(Boss &boss) {
    // Sin habilidades en el stub
}

void Ropera::CheckCollisions(Boss &boss) {
    // Sin colisiones de ataque en el stub
}

std::vector<AbilityInfo> Ropera::GetAbilities() const {
    return {}; // Lista vacía de habilidades
}
