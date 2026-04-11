#pragma once

#include <glad/gl.h>

#include <source_location>
#include <string_view>

#include "core/config.h"

#ifdef PTGN_DEBUG

namespace ptgn::impl::gl {

inline void ClearErrors() {
	while (glGetError() != GL_NO_ERROR) { /* glGetError clears the error queue */
	}
}

std::string_view GetErrorString(GLenum error);

void HandleErrors(std::source_location location = std::source_location::current());

} // namespace ptgn::impl::gl

#define GLCall(x)                    \
	::ptgn::impl::gl::ClearErrors(); \
	x;                               \
	::ptgn::impl::gl::HandleErrors()

#define GLCallReturn(x)                   \
	std::invoke([&]() {                   \
		::ptgn::impl::gl::ClearErrors();  \
		auto value = x;                   \
		::ptgn::impl::gl::HandleErrors(); \
		return value;                     \
	})

#else

#define GLCall(x)		x
#define GLCallReturn(x) x

#endif