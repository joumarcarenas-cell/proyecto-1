#include "raylib.h"
#include "rlgl.h"
#include "entities.h"
#include <vector>
#include <algorithm>

// Implementación de Enemy::Update y Draw (Para que compile todo junto)
void Enemy::Update() {
    if (isDead) {
        respawnTimer -= GetFrameTime();
        if (respawnTimer <= 0.0f) { isDead = false; hp = maxHp; position = spawnPos; velocity = {0, 0}; }
        return;
    }
    Vector2 nextPos = Vector2Add(position, Vector2Scale(velocity, GetFrameTime()));
    
    // Restricción de Arena (Hull de Diamante)
    float dx = std::abs(nextPos.x - 2000.0f) / 1400.0f;
    float dy = std::abs(nextPos.y - 2000.0f) / 700.0f;
    if (dx + dy <= 1.0f) {
        position = nextPos;
    }
    
    velocity = Vector2Scale(velocity, 0.85f);
}

void Enemy::Draw() {
    if (isDead) return;
    DrawEllipse((int)position.x, (int)position.y, radius, radius * 0.5f, Fade(BLACK, 0.4f));
    DrawCircleV({position.x, position.y - 30}, radius, color);
    DrawHealthBar(80, 10);
}

void DrawQuad(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, Color c) {
    DrawTriangle(p1, p2, p3, c); DrawTriangle(p1, p3, p2, c);
    DrawTriangle(p1, p3, p4, c); DrawTriangle(p1, p4, p3, c);
}

struct Particle { Vector2 pos; Vector2 vel; float life; Color col; };

int main() {
    InitWindow(1280, 720, "Hades Prototype - Skills & Energy");
    SetTargetFPS(60);

    Player player({2000, 2000});
    player.energy = 100.0f; // ENERGÍA INICIAL PARA PRUEBAS (Integridad V2)
    Enemy boss({2300, 2000});
    boss.maxHp = 2000.0f;
    boss.hp = 2000.0f;
    Camera2D camera = { {640, 360}, player.position, 0.0f, 1.0f };
    std::vector<Particle> particles;
    
    float hitLagTimer = 0.0f;
    float screenShake = 0.0f;

    GamePhase currentPhase = GamePhase::RUNNING;
    int* rebindingKey = nullptr;
    std::string rebindingName = "";

    HideCursor();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        // --- GESTIÓN DE PAUSA Y MENÚS ---
        if (IsKeyPressed(KEY_K)) {
            if (currentPhase == GamePhase::RUNNING) {
                currentPhase = GamePhase::PAUSED;
                ShowCursor();
            } else if (currentPhase == GamePhase::PAUSED || currentPhase == GamePhase::SETTINGS) {
                currentPhase = GamePhase::RUNNING;
                HideCursor();
                rebindingKey = nullptr;
            }
        }

        if (currentPhase == GamePhase::RUNNING) {
            if (hitLagTimer > 0) hitLagTimer -= dt;
            if (screenShake > 0) screenShake -= dt;
            camera.target = player.position;
            player.targetAim = GetScreenToWorld2D(GetMousePosition(), camera);

            if (hitLagTimer <= 0) {
                player.Update();
                boss.Update();
            }
        } else if (currentPhase == GamePhase::REBINDING) {
            int key = GetKeyPressed();
            if (key > 0) {
                *rebindingKey = key;
                currentPhase = GamePhase::SETTINGS;
                rebindingKey = nullptr;
            }
        }

        // Colisión de Giro (Spin)
        if (currentPhase == GamePhase::RUNNING && player.isSpinning && player.spinTimer <= 0 && !boss.isDead) {
            float dist = Vector2Distance(player.position, boss.position);
            if (dist < player.radius + 125.0f) { // Rango Melee 125 (Reducido 15%)
                boss.hp -= 5.0f; 
                player.spinHitCount++;
                player.spinTimer = 0.15f; 
                
                // Cada 2 hits, consume 10% de energía
                if (player.spinHitCount % 2 == 0) {
                    player.energy -= 10.0f;
                    if (player.energy < 0) player.energy = 0;
                }
                
                // Partículas de impacto
                for (int i = 0; i < 5; i++) {
                    particles.push_back({ boss.position, { (float)GetRandomValue(-150, 150), (float)GetRandomValue(-150, 150) }, 0.5f, SKYBLUE });
                }
            }
        }

        // Colisión Bumeranes Activos (Múltiples y Orbitales)
        for (auto& b : player.activeBoomerangs) {
            if (b.active && !boss.isDead) {
                if (CheckCollisionCircles(b.position, b.isOrbital ? 30 : 25, boss.position, boss.radius)) {
                    boss.hp -= b.isOrbital ? 3.0f : 1.5f; 
                    if (boss.hp <= 0) { boss.isDead = true; boss.respawnTimer = 3.0f; }
                }
            }
        }

        // Combate e Impactos
        if (!boss.isDead && player.isAttacking && !player.hasHit) {
            if (player.CheckAttackCollision(boss)) {
                float dmg = player.combo[player.comboStep].damage;
                boss.hp -= dmg;
                player.hasHit = true;
                
                // RECARGA ENERGIA (5 golpes para 100% -> 20 por golpe)
                player.energy = fminf(player.maxEnergy, player.energy + 20.0f);
                
                // ROBO DE VIDA (30% si está buffado)
                if (player.isBuffed) player.hp = fminf(player.maxHp, player.hp + (dmg * 0.3f));

                // FEEDBACK: HIT LAG (Sutil) + SCREEN SHAKE (Mínimo)
                hitLagTimer = 0.02f; 
                screenShake = dmg * 0.02f; 
                
                boss.velocity = Vector2Scale(player.facing, dmg * 50.0f);
                for (int i = 0; i < 20; i++) {
                    float pAngle = atan2f(player.facing.y, player.facing.x) + (float)GetRandomValue(-60, 60) * DEG2RAD;
                    float pSpeed = (float)GetRandomValue(200, 600);
                    particles.push_back({boss.position, {cosf(pAngle) * pSpeed, sinf(pAngle) * pSpeed}, 0.6f, (player.isBuffed ? GOLD : RED)});
                }
            }
        }
        
        // 3. Definitiva (R) - Solo si el enemigo tiene < 75% vida y CD listo
        if (IsKeyPressed(player.controls.ultimate) && player.ultimateCooldown <= 0 && !boss.isDead && (boss.hp / boss.maxHp) < 0.75f) {
            player.ActivateUltimate();
            for (int i = 0; i < 20; i++) particles.push_back({player.position, {(float)GetRandomValue(-400,400), (float)GetRandomValue(-400,400)}, 0.8f, RED});
        }

        // --- COLISION DE SUPER ESTOCADA ---
        if (!boss.isDead && player.isDashAttacking && !player.hasDashHit) {
            if (player.CheckDashCollision(boss)) {
                float dmg = 45.0f; // 10 más que el último golpe
                boss.hp -= dmg;
                player.hasDashHit = true;
                
                screenShake = 1.2f;
                for (int i = 0; i < 15; i++) {
                    particles.push_back({ boss.position, { (float)GetRandomValue(-300, 300), (float)GetRandomValue(-300, 300) }, 0.6f, GOLD });
                }
            }
        }

        // Partículas
        for (auto& p : particles) { p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt)); p.life -= dt; }
        particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& p){ return p.life <= 0; }), particles.end());

        BeginDrawing();
            ClearBackground({25, 30, 40, 255}); 
            
            // Aplicar Screen Shake Mínimo
            Camera2D shakeCam = camera;
            if (screenShake > 0) {
                shakeCam.offset.x += (float)GetRandomValue(-1, 1) * screenShake;
                shakeCam.offset.y += (float)GetRandomValue(-1, 1) * screenShake;
            }

            BeginMode2D(shakeCam);
                // Arena Isométrica Mejorada (Diamante 1400x700)
                Vector2 pNorth = { 2000, 2000 - 700 };
                Vector2 pSouth = { 2000, 2000 + 700 };
                Vector2 pEast  = { 2000 + 1400, 2000 };
                Vector2 pWest  = { 2000 - 1400, 2000 };
                
                Color groundColor = { 245, 245, 245, 255 }; // Blanco/Gris muy claro
                Color wallColor   = { 180, 185, 195, 255 }; // Color paredes (mismas que el borde pero rellenas)
                Color borderColor = { 60, 65, 75, 255 };
                
                float wallHeight = 120.0f;
                
                // --- DIBUJO DE PAREDES TRASERAS (Profundidad 2.5D) ---
                // Pared Nor-Oeste
                DrawQuad(pWest, pNorth, {pNorth.x, pNorth.y - wallHeight}, {pWest.x, pWest.y - wallHeight}, wallColor);
                // Pared Nor-Este
                DrawQuad(pNorth, pEast, {pEast.x, pEast.y - wallHeight}, {pNorth.x, pNorth.y - wallHeight}, ColorBrightness(wallColor, -0.1f));

                // Dibujar el relleno del diamante
                DrawTriangle(pWest, pNorth, pEast, groundColor);
                DrawTriangle(pWest, pEast, pSouth, groundColor);
                
                // Dibujar bordes definidos (Suelo)
                DrawLineEx(pNorth, pEast, 5.0f, borderColor);
                DrawLineEx(pEast, pSouth, 5.0f, borderColor);
                DrawLineEx(pSouth, pWest, 5.0f, borderColor);
                DrawLineEx(pWest, pNorth, 5.0f, borderColor);
                
                // Bordes superiores de las paredes
                DrawLineEx({pWest.x, pWest.y - wallHeight}, {pNorth.x, pNorth.y - wallHeight}, 5.0f, borderColor);
                DrawLineEx({pNorth.x, pNorth.y - wallHeight}, {pEast.x, pEast.y - wallHeight}, 5.0f, borderColor);
                DrawLineEx(pWest, {pWest.x, pWest.y - wallHeight}, 5.0f, borderColor);
                DrawLineEx(pNorth, {pNorth.x, pNorth.y - wallHeight}, 5.0f, borderColor);
                DrawLineEx(pEast, {pEast.x, pEast.y - wallHeight}, 5.0f, borderColor);
                
                if (player.position.y < boss.position.y) { player.Draw(); boss.Draw(); }
                else { boss.Draw(); player.Draw(); }

                for (auto& p : particles) DrawCircleV(p.pos, 3, p.col);
            EndMode2D();

            // --- UI DE JUEGO ---
            if (currentPhase == GamePhase::RUNNING) {
                DrawText(TextFormat("ENERGIA: %i%%", (int)player.energy), 20, 20, 20, SKYBLUE);
                if (player.ultimateCooldown > 0) DrawText(TextFormat("R EN CD: %.1fs", player.ultimateCooldown), 20, 80, 20, RED);
            }

            // --- MENÚ DE PAUSA ---
            if (currentPhase == GamePhase::PAUSED) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
                DrawText("JUEGO EN PAUSA", GetScreenWidth()/2 - 120, 150, 30, RAYWHITE);
                
                Rectangle btnResume = { (float)GetScreenWidth()/2 - 100, 250, 200, 50 };
                Rectangle btnSettings = { (float)GetScreenWidth()/2 - 100, 320, 200, 50 };
                
                if (CheckCollisionPointRec(GetMousePosition(), btnResume)) {
                    DrawRectangleRec(btnResume, GRAY);
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { currentPhase = GamePhase::RUNNING; HideCursor(); }
                } else DrawRectangleRec(btnResume, DARKGRAY);
                DrawText("REANUDAR", (int)btnResume.x + 45, (int)btnResume.y + 15, 20, WHITE);
                
                if (CheckCollisionPointRec(GetMousePosition(), btnSettings)) {
                    DrawRectangleRec(btnSettings, GRAY);
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) currentPhase = GamePhase::SETTINGS;
                } else DrawRectangleRec(btnSettings, DARKGRAY);
                DrawText("AJUSTES", (int)btnSettings.x + 55, (int)btnSettings.y + 15, 20, WHITE);
            }

            // --- MENÚ DE AJUSTES ---
            if (currentPhase == GamePhase::SETTINGS || currentPhase == GamePhase::REBINDING) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), BLACK);
                DrawText("AJUSTES Y CONTROLES", 100, 50, 30, GOLD);
                
                // Resolución
                DrawText("RESOLUCION:", 100, 120, 20, GRAY);
                Rectangle res1 = { 300, 110, 120, 40 };
                Rectangle res2 = { 430, 110, 120, 40 };
                if (CheckCollisionPointRec(GetMousePosition(), res1) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) SetWindowSize(1280, 720);
                if (CheckCollisionPointRec(GetMousePosition(), res2) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) SetWindowSize(1600, 900);
                DrawRectangleRec(res1, DARKGRAY); DrawText("720p", 335, 120, 20, WHITE);
                DrawRectangleRec(res2, DARKGRAY); DrawText("900p", 465, 120, 20, WHITE);

                // Rebinds
                DrawText("CONTROLES (Haz click para reasignar):", 100, 200, 20, GRAY);
                auto DrawRebind = [&](const char* label, int* key, int y) {
                    DrawText(label, 120, y, 20, WHITE);
                    Rectangle r = { 300, (float)y - 5, 150, 30 };
                    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
                    DrawRectangleRec(r, hover ? GRAY : DARKGRAY);
                    DrawText(currentPhase == GamePhase::REBINDING && rebindingKey == key ? "..." : TextFormat("K: %i", *key), 310, y, 20, GOLD);
                    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        currentPhase = GamePhase::REBINDING;
                        rebindingKey = key;
                        rebindingName = label;
                    }
                };
                
                DrawRebind("Dash", &player.controls.dash, 250);
                DrawRebind("Bumeran", &player.controls.boomerang, 300);
                DrawRebind("Berserker", &player.controls.berserker, 350);
                DrawRebind("Ultimate", &player.controls.ultimate, 400);

                if (currentPhase == GamePhase::REBINDING) {
                    DrawText(TextFormat("Pulsa una tecla para [%s]", rebindingName.c_str()), 100, 500, 20, SKYBLUE);
                }

                DrawText("PRESIONA K PARA VOLVER", 100, 650, 15, GRAY);
            }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}