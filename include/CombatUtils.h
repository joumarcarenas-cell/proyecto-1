#pragma once

#include "raylib.h"
#include "raymath.h"
#include <cmath>

namespace CombatUtils {

    // Devuelve progresion 0.0 a 1.0
    inline float GetProgress(float timer, float totalDuration) {
        if (totalDuration <= 0.001f) return 1.0f;
        return fmaxf(0.0f, fminf(1.0f, 1.0f - timer / totalDuration));
    }

    // Distancia isométrica unificada (2:1)
    // Reconstruye la escala top-down multiplicando Y por 2.0 antes de medir
    inline float GetIsoDistance(Vector2 p1, Vector2 p2) {
        float dx = p2.x - p1.x;
        float dy = (p2.y - p1.y) * 2.0f;
        return sqrtf(dx * dx + dy * dy);
    }

    // Calcula si una posicion esta dentro de un sector isométrico (Arc)
    inline bool IsoArc(Vector2 pos1, Vector2 facing, Vector2 pos2, float targetRadius, float range, float halfAngleDeg) {
        Vector2 diff = Vector2Subtract(pos2, pos1);
        float isoY = diff.y * 2.0f;
        float isoDist = sqrtf(diff.x * diff.x + isoY * isoY);
        if (isoDist > range + targetRadius) return false;

        float angleToEnemy = atan2f(isoY, diff.x) * RAD2DEG;
        float facingAngle = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;

        float ad = fabsf(fmodf(angleToEnemy - facingAngle + 540.0f, 360.0f) - 180.0f);
        float angTol = fmaxf(6.0f, asinf(fminf(targetRadius / fmaxf(isoDist, 1.0f), 1.0f)) * RAD2DEG);
        return ad <= halfAngleDeg + angTol;
    }

    // ESTOCADA (Thrust): El alcance del ataque crece con el tiempo
    inline bool CheckProgressiveThrust(Vector2 attackerPos, Vector2 facing, 
                                       Vector2 targetPos, float targetRadius, 
                                       float maxRange, float halfAngleDeg, float progress) {
        float currentRange = maxRange * progress;
        return IsoArc(attackerPos, facing, targetPos, targetRadius, currentRange, halfAngleDeg);
    }

    // BARRIDO (Sweep): El sector barrido gira desde un ángulo inicial
    // dirFlag: +1.0f (Antihorario), -1.0f (Horario)
    inline bool CheckProgressiveSweep(Vector2 attackerPos, Vector2 facing, 
                                      Vector2 targetPos, float targetRadius, 
                                      float range, float startOffsetDeg, float totalSweepDeg, float dirFlag, float progress) {
        Vector2 diff = Vector2Subtract(targetPos, attackerPos);
        float isoY = diff.y * 2.0f;
        float isoDist = sqrtf(diff.x * diff.x + isoY * isoY);
        if (isoDist > range + targetRadius) return false;

        float angleToEnemy = atan2f(isoY, diff.x) * RAD2DEG;
        float facingAngle = atan2f(facing.y * 2.0f, facing.x) * RAD2DEG;
        float startAngle = facingAngle + startOffsetDeg;
        float coveredDeg = totalSweepDeg * progress;

        float angTol = fmaxf(8.0f, asinf(fminf(targetRadius / fmaxf(isoDist, 1.0f), 1.0f)) * RAD2DEG);

        float delta = fmodf((angleToEnemy - startAngle) * dirFlag + 7200.0f, 360.0f);
        return delta <= coveredDeg + angTol;
    }

    // AREA CIRCULAR (Spin / Torbellino / Slam): El radio del estallido crece con el tiempo
    inline bool CheckProgressiveRadial(Vector2 centerPos, Vector2 targetPos, float targetRadius, float maxRange, float progress) {
        Vector2 diff = Vector2Subtract(targetPos, centerPos);
        float isoY = diff.y * 2.0f;
        float isoDist = sqrtf(diff.x * diff.x + isoY * isoY);
        float currentRange = maxRange * progress;
        return isoDist <= currentRange + targetRadius;
    }
}
