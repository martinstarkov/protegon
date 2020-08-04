#pragma once

#include "common.h"

#include "Vec2D.h"
#include "Ray2D.h"

#include "Shape.h"

struct AABB : Shape<AABB> {
	Vec2D position;
	Vec2D size;
	AABB() : position(Vec2D()), size(Vec2D()) {}
	AABB(Vec2D position, Vec2D size) : position(position), size(size) {}
	AABB(double x, double y, double w, double h) : position(x, y), size(w, h) {}
	AABB expandedBy(const AABB& other) const {
		return AABB(position - other.size / 2.0, size + other.size);
	}
	Vec2D center() {
		return position + size / 2.0;
	}
	Vec2D center() const {
		return position + size / 2.0;
	}
	operator bool() const {
		return position || size;
	}
	friend std::ostream& operator<<(std::ostream& os, const AABB& obj) {
		os << '[' << obj.position << ',' << obj.size << ']';
		return os;
	}
};

inline void to_json(nlohmann::json& j, const AABB& o) {
	j["position"] = o.position;
	j["size"] = o.size;
}
inline void from_json(const nlohmann::json& j, AABB& o) {
	if (j.find("position") != j.end()) {
		o.position = j.at("position").get<Vec2D>();
	}
	if (j.find("size") != j.end()) {
		o.size = j.at("size").get<Vec2D>();
	}
}