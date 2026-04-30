#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
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

	/// @brief Constructs a default texture unit state with all values set to their OpenGL defaults.
	explicit TextureUnitState(bool) : id{ TextureId{ 0 } } {}

	std::optional<TextureId> id;

	std::optional<TextureMinFilter> min_filter{ TextureMinFilter::Linear };
	std::optional<TextureMagFilter> mag_filter{ TextureMagFilter::Linear };
	std::optional<TextureWrap> wrap_s{ TextureWrap::Repeat };
	std::optional<TextureWrap> wrap_t{ TextureWrap::Repeat };

	bool operator==(const TextureUnitState&) const = default;
};

using TextureUnits = std::vector<TextureUnitState>;

struct State {
	State() = default;

	/// @brief Constructs a default state with all values set to their OpenGL defaults.
	explicit State(std::size_t max_texture_slots) :
		framebuffer{ FramebufferId{ 0 } },
		renderbuffer{ RenderbufferId{ 0 } },
		vertex_buffer{ VertexBufferId{ 0 } },
		uniform_buffer{ UniformBufferId{ 0 } },
		shader_program{ ShaderId{ 0 } },
		vertex_array{ VertexArrayId{ 0 } },
		viewport{ Viewport{ { 0, 0 }, { 0, 0 } } },
		depth_testing{ false },
		blend{ false },
		depth_mask{ DepthMaskState{} },
		blend_mode{ BlendMode::ReplaceRGBA },
		color_mask{ ColorMaskState{} },
		active_texture{ ActiveTexture{ 0 } },
		texture_units(max_texture_slots, TextureUnitState{ true }),
		scissor{ ScissorState{ false } },
		raster{ RasterState{} },
		stencil{ StencilState{} },
		clear_depth{ ClearDepth{ 1.0 } },
		clear_stencil{ 0 },
		clear_color{ Color{ 0, 0, 0, 0 } } {
		PTGN_ASSERT(max_texture_slots > 0);
	}

	// Core object bindings
	std::optional<FramebufferId> framebuffer;
	std::optional<RenderbufferId> renderbuffer;
	std::optional<VertexBufferId> vertex_buffer;
	std::optional<UniformBufferId> uniform_buffer;
	std::optional<ShaderId> shader_program;
	std::optional<VertexArrayId> vertex_array;

	std::optional<Viewport> viewport;

	std::optional<bool> depth_testing;
	std::optional<bool> blend;
	std::optional<DepthMaskState> depth_mask;
	std::optional<BlendMode> blend_mode;

	std::optional<ColorMaskState> color_mask;

	ActiveTexture active_texture;
	TextureUnits texture_units;

	std::optional<ScissorState> scissor;

	// Polygon rasterization
	std::optional<RasterState> raster;

	std::optional<StencilState> stencil;

	std::optional<ClearDepth> clear_depth;
	std::optional<int> clear_stencil;
	std::optional<Color> clear_color;

	bool operator==(const State&) const = default;

	void Invalidate() {
		std::size_t max_texture_slots{ texture_units.size() };

		*this = {};

		texture_units.resize(max_texture_slots, {});
	}
};

} // namespace ptgn::impl::gl