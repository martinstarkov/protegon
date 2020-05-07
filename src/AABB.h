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
    float sweepingIntersectFraction(Vec2D relativePoint, Vec2D relativeMotion) {
        Vec2D end = relativePoint + relativeMotion;
        // for each of the AABB's four edges, calculate the minimum fraction of "direction" in order to find where the ray FIRST intersects (if it ever does)
        float minT = rayIntersectFraction(relativePoint, end, Vec2D(pos.x, pos.y), Vec2D(pos.x, getMax().y));
        float x = rayIntersectFraction(relativePoint, end, Vec2D(pos.x, getMax().y), Vec2D(getMax().x, getMax().y));
        if (x < minT) {
            minT = x;
        }
        x = rayIntersectFraction(relativePoint, end, Vec2D(getMax().x, getMax().y), Vec2D(getMax().x, pos.y));
        if (x < minT) {
            minT = x;
        }
        x = rayIntersectFraction(relativePoint, end, Vec2D(getMax().x, pos.y), Vec2D(pos.x, pos.y));
        if (x < minT) {
            minT = x;
        }
        return minT; // return the fractional component along the collided ray
    }
    SDL_Rect* AABBtoRect() {
        return new SDL_Rect{ (int)round(pos.x), (int)round(pos.y), (int)round(size.x), (int)round(size.y) };
    }
};

//Vec2D relativeVelocity = velocity - entity->getVelocity();
//float h = md.sweepingIntersectFraction(Vec2D(), relativeVelocity); // raycast relativeVelocity to origin
//if (h < std::numeric_limits<float>::infinity()) { // intersection occurs
//	hitbox.pos += velocity * h;
//	entity->getHitbox().pos += entity->getVelocity() * h;
//	colliding = true;
//	// zero the normal component of the velocity
//	// (project the velocity onto the tangent of the relative velocities, and only keep the projected component, tossing the normal component)
//	Vec2D tangent = relativeVelocity.unitVector().tangent();
//	//velocity = tangent * velocity.dotProduct(tangent);
//	entity->setVelocity(tangent * entity->getVelocity().dotProduct(tangent));
//} else { // no intersection
//	//hitbox.pos += velocity;
//	entity->getHitbox().pos += entity->getVelocity();
//}