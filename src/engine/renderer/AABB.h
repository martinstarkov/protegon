#pragma once

#include <engine/renderer/Ray2D.h>
#include <engine/renderer/Shape.h>

#include <engine/utils/Vector2.h>

struct AABB : Shape<AABB> {
	V2_double position;
	V2_double size;
	AABB() : position{}, size{} {}
	AABB(V2_double position, V2_double size) : position{ position }, size{ size } {}
	AABB(int x, int y, int w, int h) : position{ x, y }, size{ w, h } {}
	AABB ExpandedBy(const AABB& other) const {
		return { position - other.size / 2, size + other.size };
	}
	V2_double Center() {
		return position + size / 2.0;
	}
	V2_double Center() const {
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
		o.position = j.at("position").get<V2_double>();
	}
	if (j.find("size") != j.end()) {
		o.size = j.at("size").get<V2_double>();
	}
}