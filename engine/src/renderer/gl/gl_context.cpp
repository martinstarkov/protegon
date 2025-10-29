#include "renderer/gl/gl_context.h"

#include <ostream>

#include "core/util/macro.h"
#include "debug/core/log.h"
#include "debug/runtime/assert.h"
#include "renderer/gl/gl.h"
#include "SDL_error.h"
#include "SDL_video.h"

#define PTGN_VSYNC_MODE -1

namespace ptgn::impl::gl {

struct GLVersion {
	GLVersion() {
		int r = SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
		PTGN_ASSERT(!r, SDL_GetError());
		r = SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
		PTGN_ASSERT(!r, SDL_GetError());
	}

	int major{ 0 };
	int minor{ 0 };
};

inline std::ostream& operator<<(std::ostream& os, const GLVersion& v) {
	os << v.major << "." << v.minor;
	return os;
}

// Must be called after SDL and window have been initialized.
void GLContext::LoadGLFunctions() {
#ifdef PTGN_PLATFORM_MACOS
	return;
#endif

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

	// PTGN_LOG("OpenGL Build: ", GLCall(glGetString(GL_VERSION)));

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
}

GLContext::GLContext(SDL_Window* window) {
	if (context_ != nullptr) {
		int result = SDL_GL_MakeCurrent(window, context_);
		PTGN_ASSERT(!result, SDL_GetError());
		return;
	}

	context_ = SDL_GL_CreateContext(window);
	PTGN_ASSERT(context_, SDL_GetError());

	GLVersion gl_version;

	PTGN_INFO("Initialized OpenGL version: ", gl_version);
	PTGN_INFO("Created OpenGL context");

	// From: https://nullprogram.com/blog/2023/01/08/
	// Set a non-zero SDL_GL_SetSwapInterval so that SDL_GL_SwapWindow synchronizes.
	if (!SDL_GL_SetSwapInterval(PTGN_VSYNC_MODE)) {
		// If no adaptive VSYNC available, fallback to VSYNC.
		SDL_GL_SetSwapInterval(1);
	}

	LoadGLFunctions();
}

GLContext::~GLContext() {
	if (context_) {
		SDL_GL_DeleteContext(context_);
		context_ = nullptr;
		PTGN_INFO("Destroyed OpenGL context");
	}
}

} // namespace ptgn::impl::gl
