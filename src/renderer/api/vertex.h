#pragma once

#include "renderer/gl/gl_types.h"

namespace ptgn::impl {

struct Vertex {
	glsl::vec3 position{};
	glsl::vec4 color{};
	glsl::vec2 tex_coord{};
	// For textures this is from 1 to max_texture_slots.
	// For solid triangles/quads this is 0 (white 1x1 texture).
	// For circles this stores the thickness: 0 is hollow, 1 is solid.
	glsl::float_ tex_index{};
};

} // namespace ptgn::impl