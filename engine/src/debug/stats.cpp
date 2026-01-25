#include "debug/stats.h"

#include "core/log.h"

namespace ptgn::impl {

void Stats::Reset() {
	ResetRendererRelated();
}

void Stats::ResetRendererRelated() {
	shader_binds		= 0;
	texture_binds		= 0;
	buffer_binds		= 0;
	vertex_array_binds	= 0;
	frame_buffer_binds	= 0;
	render_buffer_binds = 0;
	blend_mode_changes	= 0;
	viewport_changes	= 0;
	clears				= 0;
	clear_colors		= 0;
	draw_calls			= 0;
	// TODO: Fix.
	// gl_calls			= 0;
}

void Stats::PrintRenderer() const {
	PTGN_LOG("shader_binds: ", shader_binds);
	PTGN_LOG("texture_binds: ", texture_binds);
	PTGN_LOG("buffer_binds: ", buffer_binds);
	PTGN_LOG("vertex_array_binds: ", vertex_array_binds);
	PTGN_LOG("frame_buffer_binds: ", frame_buffer_binds);
	PTGN_LOG("render_buffer_binds: ", render_buffer_binds);
	PTGN_LOG("blend_mode_changes: ", blend_mode_changes);
	PTGN_LOG("viewport_changes: ", viewport_changes);
	PTGN_LOG("clears: ", clears);
	PTGN_LOG("clear_colors: ", clear_colors);
	PTGN_LOG("draw_calls: ", draw_calls);
	// TODO: Fix.
	// PTGN_LOG("gl_calls: ", gl_calls);
}

} // namespace ptgn::impl