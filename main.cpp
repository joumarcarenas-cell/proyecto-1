#include "raylib.h"
#include "rlgl.h"
#include "entities.h"
#include <vector>
#include <algorithm>

void DrawQuad(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, Color c) {
    // Dibujamos ambas direcciones de vértices para anular culling por error
    DrawTriangle(p1, p2, p3, c); DrawTriangle(p1, p3, p2, c);
    DrawTriangle(p1, p3, p4, c); DrawTriangle(p1, p4, p3, c);
}

struct Particle {
    Vector2 pos;
    Vector2 vel;
    float life;
    Color col;
};

int main() {
    InitWindow(1280, 720, "Expandido - Fisicas y Particulas");
    SetTargetFPS(60);

    Player player({2000, 2000}); // Centro del mapa gigante 4000x4000
    Enemy boss({2200, 2000});

    Camera2D camera = { 0 };
    camera.offset = { 1280.0f / 2.0f, 720.0f / 2.0f }; // Camara en el centro de la pantalla
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    std::vector<Particle> particles;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        // La cámara sigue al jugador
        camera.target = player.position;

        // Entregar la posición global del mouse a la lógica del jugador para apuntar
        player.targetAim = GetScreenToWorld2D(GetMousePosition(), camera);

        // 1. Actualización de entidades
        player.Update();
        boss.Update();

        // 2. Lógica de Colisión "Soft-Body" (Para que no se encimen)
        if (!boss.isDead) {
            Vector2 diff = Vector2Subtract(player.position, boss.position);
            float dist = Vector2Length(diff);
            float minDist = player.radius + boss.radius;
            if (dist > 0 && dist < minDist) {
                float overlap = minDist - dist;
                Vector2 push = Vector2Scale(Vector2Normalize(diff), overlap * 0.5f);
                player.position = Vector2Add(player.position, push);
                boss.position = Vector2Subtract(boss.position, push);
            }
        }

        // 3. Lógica de ataque y combate
        if (!boss.isDead && player.isAttacking && !player.hasHit) {
            if (player.CheckAttackCollision(boss)) {
                boss.hp -= player.combo[player.comboStep].damage;
                player.hasHit = true; 

                // Muerte del enemigo
                if (boss.hp <= 0) {
                    boss.isDead = true;
                    boss.respawnTimer = 3.0f; // Reaparece en 3 segundos
                }

                // Knockback: Empujón violento en dirección al ataque
                Vector2 knockbackDir = Vector2Normalize(Vector2Subtract(boss.position, player.position));
                boss.velocity = Vector2Scale(knockbackDir, player.combo[player.comboStep].damage * 60.0f); 

                // Emitir partículas de golpe (simulando sangre y chispas)
                for (int i = 0; i < 20; i++) {
                    Vector2 pVel = { (float)GetRandomValue(-400, 400), (float)GetRandomValue(-400, 400) };
                    particles.push_back({boss.position, pVel, (float)GetRandomValue(3, 7)/10.0f, RED});
                }
            }
        }

        // 4. Actualización de partículas
        for (auto& p : particles) {
            p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt));
            p.vel = Vector2Scale(p.vel, 0.9f); // Fricción en el aire para frenarlas
            p.life -= dt;
        }
        // Limpiar partículas muertas
        particles.erase(std::remove_if(particles.begin(), particles.end(), 
            [](const Particle& p){ return p.life <= 0; }), particles.end());

        // 5. Preparar Z-Sorting
        std::vector<Entity*> drawQueue = { &player, &boss };
        std::sort(drawQueue.begin(), drawQueue.end(), [](Entity* a, Entity* b) {
            return a->position.y < b->position.y;
        });

        // 6. Dibujo
        BeginDrawing();
            ClearBackground({20, 25, 30, 255}); // Color asfalto oscuro

            BeginMode2D(camera);
                // --- ESTRUCTURA DE BANDEJA ISOMÉTRICA (Tray) ---
                Vector2 C = { 2000.0f, 2000.0f }; // Centro
                float Rx = 1400.0f; 
                float Ry = 700.0f;
                float Z = 150.0f; // Altura de los muros (z-depth)

                Vector2 T = { C.x, C.y - Ry };
                Vector2 R = { C.x + Rx, C.y };
                Vector2 B = { C.x, C.y + Ry };
                Vector2 L = { C.x - Rx, C.y };

                Vector2 Tu = { T.x, T.y - Z };
                Vector2 Lu = { L.x, L.y - Z };
                Vector2 Ru = { R.x, R.y - Z };

                Vector2 Ld = { L.x, L.y + Z };
                Vector2 Bd = { B.x, B.y + Z };
                Vector2 Rd = { R.x, R.y + Z };

                // 1. Piso interior de la bandeja
                DrawQuad(T, L, B, R, {220, 220, 225, 255}); // Color claro (tipo lienzo)

                // 2. Muros Interiores (Top-Left y Top-Right)
                DrawQuad(Lu, L, T, Tu, {180, 180, 185, 255}); // Gris medio
                DrawQuad(Tu, T, R, Ru, {140, 140, 145, 255}); // Gris oscuro (sombra)

                // 3. Muros Exteriores Frontales (Bottom-Left y Bottom-Right cayendo)
                DrawQuad(L, Ld, Bd, B, {140, 140, 145, 255}); // Gris oscuro
                DrawQuad(B, Bd, Rd, R, {180, 180, 185, 255}); // Gris medio

                // 4. Perfilados (Outlines) estilo boceto para resaltar los cruces (estilo el ejemplo dado)
                Color lineCol = { 40, 40, 40, 255 };
                float thick = 4.0f;
                
                // Bordes superiores interiores
                DrawLineEx(Lu, Tu, thick, lineCol);
                DrawLineEx(Tu, Ru, thick, lineCol);
                
                // Esquinas verticales de arrriba
                DrawLineEx(Lu, L, thick, lineCol);
                DrawLineEx(Tu, T, thick, lineCol);
                DrawLineEx(Ru, R, thick, lineCol);
                
                // Bordes del rombo interior (El suelo)
                DrawLineEx(L, T, thick, lineCol);
                DrawLineEx(T, R, thick, lineCol);
                DrawLineEx(L, B, thick, lineCol);
                DrawLineEx(B, R, thick, lineCol);
                
                // Esquinas verticales de abajo
                DrawLineEx(L, Ld, thick, lineCol);
                DrawLineEx(B, Bd, thick, lineCol);
                DrawLineEx(R, Rd, thick, lineCol);
                
                // Bordes inferiores exteriores
                DrawLineEx(Ld, Bd, thick, lineCol);
                DrawLineEx(Bd, Rd, thick, lineCol);

                // Dibujar entidades encima del piso (el Z-sort respeta la elevación visual del tray)
                for (auto entity : drawQueue) {
                    entity->Draw();
                }

                // Dibujar partículas usando lógica 2.5D para "rebotar" y caer
                for (auto& p : particles) {
                    // Calculamos una parábola basada en su vida para q salten ("height")
                    // Supongamos que life era 0.5 en maximo.
                    float heightOff = sinf(p.life * PI * 2.0f) * 40.0f; 
                    if (heightOff < 0) heightOff = 0; // Evitar ir bajo tierra

                    // Sombrita
                    DrawEllipse((int)p.pos.x, (int)p.pos.y, 4, 2, Fade(BLACK, 0.5f));
                    // Partícula real
                    DrawCircleV({p.pos.x, p.pos.y - heightOff}, 4.0f, p.col);
                }
            EndMode2D();

            DrawText("Modo 2.5D Logico: Mapa gigante 4000x4000.", 10, 10, 20, RAYWHITE);
            DrawText("Choca con el enemigo, hay empuje, frenado y separacion dinamica.", 10, 35, 20, RAYWHITE);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
