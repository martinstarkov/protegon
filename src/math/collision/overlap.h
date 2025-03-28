#pragma once

#include "core/transform.h"
#include "math/geometry/axis.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"

namespace ptgn {

[[nodiscard]] bool Overlaps(const V2_float& point, const Transform& transform, Line line);
[[nodiscard]] bool Overlaps(const V2_float& point, const Transform& transform, Circle circle);
[[nodiscard]] bool Overlaps(const V2_float& point, const Transform& transform, Triangle triangle);
[[nodiscard]] bool Overlaps(const V2_float& point, Transform transform, Rect rect);
[[nodiscard]] bool Overlaps(const V2_float& point, const Transform& transform, Capsule capsule);

[[nodiscard]] bool Overlaps(const Transform& a, Line A, const Transform& b, Line B);
[[nodiscard]] bool Overlaps(const Transform& a, Line A, const Transform& b, Circle B);
[[nodiscard]] bool Overlaps(const Transform& a, Line A, const Transform& b, Triangle B);
[[nodiscard]] bool Overlaps(const Transform& a, Line A, Transform b, Rect B);
[[nodiscard]] bool Overlaps(const Transform& a, Line A, const Transform& b, Polygon B);

[[nodiscard]] bool Overlaps(const Transform& a, const Circle& A, const Transform& b, const Line& B);
[[nodiscard]] bool Overlaps(const Transform& a, Circle A, const Transform& b, Circle B);
[[nodiscard]] bool Overlaps(const Transform& a, Circle A, const Transform& b, Triangle B);
[[nodiscard]] bool Overlaps(const Transform& a, Circle A, Transform b, Rect B);
[[nodiscard]] bool Overlaps(const Transform& a, Circle A, const Transform& b, Polygon B);

[[nodiscard]] bool Overlaps(
	const Transform& a, const Triangle& A, const Transform& b, const Line& B
);
[[nodiscard]] bool Overlaps(
	const Transform& a, const Triangle& A, const Transform& b, const Circle& B
);
[[nodiscard]] bool Overlaps(const Transform& a, Triangle A, const Transform& b, Triangle B);
[[nodiscard]] bool Overlaps(const Transform& a, Triangle A, Transform b, Rect B);
[[nodiscard]] bool Overlaps(const Transform& a, Triangle A, const Transform& b, Polygon B);

[[nodiscard]] bool Overlaps(Transform a, const Rect& A, const Transform& b, const Line& B);
[[nodiscard]] bool Overlaps(Transform a, const Rect& A, const Transform& b, const Circle& B);
[[nodiscard]] bool Overlaps(Transform a, const Rect& A, const Transform& b, const Triangle& B);
[[nodiscard]] bool Overlaps(Transform a, Rect A, Transform b, Rect B);
[[nodiscard]] bool Overlaps(Transform a, Rect A, const Transform& b, Polygon B);

[[nodiscard]] bool Overlaps(const Transform& a, const Polygon& A, const Transform& b, Line B);
[[nodiscard]] bool Overlaps(const Transform& a, const Polygon& A, const Transform& b, Circle B);
[[nodiscard]] bool Overlaps(const Transform& a, const Polygon& A, const Transform& b, Triangle B);
[[nodiscard]] bool Overlaps(const Transform& a, const Polygon& A, const Transform& b, Rect B);
[[nodiscard]] bool Overlaps(const Transform& a, Polygon A, const Transform& b, Polygon B);

namespace impl {

[[nodiscard]] bool PolygonsHaveOverlapAxis(
	const V2_float* polygonA_vertices, std::size_t polygonA_vertex_count,
	const V2_float* polygonB_vertices, std::size_t polygonB_vertex_count
);

[[nodiscard]] bool GetPolygonMinimumOverlap(
	const V2_float* polygonA_vertices, std::size_t polygonA_vertex_count,
	const V2_float* polygonB_vertices, std::size_t polygonB_vertex_count, float& depth, Axis& axis
);

[[nodiscard]] bool LineContainsLine(
	const V2_float& lineA_start, const V2_float& lineA_end, const V2_float& lineB_start,
	const V2_float& lineB_end
);

[[nodiscard]] bool PolygonContainsPolygon(
	const V2_float* polygonA_vertices, std::size_t polygonA_vertex_count,
	const V2_float* polygonB_vertices, std::size_t polygonB_vertex_count
);

[[nodiscard]] bool TriangleContainsTriangle(
	const V2_float& v1A, const V2_float& v2A, const V2_float& v3A, const V2_float& v1B,
	const V2_float& v2B, const V2_float& v3B
);

[[nodiscard]] bool PolygonContainsTriangle(
	const V2_float* vertices, std::size_t vertex_count, const V2_float& triangle_a,
	const V2_float& triangle_b, const V2_float& triangle_c
);

[[nodiscard]] bool OverlapPointLine(
	const V2_float& point, const V2_float& line_start, const V2_float& line_end
);

[[nodiscard]] bool OverlapPointTriangle(
	const V2_float& point, const V2_float& triangle_a, const V2_float& triangle_b,
	const V2_float& triangle_c
);

[[nodiscard]] bool OverlapPointCircle(
	const V2_float& point, const V2_float& circle_center, float circle_radius
);

[[nodiscard]] bool OverlapPointRect(
	const V2_float& point, const V2_float& rect_center, const V2_float& rect_size,
	float rect_rotation
);

[[nodiscard]] bool OverlapPointCapsule(
	const V2_float& point, const V2_float& capsule_start, const V2_float& capsule_end,
	float capsule_radius
);

[[nodiscard]] bool OverlapPointPolygon(
	const V2_float& point, const V2_float* polygon_vertices, std::size_t polygon_vertex_count
);

[[nodiscard]] bool OverlapLineLine(
	const V2_float& lineA_start, const V2_float& lineA_end, const V2_float& lineB_start,
	const V2_float& lineB_end
);

[[nodiscard]] bool OverlapLineCircle(
	const V2_float& line_start, const V2_float& line_end, const V2_float& circle_center,
	float circle_radius
);

bool OverlapLineTriangle(
	const V2_float& line_start, const V2_float& line_end, const V2_float& triangle_a,
	const V2_float& triangle_b, const V2_float& triangle_c
);

[[nodiscard]] bool OverlapLineRect(
	const V2_float& line_start, const V2_float& line_end, const V2_float& rect_center,
	const V2_float& rect_size
);

[[nodiscard]] bool OverlapLineCapsule(
	const V2_float& line_start, const V2_float& line_end, const V2_float& capsule_start,
	const V2_float& capsule_end, float capsule_radius
);

[[nodiscard]] bool OverlapLinePolygon(
	const V2_float& line_start, const V2_float& line_end, const V2_float* polygon_vertices,
	std::size_t polygon_vertex_count
);

[[nodiscard]] bool OverlapCircleCircle(
	const V2_float& circleA_center, float circleA_radius, const V2_float& circleB_center,
	float circleB_radius
);

[[nodiscard]] bool OverlapCircleTriangle(
	const V2_float& circle_center, float circle_radius, const V2_float& triangle_a,
	const V2_float& triangle_b, const V2_float& triangle_c
);

[[nodiscard]] bool OverlapCircleRect(
	const V2_float& circle_center, float circle_radius, const V2_float& rect_center,
	const V2_float& rect_size
);

[[nodiscard]] bool OverlapCirclePolygon(
	const V2_float& circle_center, float circle_radius, const V2_float* polygon_vertices,
	std::size_t polygon_vertex_count
);

[[nodiscard]] bool OverlapCircleCapsule(
	const V2_float& circle_center, float circle_radius, const V2_float& capsule_start,
	const V2_float& capsule_end, float capsule_radius
);

[[nodiscard]] bool OverlapTriangleTriangle(
	const V2_float& triangleA_a, const V2_float& triangleA_b, const V2_float& triangleA_c,
	const V2_float& triangleB_a, const V2_float& triangleB_b, const V2_float& triangleB_c
);

[[nodiscard]] bool OverlapTriangleRect(
	const V2_float& triangle_a, const V2_float& triangle_b, const V2_float& triangle_c,
	const V2_float& rect_center, const V2_float& rect_size, float rect_rotation
);

[[nodiscard]] bool OverlapTrianglePolygon(
	const V2_float& triangle_a, const V2_float& triangle_b, const V2_float& triangle_c,
	const V2_float* polygon_vertices, std::size_t polygon_vertex_count
);

[[nodiscard]] bool OverlapRectRect(
	const V2_float& rectA_center, const V2_float& rectA_size, float rectA_rotation,
	const V2_float& rectB_center, const V2_float& rectB_size, float rectB_rotation
);

[[nodiscard]] bool OverlapRectCapsule(
	const V2_float& rect_center, const V2_float& rect_size, float rect_rotation,
	const V2_float& capsule_start, const V2_float& capsule_end, float capsule_radius
);

[[nodiscard]] bool OverlapRectPolygon(
	const V2_float& rect_center, const V2_float& rect_size, float rect_rotation,
	const V2_float* polygon_vertices, std::size_t polygon_vertex_count
);

[[nodiscard]] bool OverlapCapsuleCapsule(
	const V2_float& capsuleA_start, const V2_float& capsuleA_end, float capsuleA_radius,
	const V2_float& capsuleB_start, const V2_float& capsuleB_end, float capsuleB_radius
);

[[nodiscard]] bool OverlapPolygonPolygon(
	const V2_float* polygonA_vertices, std::size_t polygonA_vertex_count,
	const V2_float* polygonB_vertices, std::size_t polygonB_vertex_count
);

} // namespace impl

} // namespace ptgn