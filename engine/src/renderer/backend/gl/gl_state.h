#pragma once

#include <cstdint>
#include <ostream>
#include <vector>

#include "renderer/camera/viewport.h"
#include "renderer/resources/buffer.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/render_state.h"
#include "renderer/resources/renderbuffer.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/vertex_array.h"

namespace ptgn::impl::gl {

struct ActiveTexture {
	ActiveTexture() = default;

	explicit ActiveTexture(std::uint32_t slot) : slot{ slot } {}

	std::uint32_t slot{ 0 };

	bool operator==(const ActiveTexture&) const = default;

	friend std::ostream& operator<<(std::ostream& os, const ActiveTexture& active_texture) {
		os << "ActiveTexture(slot=" << active_texture.slot << ")";
		return os;
	}
};

struct TextureUnitState {
	TextureId id{ 0 };

	TextureMinFilter min_filter{ TextureMinFilter::Linear };
	TextureMagFilter mag_filter{ TextureMagFilter::Linear };
	TextureWrap wrap_s{ TextureWrap::Repeat };
	TextureWrap wrap_t{ TextureWrap::Repeat };

	bool operator==(const TextureUnitState&) const = default;
};

using TextureUnits = std::vector<TextureUnitState>;

struct State {
	// Core object bindings
	FramebufferId framebuffer{ 0 };
	RenderbufferId renderbuffer{ 0 };
	VertexBufferId vertex_buffer{ 0 };
	UniformBufferId uniform_buffer{ 0 };
	ShaderId shader_program{ 0 };
	VertexArrayId vertex_array{ 0 };

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