#pragma once
#include "Vec2D.h"

struct AABB {
    Vec2D pos, size;
    AABB(int x = 0, int y = 0, int w = 0, int h = 0) : pos(x, y), size(w, h) {}
    AABB(Vec2D pos, Vec2D size) : pos(pos), size(size) {}
    Vec2D getMax() {
        return Vec2D(pos.x + size.x, pos.y + size.y);
    }
    AABB minkowskiDifference(AABB other) {
        Vec2D mPos = pos - other.getMax();
        Vec2D mSize = size + other.size;
        return AABB(mPos, mSize);
    }
    void penetrationVector(Vec2D relativePoint, Vec2D& pv, Vec2D& edge) { // find shortest distance from origin to edge of minkowski difference rectangle
        float minDist = abs(relativePoint.x - pos.x); // left edge
        pv = Vec2D(pos.x, relativePoint.y);
        if (abs(getMax().x - relativePoint.x) < minDist) { // right edge
            minDist = abs(getMax().x - relativePoint.x);
            pv = Vec2D(getMax().x, relativePoint.y);
        }
        if (abs(getMax().y - relativePoint.y) < minDist) { // bottom edge
            minDist = abs(getMax().y - relativePoint.y);
            pv = Vec2D(relativePoint.x, getMax().y);
        }
        if (abs(pos.y - relativePoint.y) < minDist) { // top edge
            minDist = abs(pos.y - relativePoint.y);
            pv = Vec2D(relativePoint.x, pos.y);
        }
        edge = pv.unitVector();
    }
    SDL_Rect* AABBtoRect() {
        return new SDL_Rect{ (int)round(pos.x), (int)round(pos.y), (int)round(size.x), (int)round(size.y) };
    }
};