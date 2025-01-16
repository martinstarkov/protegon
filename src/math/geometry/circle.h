#pragma once

#include <cstdint>
#include <vector>

#include "math/geometry/intersection.h"
#include "math/raycast.h"
#include "math/vector2.h"
#include "math/vector4.h"

namespace ptgn {

struct LayerInfo;
struct Color;
struct Rect;
struct Line;
struct Capsule;
struct RoundedRect;

namespace impl {

class RenderData;

// Fade used with circle shader.
constexpr static float fade_{ 0.005f };

} // namespace impl

struct Circle {
	V2_float center;
	float radius{ 0.0f };

	// Uses default render target.
	void Draw(const Color& color, float line_width = -1.0f) const;

	void Draw(const Color& color, float line_width, const LayerInfo& layer_info) const;

	// center += offset
	void Offset(const V2_float& offset);

	[[nodiscard]] V2_float Center() const;

	[[nodiscard]] bool Overlaps(const V2_float& point) const;
	[[nodiscard]] bool Overlaps(const Line& line) const;
	[[nodiscard]] bool Overlaps(const Circle& circle) const;
	[[nodiscard]] bool Overlaps(const Rect& rect) const;
	[[nodiscard]] bool Overlaps(const Capsule& capsule) const;

	[[nodiscard]] Intersection Intersects(const Circle& circle) const;
	[[nodiscard]] Intersection Intersects(const Rect& rect) const;

	[[nodiscard]] ptgn::Raycast Raycast(const V2_float& ray, const Line& line) const;
	[[nodiscard]] ptgn::Raycast Raycast(const V2_float& ray, const Circle& circle) const;
	[[nodiscard]] ptgn::Raycast Raycast(const V2_float& ray, const Capsule& capsule) const;
	[[nodiscard]] ptgn::Raycast Raycast(const V2_float& ray, const Rect& rect) const;

private:
	void DrawSolid(const V4_float& color, std::int32_t render_layer, impl::RenderData& render_data)
		const;

	void DrawThick(
		float line_width, const V4_float& color, std::int32_t render_layer,
		impl::RenderData& render_data
	) const;
};

struct Arc {
	V2_float center;
	float radius{ 0.0f };

	// Radians counter-clockwise from the right.
	float start_angle{ 0.0f };
	// Radians counter-clockwise from the right.
	float end_angle{ 0.0f };

	// Uses default render target.
	void Draw(bool clockwise, const Color& color, float line_width = -1.0f) const;

	void Draw(bool clockwise, const Color& color, float line_width, const LayerInfo& layer_info)
		const;

private:
	friend struct RoundedRect;
	friend struct Capsule;

	// @param clockwise Whether the vertices are in clockwise direction (true), or counter-clockwise
	// (false).
	// @param sa start_angle clamped between 0 and 2 pi.
	// @param ea end_angle clamped between 0 and 2 pi.
	void DrawSolid(
		bool clockwise, float sa, float ea, const V4_float& color, std::int32_t render_layer,
		impl::RenderData& render_data
	) const;

	void DrawThick(
		float line_width, bool clockwise, float sa, float ea, const V4_float& color,
		std::int32_t render_layer, impl::RenderData& render_data
	) const;

	// @param clockwise Whether the vertices are in clockwise direction (true), or counter-clockwise
	// (false).
	// @param sa start_angle clamped between 0 and 2 pi.
	// @param ea end_angle clamped between 0 and 2 pi.
	// @return The vertices which make up the arc.
	[[nodiscard]] std::vector<V2_float> GetVertices(bool clockwise, float sa, float ea) const;
};

struct Ellipse {
	V2_float center;
	V2_float radius;

	// Uses default render target.
	void Draw(const Color& color, float line_width = -1.0f) const;

	void Draw(const Color& color, float line_width, const LayerInfo& layer_info) const;

private:
	friend struct Circle;

	void DrawSolid(const V4_float& color, std::int32_t render_layer, impl::RenderData& render_data)
		const;

	void DrawThick(
		float line_width, const V4_float& color, std::int32_t render_layer,
		impl::RenderData& render_data
	) const;
};

} // namespace ptgn