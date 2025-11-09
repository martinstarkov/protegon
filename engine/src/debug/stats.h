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
	std::int32_t render_buffer_binds{ 0 };
	std::int32_t blend_mode_changes{ 0 };
	std::int32_t viewport_changes{ 0 };
	std::int32_t clears{ 0 };
	std::int32_t clear_colors{ 0 };
	std::int32_t draw_calls{ 0 };
	// TODO: Fix. This requires GLCall to have access to stats somehow.
	// std::int32_t gl_calls{ 0 };

	void Reset();

	void ResetRendererRelated();

	void PrintRenderer() const;
};

} // namespace ptgn::impl