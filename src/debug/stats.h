#pragma once

#include <cstdint>

namespace ptgn::impl {

struct Stats {
	// Draw stats.
	std::int32_t shader_binds{ 0 };
	std::int32_t texture_binds{ 0 };
	std::int32_t buffer_binds{ 0 };
	std::int32_t vertex_array_binds{ 0 };
	std::int32_t frame_buffer_binds{ 0 };
	std::int32_t blend_mode_changes{ 0 };
	std::int32_t viewport_changes{ 0 };
	std::int32_t clears{ 0 };
	std::int32_t clear_colors{ 0 };
	std::int32_t draw_calls{ 0 };
	std::int32_t gl_calls{ 0 };

	std::int32_t overlap_point_line{ 0 };
	std::int32_t overlap_point_circle{ 0 };
	std::int32_t overlap_point_rect{ 0 };
	std::int32_t overlap_point_capsule{ 0 };
	std::int32_t overlap_point_triangle{ 0 };
	std::int32_t overlap_point_polygon{ 0 };

	std::int32_t overlap_line_line{ 0 };
	std::int32_t overlap_line_circle{ 0 };
	std::int32_t overlap_line_rect{ 0 };
	std::int32_t overlap_line_capsule{ 0 };

	std::int32_t raycast_line_line{ 0 };
	std::int32_t raycast_line_circle{ 0 };
	std::int32_t raycast_line_rect{ 0 };
	std::int32_t raycast_line_capsule{ 0 };

	std::int32_t overlap_circle_circle{ 0 };
	std::int32_t overlap_circle_rect{ 0 };
	std::int32_t overlap_circle_capsule{ 0 };

	std::int32_t intersect_circle_circle{ 0 };
	std::int32_t intersect_circle_rect{ 0 };
	std::int32_t intersect_circle_polygon{ 0 };

	std::int32_t raycast_circle_rect{ 0 };

	std::int32_t overlap_triangle_rect{ 0 };
	std::int32_t overlap_triangle_capsule{ 0 };

	std::int32_t overlap_rect_rect{ 0 };
	std::int32_t overlap_rect_capsule{ 0 };

	std::int32_t intersect_rect_rect{ 0 };

	std::int32_t raycast_rect_rect{ 0 };

	std::int32_t overlap_capsule_capsule{ 0 };

	std::int32_t overlap_polygon_polygon{ 0 };

	std::int32_t intersect_polygon_polygon{ 0 };

	void Reset();

	void ResetCollisionRelated();

	void ResetRendererRelated();

	void PrintCollisionOverlap() const;

	void PrintCollisionIntersect() const;

	void PrintCollisionRaycast() const;

	void PrintRenderer() const;
};

} // namespace ptgn::impl