#pragma once

#include <cstdint>
#include <string>

#include "core/util/id_map.h"
#include "math/vector2.h"
#include "renderer/gl/gl.h"

namespace ptgn::impl::gl {

struct BufferCache {
	GLenum usage{ GL_STATIC_DRAW };
	std::uint32_t count{ 0 };
};

struct RenderBufferCache {
	V2_int size;
	GLenum internal_format{ GL_RGBA8 };
};

struct TextureCache {
	V2_int size;
	GLenum internal_format{ GL_RGBA8 };
};

struct FrameBufferCache {
	GLuint texture{ 0 };
	GLuint render_buffer{ 0 };
};

// Wrapper for distinguishing between Shader from path construction and Shader
// from source construction.
struct ShaderCode {
	std::string source;
};

struct ShaderCache {
	std::string shader_name;

	// cache needs to be mutable even in const functions.
	mutable IdMap<std::size_t, std::int32_t> uniform_locations;
};

} // namespace ptgn::impl::gl