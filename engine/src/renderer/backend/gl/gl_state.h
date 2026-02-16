#pragma once

#include <vector>

#include "renderer/resources/handle.h"
#include "renderer/camera/viewport.h"
#include "renderer/resources/render_state.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl::gl {

struct ActiveTextureSlot {
	ActiveTextureSlot() = default;

	explicit ActiveTextureSlot(Id value) : value{ value } {}

	Id value{ 0 };
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
	FramebufferId framebuffer;
	RenderbufferId renderbuffer;
	VertexBufferId vertex_buffer;
	UniformBufferId uniform_buffer;
	ShaderId shader;
	VertexArrayId vertex_array;

	Viewport viewport;

	DepthState depth;

	BlendState blend;

	ColorMaskState color_mask;

	ActiveTextureSlot active_texture_slot;
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