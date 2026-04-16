#pragma once
#include <algorithm>
#include <functional>
#include <raylib.h>
#include <vector>

namespace Graphics {

enum class RenderLayer {
  BACKGROUND = 0, // Suelo, etc.
  WORLD = 1,      // Entidades, paredes, obstáculos
  VFX = 2,        // Partículas, rastro, etc.
  LIGHTING = 3,   // Luces, brillos, sombras suaves
  UI = 4          // Interfaz
};

struct DrawCall {
  std::function<void()> drawCommand;
  RenderLayer layer;
  float zDepth;
  BlendMode blendMode;
};

class RenderManager {
public:
  static RenderManager &GetInstance() {
    static RenderManager instance;
    return instance;
  }

  RenderManager(const RenderManager &) = delete;
  RenderManager &operator=(const RenderManager &) = delete;

  // Submit con capa y blend mode opcionales
  void Submit(float zDepth, std::function<void()> drawCommand,
              RenderLayer layer = RenderLayer::WORLD,
              BlendMode blendMode = BLEND_ALPHA) {
    drawQueue.push_back({std::move(drawCommand), layer, zDepth, blendMode});
  }

  void Render() {
    // Ordenar primero por capa y luego por profundidad Z
    std::stable_sort(drawQueue.begin(), drawQueue.end(),
                     [](const DrawCall &a, const DrawCall &b) {
                       if (a.layer != b.layer)
                         return (int)a.layer < (int)b.layer;
                       return a.zDepth < b.zDepth;
                     });

    BlendMode currentMode = BLEND_ALPHA;
    for (const auto &call : drawQueue) {
      if (call.blendMode != currentMode) {
        if (currentMode != BLEND_ALPHA)
          EndBlendMode();
        currentMode = call.blendMode;
        if (currentMode != BLEND_ALPHA)
          BeginBlendMode(currentMode);
      }
      call.drawCommand();
    }
    if (currentMode != BLEND_ALPHA)
      EndBlendMode();

    drawQueue.clear();
  }

private:
  RenderManager() = default;
  std::vector<DrawCall> drawQueue;
};
} // namespace Graphics
