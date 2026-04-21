#pragma once
#include "raylib.h"
#include "raymath.h"
#include <cmath>

namespace Directions {
    // Standardized 8-Direction Enum (Clockwise from South)
    enum class Direction8 {
        S = 0,
        SW = 1,
        W = 2,
        NW = 3,
        N = 4,
        NE = 5,
        E = 6,
        SE = 7,
        NONE = -1
    };

    // Helper to get the 8-directional index from a vector
    // Accounts for isometric 2:1 compression if needed
    inline Direction8 GetDirection8(Vector2 dir) {
        if (Vector2Length(dir) < 0.01f) return Direction8::NONE;

        // Isometric Angle mapping
        // We use world space vector but adjust for the visual tilt (Y*2.0 for distance/angle)
        float angle = atan2f(dir.y * 2.0f, dir.x) * RAD2DEG;
        if (angle < 0) angle += 360.0f;

        // Shift angle so South (90 deg) is at the center of the first sector if we start there
        // Or just map directly to sectors:
        // S is around 90, SE around 45 (or 26.5 iso), E around 0, NE around 315 (-26.5 iso)...
        
        if (angle >= 67.5f && angle < 112.5f)   return Direction8::S;
        if (angle >= 112.5f && angle < 157.5f)  return Direction8::SW;
        if (angle >= 157.5f && angle < 202.5f)  return Direction8::W;
        if (angle >= 202.5f && angle < 247.5f)  return Direction8::NW;
        if (angle >= 247.5f && angle < 292.5f)  return Direction8::N;
        if (angle >= 292.5f && angle < 337.5f)  return Direction8::NE;
        if (angle >= 337.5f || angle < 22.5f)   return Direction8::E;
        if (angle >= 22.5f && angle < 67.5f)    return Direction8::SE;

        return Direction8::S;
    }

    // Returns a normalized vector snapped to one of the 8 isometric directions
    inline Vector2 GetSnappedVector(Vector2 dir) {
        Direction8 d = GetDirection8(dir);
        switch (d) {
            case Direction8::S:  return { 0, 1 };
            case Direction8::SW: return Vector2Normalize({ -1, 0.5f });
            case Direction8::W:  return { -1, 0 };
            case Direction8::NW: return Vector2Normalize({ -1, -0.5f });
            case Direction8::N:  return { 0, -1 };
            case Direction8::NE: return Vector2Normalize({ 1, -0.5f });
            case Direction8::E:  return { 1, 0 };
            case Direction8::SE: return Vector2Normalize({ 1, 0.5f });
            default: return { 0, 1 };
        }
    }

    // Helper for sprite selection logic
    inline int ToIndex(Direction8 dir) {
        if (dir == Direction8::NONE) return 0;
        return (int)dir;
    }
}
