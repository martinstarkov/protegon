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
	std::uint32_t slot{ 0 };

	constexpr bool operator==(const ActiveTexture&) const = default;
};

struct TextureUnitState {
	TextureId id{};

	TextureParams params{
		.min_filter = TextureMinFilter::Linear,
		.mag_filter = TextureMagFilter::Linear,
		.wrap_s = TextureWrap::Repeat,
		.wrap_t = TextureWrap::Repeat,
	};

	constexpr bool operator==(const TextureUnitState&) const = default;
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

	RenderState render_state{};

	FramebufferId framebuffer{};
	RenderbufferId renderbuffer{};
	VertexBufferId vertex_buffer{};
	UniformBufferId uniform_buffer{};
	ShaderId shader_program{};
	VertexArrayId vertex_array{};

	ActiveTexture active_texture{};
	TextureUnits texture_units{};

	Depth clear_depth{};
	Stencil clear_stencil{};
	Color clear_color{};

	constexpr bool operator==(const State&) const = default;
};

} // namespace ptgn::impl::gl