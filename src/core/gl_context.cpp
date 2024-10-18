#include "core/gl_context.h"

#include <filesystem>
#include <iosfwd>
#include <ostream>
#include <string_view>
#include <vector>

#include "SDL_error.h"
#include "SDL_video.h"
#include "core/window.h"
#include "protegon/file.h"
#include "protegon/game.h"
#include "protegon/log.h"
#include "renderer/gl_loader.h"
#include "utility/debug.h"

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
#define GLE(name, caps_name) \
	gl::name = (gl::PFNGL##caps_name##PROC)SDL_GL_GetProcAddress(PTGN_STRINGIFY_MACRO(gl##name));
	GL_LIST_1
#undef GLE

#ifdef __EMSCRIPTEN__

#define GLE(name, caps_name) \
	gl::name = (gl::PFNGL##caps_name##OESPROC)SDL_GL_GetProcAddress(PTGN_STRINGIFY_MACRO(gl##name));
	GL_LIST_2
#undef GLE
#define GLE(name, caps_name) \
	gl::name = (gl::PFNGL##caps_name##EXTPROC)SDL_GL_GetProcAddress(PTGN_STRINGIFY_MACRO(gl##name));
	GL_LIST_3
#undef GLE

#else

#define GLE(name, caps_name) \
	gl::name = (gl::PFNGL##caps_name##PROC)SDL_GL_GetProcAddress(PTGN_STRINGIFY_MACRO(gl##name));
	GL_LIST_2
	GL_LIST_3
#undef GLE

#endif

	// PTGN_LOG("OpenGL Build: ", GLCall(gl::glGetString(GL_VERSION)));

// For debugging which commands were not initialized.
#define GLE(name, caps_name) \
	PTGN_ASSERT(gl::name, "Failed to load ", PTGN_STRINGIFY_MACRO(gl::name));
	GL_LIST_1
	GL_LIST_2
	GL_LIST_3
#undef GLE

// Check that each of the loaded gl functions was found.
#define GLE(name, caps_name) gl::name&&
	bool gl_init = GL_LIST_1 GL_LIST_2 GL_LIST_3 true;
#undef GLE
	PTGN_ASSERT(gl_init, "Failed to load OpenGL functions");
	PTGN_INFO("Loaded all OpenGL functions");
}

bool GLContext::IsInitialized() const {
	return context_ != nullptr;
}

void GLContext::Init() {
	PTGN_ASSERT(
		game.window.IsValid(), "GLContext must be constructed after SDL window construction"
	);

	if (IsInitialized()) {
		int result = game.window.MakeGLContextCurrent(context_);
		PTGN_ASSERT(result == 0, SDL_GetError());
		return;
	}

	context_ = game.window.CreateGLContext();
	PTGN_ASSERT(IsInitialized(), SDL_GetError());

	GLVersion gl_version;

	PTGN_INFO("Initialized OpenGL version: ", gl_version);
	PTGN_INFO("Created OpenGL context");

	// From: https://nullprogram.com/blog/2023/01/08/
	// Set a non-zero SDL_GL_SetSwapInterval so that SDL_GL_SwapWindow synchronizes.
	SDL_GL_SetSwapInterval(1);

	LoadGLFunctions();
}

void GLContext::Shutdown() {
	SDL_GL_DeleteContext(context_);
	context_ = nullptr;
	PTGN_INFO("Destroyed OpenGL context");
}

void GLContext::ClearErrors() {
	while (game.gl_context_->IsInitialized() && game.IsRunning() &&
		   gl::glGetError() != static_cast<gl::GLenum>(GLError::None)
	) { /* glGetError clears the error queue */
	}
}

std::vector<GLError> GLContext::GetErrors() {
	std::vector<GLError> errors;
	while (game.gl_context_->IsInitialized() && game.IsRunning()) {
		gl::GLenum error = gl::glGetError();
		auto e			 = static_cast<GLError>(error);
		if (e == GLError::None) {
			break;
		}
		errors.emplace_back(e);
	}
	return errors;
}

std::string_view GLContext::GetErrorString(GLError error) {
	PTGN_ASSERT(error != GLError::None, "Cannot retrieve error string for none type error");
	switch (error) {
		case GLError::InvalidEnum:		return "Invalid Enum";
		case GLError::InvalidValue:		return "Invalid Value";
		case GLError::InvalidOperation: return "Invalid Operation";
		case GLError::StackOverflow:	return "Stack Overflow";
		case GLError::StackUnderflow:	return "Stack Underflow";
		case GLError::OutOfMemory:		return "Out of Memory";
		default:						PTGN_ERROR("Failed to recognize GL error code");
	}
}

void GLContext::PrintErrors(
	std::string_view function_name, const path& filepath, std::size_t line,
	const std::vector<GLError>& errors
) {
	for (auto error : errors) {
		ptgn::debug::Print(
			"OpenGL Error: ", filepath.filename().string(), ":", line, ": ", function_name, ": ",
			GetErrorString(error)
		);
	}
}

} // namespace ptgn::impl