#pragma once

#include <array>
#include <vector>

#include "core/transform.h"
#include "math/vector2.h"
#include "renderer/origin.h"
#include "serialization/serializable.h"

namespace ptgn {

struct Rect;

namespace impl {

[[nodiscard]] std::array<V2_float, 4> GetVertices(const Transform& transform, Rect rect);

} // namespace impl

[[nodiscard]] V2_float GetPolygonCenter(const V2_float* vertices, std::size_t vertex_count);

struct Triangle {
	Triangle() = default;
	Triangle(const V2_float& a, const V2_float& b, const V2_float& c);
	explicit Triangle(const std::array<V2_float, 3>& vertices);

	friend bool operator==(const Triangle& a, const Triangle& b) {
		return a.vertices == b.vertices;
	}

	friend bool operator!=(const Triangle& a, const Triangle& b) {
		return !(a == b);
	}

	std::array<V2_float, 3> vertices;

	PTGN_SERIALIZER_REGISTER(Triangle, vertices)
};

struct Rect {
	Rect() = default;
	Rect(const V2_float& size, Origin origin = Origin::Center);

	friend bool operator==(const Rect& a, const Rect& b) {
		return a.size == b.size && a.origin == b.origin;
	}

	friend bool operator!=(const Rect& a, const Rect& b) {
		return !(a == b);
	}

	// @return Half the size of the rectangle.
	[[nodiscard]] V2_float Half() const;

	// @return How much needs to be added to the position to get the center of the rect.
	[[nodiscard]] V2_float GetCenterOffset() const;

	V2_float size;
	Origin origin{ Origin::Center };

	PTGN_SERIALIZER_REGISTER(Rect, size, origin)
};

/*
struct RoundedRect : public Rect {
	float radius{ 0.0f };

	RoundedRect() = default;

	explicit RoundedRect(
		const V2_float& position, float radius = 0.0f, const V2_float& size = {},
		Origin origin = Origin::Center, float rotation = 0.0f
	);

	// @return The inner rectangle which consists of the the vertices connecting the corner arc
	// start points, i.e. the asterisk * region in the following illustration.
	//  /-------\
	//  |*******|
	//  |*******|
	//  \-------/
	[[nodiscard]] Rect GetInnerRect() const;

	// @return The outer rectangle which contains the entirely of the rounded rectangle as if it had
	// no radius.
	[[nodiscard]] Rect GetOuterRect() const;
};
*/

struct Polygon {
	Polygon() = default;
	explicit Polygon(const std::vector<V2_float>& vertices);

	friend bool operator==(const Polygon& a, const Polygon& b) {
		return a.vertices == b.vertices;
	}

	friend bool operator!=(const Polygon& a, const Polygon& b) {
		return !(a == b);
	}

	// @return True if all the interior angles are less than 180 degrees.
	[[nodiscard]] bool IsConvex() const;

	// @return True if any of the interior angles are above 180 degrees.
	[[nodiscard]] bool IsConcave() const;

	std::vector<V2_float> vertices;

	PTGN_SERIALIZER_REGISTER(Polygon, vertices)
};

namespace impl {

[[nodiscard]] float TriangulateArea(const V2_float* contour, std::size_t count);

// InsideTriangle decides if a point P is Inside of the triangle defined by A, B, C.
[[nodiscard]] bool TriangulateInsideTriangle(
	float Ax, float Ay, float Bx, float By, float Cx, float Cy, float Px, float Py
);

[[nodiscard]] bool TriangulateSnip(
	const V2_float* contour, int u, int v, int w, int n, const std::vector<int>& V
);

} // namespace impl

// @return A vector of triangles which make up the polygon contour.
[[nodiscard]] std::vector<std::array<V2_float, 3>> Triangulate(
	const V2_float* vertices, std::size_t vertex_count
);

[[nodiscard]] V2_float GetCenter(const Transform& transform, Rect rect);

} // namespace ptgn