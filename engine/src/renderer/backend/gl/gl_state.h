#pragma once

#include <cstdint>
#include <ostream>
#include <vector>

#include "renderer/primitives/id.h"
#include "renderer/primitives/render_state.h"
#include "renderer/primitives/texture_format.h"
#include "renderer/primitives/viewport.h"

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
	TextureId id;

	TextureMinFilter min_filter{ TextureMinFilter::Linear };
	TextureMagFilter mag_filter{ TextureMagFilter::Linear };
	TextureWrap wrap_s{ TextureWrap::Repeat };
	TextureWrap wrap_t{ TextureWrap::Repeat };

	bool operator==(const TextureUnitState&) const = default;
};

using TextureUnits = std::vector<TextureUnitState>;

struct State {
	// Core object bindings
	FramebufferId framebuffer;
	RenderbufferId renderbuffer;
	VertexBufferId vertex_buffer;
	UniformBufferId uniform_buffer;
	ShaderId shader_program;
	VertexArrayId vertex_array;

	Viewport viewport;

	DepthState depth;

	BlendState blend;

	ColorMaskState color_mask;

	ActiveTexture active_texture;
	TextureUnits texture_units;

	ClearColor clear_color;
	ClearDepth clear_depth;
	ClearStencil clear_stencil;

	ScissorState scissor;

	// Polygon rasterization
	RasterState raster;

	StencilState stencil;

	bool operator==(const State&) const = default;
};

} // namespace ptgn::impl::gl