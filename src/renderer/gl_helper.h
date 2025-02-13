#pragma once

#include <filesystem>
#include <type_traits>

#include "core/game.h"
#include "core/gl_context.h"
#include "renderer/gl_types.h"
#include "utility/assert.h"

#ifdef PTGN_DEBUG
// Uncomment for debugging purposes
// #define GL_ANNOUNCE_RENDERER_CALLS
// #define GL_ANNOUNCE_VERTEX_ARRAY_CALLS
// #define GL_ANNOUNCE_FRAME_BUFFER_CALLS
// #define GL_ANNOUNCE_BUFFER_CALLS
// #define GL_ANNOUNCE_RENDER_BUFFER_CALLS
// #define GL_ANNOUNCE_SHADER_CALLS
// #define GL_ANNOUNCE_TEXTURE_CALLS
#endif

namespace ptgn::impl {

#ifdef PTGN_DEBUG

#define GLCall(x)                                                     \
	std::invoke([&, fn = PTGN_FUNCTION_NAME()]() {                    \
		++game.stats.gl_calls;                                        \
		ptgn::impl::GLContext::ClearErrors();                         \
		x;                                                            \
		auto errors{ ptgn::impl::GLContext::GetErrors() };            \
		if (!errors.empty()) {                                        \
			ptgn::impl::GLContext::PrintErrors(                       \
				fn, std::filesystem::path(__FILE__), __LINE__, errors \
			);                                                        \
			PTGN_ERROR("OpenGL Error");                               \
		}                                                             \
	})
#define GLCallReturn(x)                                               \
	std::invoke([&, fn = PTGN_FUNCTION_NAME()]() {                    \
		++game.stats.gl_calls;                                        \
		ptgn::impl::GLContext::ClearErrors();                         \
		auto value{ x };                                              \
		auto errors{ ptgn::impl::GLContext::GetErrors() };            \
		if (!errors.empty()) {                                        \
			ptgn::impl::GLContext::PrintErrors(                       \
				fn, std::filesystem::path(__FILE__), __LINE__, errors \
			);                                                        \
			PTGN_ERROR("OpenGL Error");                               \
		}                                                             \
		return value;                                                 \
	})

#else

#define GLCall(x)		x
#define GLCallReturn(x) x

#endif

} // namespace ptgn::impl