#pragma once
#include <algorithm>
#include <functional>
#include <raylib.h>
#include <vector>

namespace Graphics {
struct DrawCall {
  std::function<void()> drawCommand;
  float zDepth;
};

class RenderManager {
public:
  static RenderManager &GetInstance() {
    static RenderManager instance;
    return instance;
  }

  RenderManager(const RenderManager &) = delete;
  RenderManager &operator=(const RenderManager &) = delete;

  void Submit(float zDepth, std::function<void()> drawCommand) {
    drawQueue.push_back({std::move(drawCommand), zDepth});
  }

  void Render() {
    std::stable_sort(drawQueue.begin(), drawQueue.end(),
                     [](const DrawCall &a, const DrawCall &b) {
                       return a.zDepth < b.zDepth;
                     });

    for (const auto &call : drawQueue) {
      call.drawCommand();
    }

    drawQueue.clear();
  }

private:
  RenderManager() = default;
  std::vector<DrawCall> drawQueue;
};
} // namespace Graphics
