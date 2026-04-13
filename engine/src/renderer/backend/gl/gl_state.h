#pragma once

#include <cstdint>
#include <optional>
#include <ostream>
#include <vector>

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

	friend std::ostream& operator<<(std::ostream& os, const ActiveTexture& active_texture) {
		os << "{ slot: " << active_texture.slot << " }";
		return os;
	}
};

struct TextureUnitState {
	std::optional<TextureId> id;

	std::optional<TextureMinFilter> min_filter{ TextureMinFilter::Linear };
	std::optional<TextureMagFilter> mag_filter{ TextureMagFilter::Linear };
	std::optional<TextureWrap> wrap_s{ TextureWrap::Repeat };
	std::optional<TextureWrap> wrap_t{ TextureWrap::Repeat };

	bool operator==(const TextureUnitState&) const = default;
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

	std::optional<DepthState> depth;

	std::optional<BlendState> blend;

	std::optional<ColorMaskState> color_mask;

	std::optional<ActiveTexture> active_texture;
	TextureUnits texture_units;

	std::optional<ScissorState> scissor;

	// Polygon rasterization
	std::optional<RasterState> raster;

	std::optional<StencilState> stencil;

	std::optional<double> clear_depth;
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