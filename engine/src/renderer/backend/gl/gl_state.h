#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/graphics/color.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture_format.h"
#include "serialization/serialize.h"

namespace ptgn::impl::gl {

struct ActiveTexture {
	ActiveTexture() = default;

	explicit ActiveTexture(std::uint32_t slot) : slot{ slot } {}

	std::uint32_t slot{ 0 };

	bool operator==(const ActiveTexture&) const = default;

	PTGN_SERIALIZE(ActiveTexture, slot)
};

struct TextureUnitState {
	std::optional<TextureId> id;

	std::optional<TextureMinFilter> min_filter{ TextureMinFilter::Linear };
	std::optional<TextureMagFilter> mag_filter{ TextureMagFilter::Linear };
	std::optional<TextureWrap> wrap_s{ TextureWrap::Repeat };
	std::optional<TextureWrap> wrap_t{ TextureWrap::Repeat };

	bool operator==(const TextureUnitState&) const = default;

	PTGN_SERIALIZE(TextureUnitState, id, min_filter, mag_filter, wrap_s, wrap_t)
};

using TextureUnits = std::vector<TextureUnitState>;

struct State {
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

	PTGN_SERIALIZE(
		State, framebuffer, renderbuffer, vertex_buffer, uniform_buffer, shader_program,
		vertex_array, viewport, depth_testing, blend, depth_mask, blend_mode, color_mask,
		active_texture, texture_units, scissor, raster, stencil, clear_depth, clear_stencil,
		clear_color
	)
};

} // namespace ptgn::impl::gl