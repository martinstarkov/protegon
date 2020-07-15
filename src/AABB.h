#pragma once

#include <limits>

#include "common.h"

#include "Vec2D.h"

struct AABB {
	Vec2D position;
	Vec2D size;
	AABB() : position(Vec2D()), size(Vec2D()) {}
	AABB(Vec2D position, Vec2D size) : position(position), size(size) {}
	AABB(double x, double y, double w, double h) : position(x, y), size(w, h) {}
	Vec2D min() {
		return position;
	}
	Vec2D max() {
		return position + size;
	}
	SDL_Rect AABBtoRect() {
		return { static_cast<int>(round(position.x)), static_cast<int>(round(position.y)), static_cast<int>(round(size.x)), static_cast<int>(round(size.y)) };
	}
	AABB minkowskiDifference(AABB other) {
	    return AABB(min() - other.max(), size + other.size);
	}
	Vec2D colliding(AABB other, Vec2D velocity = Vec2D()) { // static collision
		AABB md = minkowskiDifference(other);
		Vec2D penetration = Vec2D();
		if (md.min().x <= 0.0 &&
			md.max().x >= 0.0 &&
			md.min().y <= 0.0 &&
			md.max().y >= 0.0) {
			penetration = md.getPVector(velocity);
		}
		return penetration;
	}
	Vec2D getPenetrationVector(Vec2D velocity = Vec2D()) {
		double minDist = abs(min().x);
		Vec2D boundsPoint = Vec2D(min().x, 0.0);
		if (abs(max().x) < minDist) {
			minDist = abs(max().x);
			boundsPoint = Vec2D(max().x, 0.0);
		}
		if (abs(max().y) < minDist) {
			minDist = abs(max().y);
			boundsPoint = Vec2D(0.0, max().y);
		}
		if (abs(min().y) < minDist) {
			minDist = abs(min().y);
			boundsPoint = Vec2D(0.0, min().y);
		}
		return boundsPoint;
	}
	Vec2D getPVector(Vec2D vel) { // find shortest distance from origin to edge of minkowski difference rectangle
		double minTime = std::numeric_limits<double>::infinity();
		Vec2D pv = Vec2D();
		Vec2D relativePoint = Vec2D();
		if (abs(vel.x) != 0.0) {
			if (abs((relativePoint.x - position.x) / vel.x) < minTime) {
				minTime = abs((relativePoint.x - position.x) / vel.x); // left edge
				pv = Vec2D(position.x, relativePoint.y);
			}
		}
		if (abs(vel.x) != 0.0) {
			if (abs((max().x - relativePoint.x) / vel.x) < minTime) { // right edge
				minTime = abs((max().x - relativePoint.x) / vel.x);
				pv = Vec2D(max().x, relativePoint.y);
			}
		}
		if (abs(vel.y) != 0.0) {
			if (abs((max().y - relativePoint.y) / vel.y) < minTime) { // bottom edge
				minTime = abs((max().y - relativePoint.y) / vel.y);
				pv = Vec2D(relativePoint.x, max().y);
			}
		}
		if (abs(vel.y) != 0.0) {
			if (abs((position.y - relativePoint.y) / vel.y) < minTime) { // top edge
				minTime = abs((position.y - relativePoint.y) / vel.y);
				pv = Vec2D(relativePoint.x, position.y);
			}
		}
		return pv;
	}
	// Operators
	operator bool() const {
		return position || size;
	}
	friend std::ostream& operator<<(std::ostream& os, const AABB& aabb) {
		os << '[' << aabb.position << ',' << aabb.size << ']';
		return os;
	}
};