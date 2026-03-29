#include "renderer/backend/gl/gl.h"

#include <SDL3/SDL_video.h>

#include <source_location>
#include <string_view>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/util/macro.h"

#ifndef PTGN_PLATFORM_MACOS

#define GLE(name, caps_name) PFNGL##caps_name##PROC name;
GL_LIST_1
#undef GLE

#endif

#ifdef __EMSCRIPTEN__

#define GLE(name, caps_name) PFNGL##caps_name##OESPROC name;
GL_LIST_2
#undef GLE
#define GLE(name, caps_name) PFNGL##caps_name##EXTPROC name;
GL_LIST_3
#undef GLE

#else

#ifndef PTGN_PLATFORM_MACOS

#define GLE(name, caps_name) PFNGL##caps_name##PROC name;
GL_LIST_2
GL_LIST_3
#undef GLE

#endif

#endif

namespace ptgn::impl::gl {

// Must be called after SDL and window have been initialized.
void LoadGLFunctions() {
#ifdef PTGN_PLATFORM_MACOS
	return;
#else

#define GLE(name, caps_name) \
	name =                   \
		reinterpret_cast<PFNGL##caps_name##PROC>(SDL_GL_GetProcAddress(PTGN_STRINGIFY(gl##name)));
	GL_LIST_1
#undef GLE

#ifndef __EMSCRIPTEN__

#define GLE(name, caps_name) \
	name =                   \
		reinterpret_cast<PFNGL##caps_name##PROC>(SDL_GL_GetProcAddress(PTGN_STRINGIFY(gl##name)));
	GL_LIST_2
	GL_LIST_3
#undef GLE

#else

#define GLE(name, caps_name)                                                                       \
	name =                                                                                         \
		reinterpret_cast<PFNGL##caps_name##OESPROC>(SDL_GL_GetProcAddress(PTGN_STRINGIFY(gl##name) \
		));
	GL_LIST_2
#undef GLE
#define GLE(name, caps_name)                                                                       \
	name =                                                                                         \
		reinterpret_cast<PFNGL##caps_name##EXTPROC>(SDL_GL_GetProcAddress(PTGN_STRINGIFY(gl##name) \
		));
	GL_LIST_3
#undef GLE

#endif

	// For debugging which commands were not initialized.
#define GLE(name, caps_name) PTGN_ASSERT(name, "Failed to load ", PTGN_STRINGIFY(name));
	GL_LIST_1
	GL_LIST_2
	GL_LIST_3
#undef GLE

	// Check that each of the loaded gl functions was found.
#define GLE(name, caps_name) name&&
	bool gl_init = GL_LIST_1 GL_LIST_2 GL_LIST_3 true;
#undef GLE
	PTGN_ASSERT(gl_init, "Failed to load OpenGL functions");
	PTGN_INFO("Loaded all OpenGL functions");
#endif
}

#ifdef PTGN_DEBUG

std::string_view GetErrorString(GLenum error) {
	switch (error) {
		case GL_INVALID_ENUM:				   return "Invalid Enum";
		case GL_INVALID_VALUE:				   return "Invalid Value";
		case GL_INVALID_OPERATION:			   return "Invalid Operation";
		case GL_STACK_OVERFLOW:				   return "Stack Overflow";
		case GL_STACK_UNDERFLOW:			   return "Stack Underflow";
		case GL_OUT_OF_MEMORY:				   return "Out of Memory";
		case GL_INVALID_FRAMEBUFFER_OPERATION: return "Invalid Frame Buffer Operation";
		case GL_NO_ERROR:
			PTGN_ERROR("Cannot retrieve error string for none type error");
			[[fallthrough]];
		default: PTGN_ERROR("Failed to recognize GL error code");
	}
}

void HandleErrors(std::source_location location) {
	std::vector<GLenum> errors;
	while (true) {
		GLenum error{ glGetError() };
		if (error == GL_NO_ERROR) {
			break;
		}
		errors.emplace_back(error);
	}
	if (!errors.empty()) {
		for (auto error : errors) {
			auto error_string{ GetErrorString(error) };
			ptgn::impl::DebugMessage("OPENGL ERROR: ", ToString(error_string), location);
		}
		PTGN_ABORT();
	}
}

#endif

} // namespace ptgn::impl::gl