#pragma once

#include <cstdint>
#include <iosfwd>

namespace ptgn {

namespace impl {

struct Stats {
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

	void ResetRendererRelated() {
		shader_binds		 = 0;
		texture_binds		 = 0;
		buffer_binds		 = 0;
		vertex_array_binds	 = 0;
		frame_buffer_binds	 = 0;
		blend_mode_changes	 = 0;
		viewport_changes	 = 0;
		clears				 = 0;
		clear_colors		 = 0;
		draw_calls			 = 0;
		gl_calls			 = 0;
	}
};

} // namespace impl

inline std::ostream& operator<<(std::ostream& os, const impl::Stats& s) {
	os << "shader_binds: " << s.shader_binds << "\n";
	os << "texture_binds: " << s.texture_binds << "\n";
	os << "buffer_binds: " << s.buffer_binds << "\n";
	os << "vertex_array_binds: " << s.vertex_array_binds << "\n";
	os << "frame_buffer_binds: " << s.frame_buffer_binds << "\n";
	os << "blend_mode_changes: " << s.blend_mode_changes << "\n";
	os << "viewport_changes: " << s.viewport_changes << "\n";
	os << "clears: " << s.clears << "\n";
	os << "clear_colors: " << s.clear_colors << "\n";
	os << "draw_calls: " << s.draw_calls << "\n";
	os << "gl_calls: " << s.gl_calls << "\n";
	return os;
}

} // namespace ptgn
