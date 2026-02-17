#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "core/math/vector2.h"
#include "renderer/backend/gl/gl.h"

namespace ptgn::impl::gl {

using VertexArray	= std::uint32_t;
using VertexBuffer	= std::uint32_t;
using ElementBuffer = std::uint32_t;
using UniformBuffer = std::uint32_t;
using Texture		= std::uint32_t;
using Renderbuffer	= std::uint32_t;
using Framebuffer	= std::uint32_t;

struct BufferCache {
	GLenum usage{ GL_STATIC_DRAW };
	std::uint32_t count{ 0 };
};

struct RenderbufferCache {
	V2_int size;
	GLenum internal_format{ GL_RGBA8 };
};

struct TextureCache {
	V2_int size;
	GLenum internal_format{ GL_RGBA8 };
};

struct AttachmentInfo {
	std::uint32_t id{ 0 };
	GLenum type{ 0 }; // GL_TEXTURE_2D, GL_RENDERBUFFER, or 0 (none)
};

struct FramebufferCache {
	std::array<AttachmentInfo, 8> color;
	AttachmentInfo depth;
	AttachmentInfo stencil;
	AttachmentInfo depth_stencil;
};

struct VertexArrayCache {
	ElementBuffer element_buffer{ 0 };
	bool layout_set{ false };
};

} // namespace ptgn::impl::gl