#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "math/vector2.h"
#include "renderer/gl/gl.h"
#include "renderer/gl/gl_handle.h"

namespace ptgn::impl::gl {

struct BufferResource {
	GLuint id{ 0 };
	GLenum usage{ GL_STATIC_DRAW };
	std::uint32_t count{ 0 };
};

struct RenderBufferResource {
	GLuint id{ 0 };
	V2_int size;
	GLenum internal_format{ GL_RGBA8 };
};

struct TextureResource {
	GLuint id{ 0 };
	V2_int size;
	GLenum internal_format{ GL_RGBA8 };
};

struct FrameBufferResource {
	GLuint id{ 0 };
	Handle<Texture> texture;
	Handle<RenderBuffer> render_buffer;
};

struct VertexArrayResource {
	GLuint id{ 0 };
	Handle<VertexBuffer> vertex_buffer;
	Handle<ElementBuffer> element_buffer;
};

struct ShaderResource {
	GLuint id{ 0 };

	std::string shader_name;

	// cache needs to be mutable even in const functions.
	mutable std::unordered_map<std::string, std::int32_t> location_cache;
};

} // namespace ptgn::impl::gl