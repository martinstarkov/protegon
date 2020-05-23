#pragma once
#include "Vec2D.h"
#include <algorithm>
#include "common.h"

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
    AABB operator+ (AABB b) {
        return AABB(pos + b.pos, size);
    }
    AABB operator* (Vec2D v) {
        return AABB(pos * v, size * v);
    }
    AABB& operator+= (AABB b) {
        *this = *this + b;
        return *this;
    }
    AABB operator+ (Vec2D v) {
        return AABB(pos + v, size);
    }
    AABB& operator+= (Vec2D v) {
        *this = *this + v;
        return *this;
    }
    AABB surroundingBox(AABB b) {
        Vec2D bPos;
        bPos.x = std::min(min().x, b.min().x);
        bPos.y = std::min(min().y, b.min().y);
        Vec2D bSize;
        bSize.x = abs(std::max(max().x, b.max().x) - bPos.x);
        bSize.y = abs(std::max(max().y, b.max().y) - bPos.y);
        return AABB(bPos, bSize);
    }
    Vec2D getPVector() {
        double minDist = abs(min().x);
        Vec2D boundsPoint = Vec2D(min().x, 0.0f);
        if (abs(max().x) < minDist) {
            minDist = abs(max().x);
            boundsPoint = Vec2D(max().x, 0.0f);
        }
        if (abs(max().y) < minDist) {
            minDist = abs(max().y);
            boundsPoint = Vec2D(0.0f, max().y);
        }
        if (abs(min().y) < minDist) {
            minDist = abs(min().y);
            boundsPoint = Vec2D(0.0f, min().y);
        }
        return boundsPoint;
    }
    Vec2D getPVector(Vec2D vel) { // find shortest distance from origin to edge of minkowski difference rectangle
        float minTime = std::numeric_limits<float>::infinity();
        Vec2D pv = Vec2D();
        Vec2D relativePoint = Vec2D();
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
        return pv;
    }
    bool colliding(AABB b) {
        return (min().x <= b.max().x && max().x >= b.min().x) &&
            (min().y <= b.max().y && max().y >= b.min().y);
    }
    int matchingCorners(AABB b) {
        Vec2D localCorners[4] = { max(), Vec2D(min().x, max().y), max(), Vec2D(max().x, min().y) };
        Vec2D foreignCorners[4] = { b.max(), Vec2D(b.min().x, b.max().y), b.max(), Vec2D(b.max().x, b.min().y) };
        int matching = 0;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (localCorners[i].intEqual(foreignCorners[j])) {
                    std::cout << "matching corner: " << localCorners[i] << std::endl;
                    matching++;
                }
            }
        }
        return matching;
    }
    int matchingCoordinates(AABB b, Axis a) {
        int axis = int(a);
        float localCoordinates[2] = { min()[axis], max()[axis] };
        float foreignCoordinates[2] = { b.min()[axis], b.max()[axis] };
        int matching = 0;
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 2; j++) {
                if (int(localCoordinates[i]) == int(foreignCoordinates[j])) {
                    matching++;
                }
            }
        }
        return matching;
    }
    AABB minkowskiDifference(AABB other) {
        Vec2D mPos = pos - other.max();
        Vec2D mSize = size + other.size;
        return AABB(mPos, mSize);
    }
    Vec2D closestPointOnBoundsToPoint(Vec2D point = Vec2D()) {
        float minDist = abs(point.x - min().x);
        Vec2D boundsPoint = Vec2D(min().x, point.y);
        if (abs(max().x - point.x) < minDist) {
            minDist = abs(max().x - point.x);
            boundsPoint = Vec2D(max().x, point.y);
        }
        if (abs(max().y - point.y) < minDist) {
            minDist = abs(max().y - point.y);
            boundsPoint = Vec2D(point.x, max().y);
        }
        if (abs(min().y - point.y) < minDist) {
            minDist = abs(min().y - point.y);
            boundsPoint = Vec2D(point.x, min().y);
        }
        return boundsPoint;
    }
    Vec2D penetrationNormal() {
        // check if the minkowski origin has hit any of the rectangle's sides
        Vec2D normal = Vec2D();
        if (linePoint(min(), Vec2D(min().x, max().y), Vec2D())) { // left
            normal.x = 1;
        }
        if (linePoint(Vec2D(max().x, min().y), max(), Vec2D())) { // right
            normal.x = -1;
        }
        if (linePoint(min(), Vec2D(max().x, min().y), Vec2D())) { // top
            normal.y = 1;
        }
        if (linePoint(Vec2D(min().x, max().y), max(), Vec2D())) { // bottom
            normal.y = -1;
        }
        return normal;
    }

    Vec2D penetrationNormal(Vec2D origin, Vec2D relVel) {
        // check if the minkowski origin has hit any of the rectangle's sides
        Vec2D normal = Vec2D();
        if (lineLine(min(), Vec2D(min().x, max().y), Vec2D(), relVel) && origin.x <= min().x) { // left
            normal.x = 1;
        }
        if (lineLine(Vec2D(max().x, min().y), max(), Vec2D(), relVel) && origin.x >= max().x) { // right
            normal.x = -1;
        }
        if (lineLine(min(), Vec2D(max().x, min().y), Vec2D(), relVel) && origin.y <= min().y) { // top
            normal.y = 1;
        }
        if (lineLine(Vec2D(min().x, max().y), max(), Vec2D(), relVel) && origin.y >= max().y) { // bottom
            normal.y = -1;
        }
        return normal;
    }

    // LINE/POINT
    float linePoint(Vec2D v1, Vec2D v2, Vec2D p) {

        // get distance from the point to the two ends of the line
        float d1 = (p - v1).magnitude();
        float d2 = (p - v2).magnitude();

        // get the length of the line
        float lineLen = (v1 - v2).magnitude();

        // since floats are so minutely accurate, add
        // a little buffer zone that will give collision
        float buffer = 0.1f;    // higher # = less accurate

        // if the two distances are equal to the line's 
        // length, the point is on the line!
        // note we use the buffer here to give a range, 
        // rather than one #
        if (d1 + d2 >= lineLen - buffer && d1 + d2 <= lineLen + buffer) {
            return 1.0f;
        }
        return 0.0f;
    }

    bool lineLine(Vec2D p1, Vec2D p2, Vec2D p3, Vec2D p4) {

        // calculate the direction of the lines
        float uA = ((p4.x - p3.x) * (p1.y - p3.y) - (p4.y - p3.y) * (p1.x - p3.x)) / ((p4.y - p3.y) * (p2.x - p1.x) - (p4.x - p3.x) * (p2.y - p1.y));
        float uB = ((p2.x - p1.x) * (p1.y - p3.y) - (p2.y - p1.y) * (p1.x - p3.x)) / ((p4.y - p3.y) * (p2.x - p1.x) - (p4.x - p3.x) * (p2.y - p1.y));

        // if uA and uB are between 0-1, lines are colliding
        if (uA >= 0 && uA <= 1 && uB >= 0 && uB <= 1) {

            // optionally, draw a circle where the lines meet
            //float intersectionX = p1.x + (uA * (p2.x - p1.x));
            //float intersectionY = p1.y + (uA * (p2.y - p1.y));

            return true;
        }
        return false;
    }


    // LINE/LINE
    bool lineLine(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4) {

        // calculate the direction of the lines
        float uA = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / ((y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1));
        float uB = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / ((y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1));

        // if uA and uB are between 0-1, lines are colliding
        if (uA >= 0 && uA <= 1 && uB >= 0 && uB <= 1) {

            // optionally, draw a circle where the lines meet
            //float intersectionX = x1 + (uA * (x2 - x1));
            //float intersectionY = y1 + (uA * (y2 - y1));

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
    SDL_Rect AABBtoRect() {
        return { (int)round(pos.x), (int)round(pos.y), (int)round(size.x), (int)round(size.y) };
    }
};