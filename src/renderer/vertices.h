#pragma once

#include "renderer/buffer_layout.h"
#include "renderer/gl_types.h"

namespace ptgn::impl {

struct Vertex {
	glsl::vec3 position;
	glsl::vec4 color;
	glsl::vec2 tex_coord;
	glsl::float_ tex_index;
};

constexpr inline const BufferLayout<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_>
	quad_vertex_layout;

} // namespace ptgn::impl