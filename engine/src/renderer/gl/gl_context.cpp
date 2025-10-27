#include "renderer/gl/gl_context.h"

#include <filesystem>
#include <iosfwd>
#include <ostream>
#include <string_view>
#include <utility>
#include <vector>

#include "core/app/application.h"
#include "core/app/window.h"
#include "core/util/file.h"
#include "core/util/macro.h"
#include "debug/core/log.h"
#include "debug/runtime/assert.h"
#include "renderer/gl/gl_loader.h"
#include "SDL_error.h"
#include "SDL_video.h"

#define PTGN_VSYNC_MODE -1

namespace ptgn::impl {

GLVersion::GLVersion() {
	int r = SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
	PTGN_ASSERT(r == 0, SDL_GetError());
	r = SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
	PTGN_ASSERT(r == 0, SDL_GetError());
}

inline std::ostream& operator<<(std::ostream& os, const GLVersion& v) {
	os << v.major << "." << v.minor;
	return os;
}

// Must be called after SDL and window have been initialized.
void GLContext::LoadGLFunctions() {
#ifndef PTGN_PLATFORM_MACOS

#define GLE(name, caps_name) \
	name =                   \
		reinterpret_cast<PFNGL##caps_name##PROC>(SDL_GL_GetProcAddress(PTGN_STRINGIFY(gl##name)));
	GL_LIST_1
#undef GLE

#endif

#ifdef __EMSCRIPTEN__

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

#else

#ifndef PTGN_PLATFORM_MACOS

#define GLE(name, caps_name) \
	name =                   \
		reinterpret_cast<PFNGL##caps_name##PROC>(SDL_GL_GetProcAddress(PTGN_STRINGIFY(gl##name)));
	GL_LIST_2
	GL_LIST_3
#undef GLE

#endif

#endif

	// PTGN_LOG("OpenGL Build: ", GLCall(glGetString(GL_VERSION)));

#ifndef PTGN_PLATFORM_MACOS

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

GLContext::GLContext(SDL_Window* window) {
	if (context_ != nullptr) {
		int result = SDL_GL_MakeCurrent(window, context_);
		PTGN_ASSERT(result == 0, SDL_GetError());
		return;
	}

	context_ = SDL_GL_CreateContext(window);
	PTGN_ASSERT(context_ != nullptr, SDL_GetError());

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

GLContext& GLContext::operator=(GLContext&& o) {
	if (this != &o) {
		context_ = std::exchange(o.context_, nullptr);
	}
	return *this;
}

void GLContext::ClearErrors() {
	while (glGetError() != static_cast<GLenum>(GLError::None)
	) { /* glGetError clears the error queue */
	}
}

std::vector<GLError> GLContext::GetErrors() {
	std::vector<GLError> errors;
	while (true) {
		GLenum error{ glGetError() };
		auto e{ static_cast<GLError>(error) };
		if (e == GLError::None) {
			break;
		}
		errors.emplace_back(e);
	}
	return errors;
}

std::string_view GLContext::GetErrorString(GLError error) {
	switch (error) {
		case GLError::InvalidEnum:		return "Invalid Enum";
		case GLError::InvalidValue:		return "Invalid Value";
		case GLError::InvalidOperation: return "Invalid Operation";
		case GLError::StackOverflow:	return "Stack Overflow";
		case GLError::StackUnderflow:	return "Stack Underflow";
		case GLError::OutOfMemory:		return "Out of Memory";
		case GLError::None:
			PTGN_ERROR("Cannot retrieve error string for none type error");
			[[fallthrough]];
		default: PTGN_ERROR("Failed to recognize GL error code");
	}
}

void GLContext::PrintErrors(
	std::string_view function_name, const path& filepath, std::size_t line,
	const std::vector<GLError>& errors
) {
	for (auto error : errors) {
		ptgn::Print(
			"OpenGL Error: ", filepath.filename().string(), ":", line, ": ", function_name, ": ",
			GetErrorString(error)
		);
	}
}

} // namespace ptgn::impl
