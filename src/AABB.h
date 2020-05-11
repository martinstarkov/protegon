#pragma once
#include "Vec2D.h"

// Significant help from guide at https://blog.hamaluik.ca/posts/simple-aabb-collision-using-minkowski-difference/

struct AABB {
    Vec2D pos, size;
    AABB(int x = 0, int y = 0, int w = 0, int h = 0) : pos(x, y), size(w, h) {}
    AABB(Vec2D pos, Vec2D size) : pos(pos), size(size) {}
    Vec2D getMax() {
        return Vec2D(pos.x + size.x, pos.y + size.y);
    }
    AABB broadphaseBox(Vec2D newPos) {
        Vec2D p;
        p.x = newPos.x > pos.x ? pos.x : newPos.x;
        p.y = newPos.y > pos.y ? pos.y : newPos.y;
        return AABB(p, (pos - newPos).abs() + size);
    }
    AABB minkowskiDifference(AABB other) {
        Vec2D mPos = pos - other.getMax();
        Vec2D mSize = size + other.size;
        return AABB(mPos, mSize);
    }
    bool colliding(AABB hitbox) {
        if (
            pos.x + size.x >= hitbox.pos.x &&
            pos.x <= hitbox.pos.x + hitbox.size.x &&
            pos.y + size.y >= hitbox.pos.y &&
            pos.y <= hitbox.pos.y + hitbox.size.y
            ) { // AABB collision occured
            return true;
        }
        return false;
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
            if (abs((getMax().x - relativePoint.x) / vel.x)  < minTime) { // right edge
                minTime = abs((getMax().x - relativePoint.x) / vel.x);
                pv = Vec2D(getMax().x, relativePoint.y);
            }
        }
        if (abs(vel.y) != 0) {
            if (abs((getMax().y - relativePoint.y) / vel.y) < minTime) { // bottom edge
                minTime = abs((getMax().y - relativePoint.y) / vel.y);
                pv = Vec2D(relativePoint.x, getMax().y);
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
    float rayIntersectFraction(Vec2D originA, Vec2D endA, Vec2D originB, Vec2D endB) {
        Vec2D r = endA - originA;
        Vec2D s = endB - originB;
        float numerator = (originB - originA).crossProductArea(r);
        float denominator = r.crossProductArea(s);

        if (numerator == 0 && denominator == 0) { // lines are co-linear
            return std::numeric_limits<float>::infinity();
        }
        if (denominator == 0) { // lines are parallel
            return std::numeric_limits<float>::infinity();
        }

        float u = numerator / denominator;
        float t = (originB - originA).crossProductArea(s) / denominator;
        if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
            return t;
        }
        return std::numeric_limits<float>::infinity();
    }
    float sweepingIntersectFractionX(Vec2D relativePoint, Vec2D relativeMotion) {
        Vec2D end = relativePoint + relativeMotion;
        // for each of the AABB's four edges, calculate the minimum fraction of "direction" in order to find where the ray FIRST intersects (if it ever does)
        float minT = rayIntersectFraction(relativePoint, end, pos, Vec2D(pos.x, getMax().y)); // left edge
        float x = rayIntersectFraction(relativePoint, end, Vec2D(getMax().x, getMax().y), Vec2D(getMax().x, pos.y)); // right edge
        return x < minT ? x : minT;
    }
    float sweepingIntersectFractionY(Vec2D relativePoint, Vec2D relativeMotion) {
        Vec2D end = relativePoint + relativeMotion;
        // for each of the AABB's four edges, calculate the minimum fraction of "direction" in order to find where the ray FIRST intersects (if it ever does)
        float minT = rayIntersectFraction(relativePoint, end, Vec2D(pos.x, getMax().y), Vec2D(getMax().x, getMax().y)); // bottom edge
        float x = rayIntersectFraction(relativePoint, end, Vec2D(getMax().x, pos.y), pos); // top edge
        return x < minT ? x : minT;
    }
    float sweepingIntersectFraction(Vec2D relativePoint, Vec2D relativeMotion) {
        Vec2D end = relativePoint + relativeMotion;
        // for each of the AABB's four edges, calculate the minimum fraction of "direction" in order to find where the ray FIRST intersects (if it ever does)
        float minT = rayIntersectFraction(relativePoint, end, pos, Vec2D(pos.x, getMax().y)); // left edge
        float x = rayIntersectFraction(relativePoint, end, Vec2D(pos.x, getMax().y), Vec2D(getMax().x, getMax().y)); // bottom edge
        if (x < minT) {
            minT = x;
        }
        x = rayIntersectFraction(relativePoint, end, Vec2D(getMax().x, getMax().y), Vec2D(getMax().x, pos.y)); // right edge
        if (x < minT) {
            minT = x;
        } 
        x = rayIntersectFraction(relativePoint, end, Vec2D(getMax().x, pos.y), pos); // top edge
        if (x < minT) { 
            minT = x;
        }
        return minT; // return the fractional component along the collided ray
    }
    SDL_Rect* AABBtoRect() {
        return new SDL_Rect{ (int)round(pos.x), (int)round(pos.y), (int)round(size.x), (int)round(size.y) };
    }
};