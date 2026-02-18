#pragma once

#include <cstdint>
#include <vector>

#include "renderer/backend/gl/gl_buffer.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_shader.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/backend/gl/gl_vertex_array.h"
#include "renderer/camera/viewport.h"
#include "renderer/resources/render_state.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl::gl {

struct ActiveTexture {
	ActiveTexture() = default;

	explicit ActiveTexture(std::uint32_t slot) : slot{ slot } {}

	std::uint32_t slot{ 0 };
};

struct TextureUnitState {
	Texture id{ 0 };

	TextureMinFilter min_filter{ TextureMinFilter::Linear };
	TextureMagFilter mag_filter{ TextureMagFilter::Linear };
	TextureWrap wrap_s{ TextureWrap::Repeat };
	TextureWrap wrap_t{ TextureWrap::Repeat };

	bool operator==(const TextureUnitState&) const = default;
};

using TextureUnits = std::vector<TextureUnitState>;

struct State {
	// Core object bindings
	Framebuffer framebuffer{ 0 };
	Renderbuffer renderbuffer{ 0 };
	VertexBuffer vertex_buffer{ 0 };
	UniformBuffer uniform_buffer{ 0 };
	Program shader_program{ 0 };
	VertexArray vertex_array{ 0 };

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