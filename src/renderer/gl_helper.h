#pragma once

#include <filesystem>
#include <type_traits>

#include "core/game.h"
#include "core/gl_context.h"
#include "renderer/gl_types.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

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
			PTGN_EXCEPTION("OpenGL Error");                           \
		}                                                             \
	})
#define GLCallReturn(x)                                               \
	std::invoke([&, fn = PTGN_FUNCTION_NAME()]() {                    \
		++game.stats.gl_calls;                                        \
		ptgn::impl::GLContext::ClearErrors();                         \
		auto value	= x;                                              \
		auto errors = ptgn::impl::GLContext::GetErrors();             \
		if (!errors.empty()) {                                        \
			ptgn::impl::GLContext::PrintErrors(                       \
				fn, std::filesystem::path(__FILE__), __LINE__, errors \
			);                                                        \
			PTGN_EXCEPTION("OpenGL Error");                           \
		}                                                             \
		return value;                                                 \
	})

#else

#define GLCall(x)		x
#define GLCallReturn(x) x

#endif

} // namespace impl

} // namespace ptgn