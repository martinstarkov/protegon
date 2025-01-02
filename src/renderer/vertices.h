#pragma once

#include "renderer/buffer_layout.h"
#include "renderer/gl_helper.h"

namespace ptgn {

struct ColorVertex {
	glsl::vec3 position;
	glsl::vec4 color;
};

constexpr inline const BufferLayout<glsl::vec3, glsl::vec4> color_vertex_layout;

struct QuadVertex {
	glsl::vec3 position;
	glsl::vec4 color;
	glsl::vec2 tex_coord;
	glsl::float_ tex_index;
};

constexpr inline const BufferLayout<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_>
	quad_vertex_layout;

struct CircleVertex {
	glsl::vec3 position;
	glsl::vec4 color;
	glsl::vec3 local_position;
	glsl::float_ line_width;
	glsl::float_ fade;
};

constexpr inline const BufferLayout<glsl::vec3, glsl::vec4, glsl::vec3, glsl::float_, glsl::float_>
	circle_vertex_layout;

} // namespace ptgn