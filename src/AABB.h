#pragma once
#include "Vec2D.h"
#include <algorithm>

// Significant help from guide at https://blog.hamaluik.ca/posts/simple-aabb-collision-using-minkowski-difference/

struct AABB {
    Vec2D pos, size;
    AABB(int x = 0, int y = 0, int w = 0, int h = 0) : pos(x, y), size(w, h) {}
    AABB(Vec2D pos, Vec2D size) : pos(pos), size(size) {}
    Vec2D min() {
        return pos;
    }
    Vec2D max() {
        return pos + size;
    }
    AABB minkowskiDifference(AABB other) {
        Vec2D mPos = pos - other.max();
        Vec2D mSize = size + other.size;
        return AABB(mPos, mSize);
    }
    void penetrationVector(Vec2D relativePoint, Vec2D& pv, Vec2D& edge, Vec2D vel) { // find shortest distance from origin to edge of minkowski difference rectangle
        float minTime = std::numeric_limits<float>::infinity();
        pv = Vec2D();
        if (abs(vel.x) != 0) {
            if (abs((relativePoint.x - pos.x) / vel.x) < minTime) {
                minTime = abs((relativePoint.x - pos.x) / vel.x); // left edge
                pv = Vec2D(pos.x, relativePoint.y);
            }
        }
        if (abs(vel.x) != 0) {
            if (abs((max().x - relativePoint.x) / vel.x) < minTime) { // right edge
                minTime = abs((max().x - relativePoint.x) / vel.x);
                pv = Vec2D(max().x, relativePoint.y);
            }
        }
        if (abs(vel.y) != 0) {
            if (abs((max().y - relativePoint.y) / vel.y) < minTime) { // bottom edge
                minTime = abs((max().y - relativePoint.y) / vel.y);
                pv = Vec2D(relativePoint.x, max().y);
            }
        }
        if (abs(vel.y) != 0) {
            if (abs((pos.y - relativePoint.y) / vel.y) < minTime) { // top edge
                minTime = abs((pos.y - relativePoint.y) / vel.y);
                pv = Vec2D(relativePoint.x, pos.y);
            }
        }
        edge = pv.unitVector();
    }
    SDL_Rect* AABBtoRect() {
        return new SDL_Rect{ (int)round(pos.x), (int)round(pos.y), (int)round(size.x), (int)round(size.y) };
    }
};