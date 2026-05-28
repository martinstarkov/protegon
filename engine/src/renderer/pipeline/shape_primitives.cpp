#include "renderer/pipeline/shape_primitives.h"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/fill_style.h"
#include "core/math/angle.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/geometry_utils.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/math_utils.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/vertex.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl {

namespace {

template <RenderPrimitive TPrimitive>
std::array<TPrimitive, 1> CreatePrimitiveArray(TPrimitive primitive) {
	return std::array<TPrimitive, 1>{ std::move(primitive) };
}

RenderQuadArray<ColorVertex> CreateColorQuadPrimitive(
	const std::array<V2_float, 4>& vertices, const CommonShapeParams& params
) {
	return CreatePrimitiveArray(
		CreateColorQuad(vertices, params.depth, params.color, params.entity_id)
	);
}

RenderTriangleArray<ColorVertex> CreateColorTrianglePrimitive(
	const std::array<V2_float, 3>& vertices, const CommonShapeParams& params
) {
	return CreatePrimitiveArray(
		CreateColorTriangle(vertices, params.depth, params.color, params.entity_id)
	);
}

RenderQuadArray<ShapeVertex> CreateShapeQuadPrimitive(
	const std::array<V2_float, 4>& vertices, const std::array<V2_float, 4>& local_coords,
	const std::array<float, 4>& data, const CommonShapeParams& params
) {
	return CreatePrimitiveArray(
		CreateShapeQuad(vertices, params.depth, params.color, local_coords, data, params.entity_id)
	);
}

struct SDFRoundData {
	float diameter{ 0.0f };
	float fade{ 0.0f };
	float normalized_radius{ 0.0f };
	float aspect_ratio{ 1.0f };
	float thickness{ 0.0f };
};

std::array<V2_float, 4> GetAspectScaledTexCoords(float aspect_ratio) {
	auto tex_coords{ GetNDCTextureCoordinates() };

	for (auto& coord : tex_coords) {
		coord.y *= aspect_ratio;
	}

	return tex_coords;
}

float GetFade(float diameter_y) {
	PTGN_ASSERT(diameter_y > 0.0f, "Diameter cannot be negative or zero");
	constexpr float fade_scaling_constant{ 0.12f };
	return fade_scaling_constant / diameter_y;
}

float GetFade(V2_float diameter) {
	return GetFade(diameter.y);
}

float GetAspectRatio(V2_float size) {
	PTGN_ASSERT(size.x > 0.0f);
	return size.y / size.x;
}

float GetNormalizedRadius(float diameter, float size_x) {
	PTGN_ASSERT(size_x > 0.0f);
	float normalized_radius{ diameter / size_x };
	return Clamp01(normalized_radius);
}

SDFRoundData GetSDFRoundData(float radius, V2_float size, FillStyle fill_style) {
	float diameter{ 2.0f * radius };
	float fade{ GetFade(diameter) };
	float normalized_radius{ GetNormalizedRadius(diameter, size.x) };
	float aspect_ratio{ GetAspectRatio(size) };
	float thickness{ fill_style.NormalizedToSDFThickness(fade, V2_float{ radius }) };

	return SDFRoundData{ .diameter			= diameter,
						 .fade				= fade,
						 .normalized_radius = normalized_radius,
						 .aspect_ratio		= aspect_ratio,
						 .thickness			= thickness };
}

} // namespace

RenderQuadArray<ColorVertex> GetSolidPrimitives(const Rect& rect, const CommonShapeParams& params) {
	auto vertices{ rect.GetWorldVertices(params.transform, params.draw_origin) };
	return CreateColorQuadPrimitive(vertices, params);
}

std::optional<RenderQuadArray<ShapeVertex>> GetSolidPrimitives(
	const RoundedRect& rounded_rect, const CommonShapeParams& params
) {
	V2_float size{ rounded_rect.rect.GetSize(params.transform) };

	if (!size.IsPositive()) {
		return std::nullopt;
	}

	float radius{ rounded_rect.GetRadius(params.transform) };

	if (radius < 0.0f) {
		return std::nullopt;
	}

	auto vertices{ rounded_rect.rect.GetWorldVertices(params.transform, params.draw_origin) };
	auto sdf{ GetSDFRoundData(radius, size, params.fill_style) };

	std::array<float, 4> data{ sdf.thickness * sdf.aspect_ratio, sdf.fade,
							   sdf.normalized_radius * sdf.aspect_ratio, sdf.aspect_ratio };

	auto local_coords{ GetAspectScaledTexCoords(sdf.aspect_ratio) };

	return CreateShapeQuadPrimitive(vertices, local_coords, data, params);
}

RenderTriangleArray<ColorVertex> GetSolidPrimitives(
	const Triangle& triangle, const CommonShapeParams& params
) {
	auto vertices{ triangle.GetWorldVertices(params.transform) };
	return CreateColorTrianglePrimitive(vertices, params);
}

std::vector<ColorTriangle> GetSolidPrimitives(
	const Polygon& polygon, const CommonShapeParams& params
) {
	if (polygon.vertices.size() < 3) {
		return {};
	}

	auto triangles{ Triangulate(polygon.vertices) };

	if (triangles.empty()) {
		return {};
	}

	std::vector<ColorTriangle> primitives;
	primitives.reserve(triangles.size());

	params.transform.WithPointTransform<Transform::Direction::Forward>([&primitives, &triangles,
																		&params](auto&& transform) {
		for (auto& triangle : triangles) {
			for (auto& vertex : triangle.vertices) {
				vertex = transform(vertex);
			}
			primitives.emplace_back(
				CreateColorTriangle(triangle.vertices, params.depth, params.color, params.entity_id)
			);
		}
	});

	PTGN_ASSERT(primitives.size() == triangles.size());

	return primitives;
}

std::vector<ColorQuad> GetSolidPrimitives(const Line& line, const CommonShapeParams& params) {
	auto points{ line.GetLocalVertices() };

	return GetHollowPrimitives(points, false, params);
}

RenderQuadArray<ColorVertex> GetSolidPrimitives(
	const V2_float& point, const CommonShapeParams& params
) {
	Rect rect{ V2_float{ 1.0f } };

	// Must copy entire struct since it is passed to CreateColorQuadPrimitive.
	CommonShapeParams primitive_params{ params };
	primitive_params.transform.Translate(point);

	auto vertices{ rect.GetWorldVertices(primitive_params.transform, Origin::Center) };
	return CreateColorQuadPrimitive(vertices, primitive_params);
}

std::optional<RenderQuadArray<ShapeVertex>> GetSolidPrimitives(
	const Circle& circle, const CommonShapeParams& params
) {
	return GetSolidPrimitives(Ellipse{ circle.radius }, params);
}

std::optional<RenderQuadArray<ShapeVertex>> GetSolidPrimitives(
	const Ellipse& ellipse, const CommonShapeParams& params
) {
	V2_float radius{ ellipse.GetRadius(params.transform) };

	if (!radius.IsPositive()) {
		return std::nullopt;
	}

	V2_float diameter{ 2.0f * radius };
	float fade{ GetFade(diameter) };
	float thickness{ params.fill_style.NormalizedToSDFThickness(fade, radius) };

	std::array<float, 4> data{ thickness, fade, 0.0f, 0.0f };

	auto vertices{ ellipse.GetWorldQuadVertices(params.transform) };
	auto local_coords{ GetNDCTextureCoordinates() };

	return CreateShapeQuadPrimitive(vertices, local_coords, data, params);
}

std::optional<RenderQuadArray<ShapeVertex>> GetSolidPrimitives(
	const Capsule& capsule, const CommonShapeParams& params
) {
	float radius{ capsule.GetRadius(params.transform) };

	if (radius <= 0.0f) {
		return std::nullopt;
	}

	V2_float size;
	auto vertices{ capsule.GetWorldQuadVertices(params.transform, &size) };

	if (!size.IsPositive()) {
		return std::nullopt;
	}

	auto sdf{ GetSDFRoundData(radius, size, params.fill_style) };

	std::array<float, 4> data{ sdf.thickness, sdf.fade, sdf.normalized_radius, sdf.aspect_ratio };

	auto local_coords{ GetAspectScaledTexCoords(sdf.aspect_ratio) };

	return CreateShapeQuadPrimitive(vertices, local_coords, data, params);
}

std::optional<RenderQuadArray<ShapeVertex>> GetSolidPrimitives(
	const Arc& arc, const CommonShapeParams& params
) {
	float radius{ arc.GetRadius(params.transform) };

	if (radius <= 0.0f) {
		return std::nullopt;
	}

	float diameter{ 2.0f * radius };
	float fade{ GetFade(diameter) };
	float thickness{ params.fill_style.NormalizedToSDFThickness(fade, V2_float{ radius }) };

	float start_angle{ arc.start_angle.value };
	float signed_aperture{ arc.GetAperture().ToRad().value * (arc.clockwise ? -1.0f : 1.0f) };

	std::array<float, 4> data{ thickness, fade, start_angle, signed_aperture };

	auto vertices{ arc.GetWorldQuadVertices(params.transform) };
	auto local_coords{ GetNDCTextureCoordinates() };

	return CreateShapeQuadPrimitive(vertices, local_coords, data, params);
}

std::vector<ColorQuad> GetHollowPrimitives(
	std::span<const V2_float> points, bool closed, const CommonShapeParams& params
) {
	auto line_width{ params.fill_style.GetLineWidth() };

	PTGN_ASSERT(line_width.has_value(), "Cannot get use solid fill style for lines");

	PTGN_ASSERT(*line_width >= kMinLineWidth, "Line width must be at least ", kMinLineWidth);

	auto count{ points.size() };

	PTGN_ASSERT(
		(closed && count >= 3) || (!closed && count >= 2),
		"There must be at least two points to draw a line, or three to connect back to the first "
		"point"
	);

	if (*line_width < kMinLineWidth) {
		return {};
	}

	if ((closed && count < 3) || (!closed && count < 2)) {
		return {};
	}

	auto segment_count{ closed ? count : count - 1 };

	std::vector<ColorQuad> primitives;
	primitives.reserve(segment_count);

	for (auto i{ 0uz }; i < segment_count; ++i) {
		auto next{ (i + 1) % count };

		Line line{ points[i], points[next] };
		auto vertices{ line.GetWorldQuadVertices(params.transform, *line_width) };

		primitives.emplace_back(
			CreateColorQuad(vertices, params.depth, params.color, params.entity_id)
		);
	}

	return primitives;
}

std::vector<ColorQuad> GetHollowPrimitives(const Rect& rect, const CommonShapeParams& params) {
	auto points{ rect.GetLocalVertices() };
	return GetHollowPrimitives(points, true, params);
}

std::optional<RenderQuadArray<ShapeVertex>> GetHollowPrimitives(
	const RoundedRect& rounded_rect, const CommonShapeParams& params
) {
	return GetSolidPrimitives(rounded_rect, params);
}

std::vector<ColorQuad> GetHollowPrimitives(
	const Triangle& triangle, const CommonShapeParams& params
) {
	auto vertices{ triangle.GetWorldVertices(params.transform) };

	CommonShapeParams stroke_params{ params };
	stroke_params.transform = Transform{};

	return GetHollowPrimitives(vertices, true, stroke_params);
}

std::vector<ColorQuad> GetHollowPrimitives(
	const Polygon& polygon, const CommonShapeParams& params
) {
	if (polygon.vertices.size() < 3) {
		return {};
	}

	return GetHollowPrimitives(polygon.vertices, true, params);
}

std::vector<ColorQuad> GetHollowPrimitives(const Line& line, const CommonShapeParams& params) {
	return GetSolidPrimitives(line, params);
}

RenderQuadArray<ColorVertex> GetHollowPrimitives(
	const V2_float& point, const CommonShapeParams& params
) {
	return GetSolidPrimitives(point, params);
}

std::optional<RenderQuadArray<ShapeVertex>> GetHollowPrimitives(
	const Circle& circle, const CommonShapeParams& params
) {
	return GetSolidPrimitives(circle, params);
}

std::optional<RenderQuadArray<ShapeVertex>> GetHollowPrimitives(
	const Ellipse& ellipse, const CommonShapeParams& params
) {
	return GetSolidPrimitives(ellipse, params);
}

std::optional<RenderQuadArray<ShapeVertex>> GetHollowPrimitives(
	const Capsule& capsule, const CommonShapeParams& params
) {
	return GetSolidPrimitives(capsule, params);
}

std::optional<RenderQuadArray<ShapeVertex>> GetHollowPrimitives(
	const Arc& arc, const CommonShapeParams& params
) {
	return GetSolidPrimitives(arc, params);
}

} // namespace ptgn::impl