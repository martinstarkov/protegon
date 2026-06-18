#pragma once

#include <cstdint>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture_format.h"

namespace ptgn::impl::gl {

struct ActiveTexture {
	ActiveTexture() = default;

	explicit ActiveTexture(std::uint32_t slot) : slot{ slot } {}

	std::uint32_t slot{ 0 };

	bool operator==(const ActiveTexture&) const = default;
};

struct TextureUnitState {
	TextureUnitState() = default;

	TextureId id;

	TextureMinFilter min_filter{ TextureMinFilter::Linear };
	TextureMagFilter mag_filter{ TextureMagFilter::Linear };
	TextureWrap wrap_s{ TextureWrap::Repeat };
	TextureWrap wrap_t{ TextureWrap::Repeat };

	bool operator==(const TextureUnitState&) const = default;
};

using TextureUnits = std::vector<TextureUnitState>;

struct State {
	State() = default;

	/// @brief Constructs a default state with all values set to OpenGL defaults.
	explicit State(std::size_t max_texture_slots) :
		texture_units(max_texture_slots),
		// Important as Depth default constructs to 0.0, whereas OpenGL default value is 1.0.
		clear_depth{ Depth{ 1.0f } },
		clear_color{ color::Transparent } {
		PTGN_ASSERT(max_texture_slots > 0);
	}

	RenderState render_state;

	FramebufferId framebuffer;
	RenderbufferId renderbuffer;
	VertexBufferId vertex_buffer;
	UniformBufferId uniform_buffer;
	ShaderId shader_program;
	VertexArrayId vertex_array;

	ActiveTexture active_texture;
	TextureUnits texture_units;

	Depth clear_depth;
	Stencil clear_stencil;
	Color clear_color;

	bool operator==(const State&) const = default;
};

} // namespace ptgn::impl::gl