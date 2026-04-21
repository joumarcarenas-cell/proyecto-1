#pragma once
#include <raylib.h>
#include <vector>
#include "ResourceManager.h"

namespace Arena {
    enum class PropType { ROCK, FOLIAGE };

    struct Prop {
        Vector2 position;
        float radius;
        float hp;
        bool active;
        PropType type;
        int textureVariant; // Index in the atlas/sheet

        Prop(Vector2 pos, float r, PropType t, int variant = 0) 
            : position(pos), radius(r), hp(1.0f), active(true), type(t), textureVariant(variant) {}

        void Draw() const {
            if (!active) return;
            Color col = (type == PropType::ROCK) ? GRAY : DARKGREEN;
            DrawCircleV(position, radius, col);
            DrawCircleLines((int)position.x, (int)position.y, radius, Fade(WHITE, 0.3f));
        }
    };
}
