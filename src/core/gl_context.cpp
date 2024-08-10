#include "gl_context.h"

#include <ostream>

#include "SDL.h"
#include "protegon/game.h"
#include "renderer/gl_loader.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

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
	GL_LIST
#undef GLE
	
	//PTGN_LOG("OpenGL Build: ", gl::glGetString(GL_VERSION));

// For debugging which commands were not initialized.
#define GLE(name, caps_name) \
	PTGN_ASSERT(gl::name, "Failed to load ", PTGN_STRINGIFY_MACRO(gl::name));
	GL_LIST
#undef GLE

// Check that each of the loaded gl functions was found.
#define GLE(name, caps_name) gl::name&&
	bool gl_init = GL_LIST true;
#undef GLE
	PTGN_ASSERT(gl_init, "Failed to load OpenGL functions");
	PTGN_INFO("Loaded all OpenGL functions");
}

GLContext::GLContext() {
	PTGN_ASSERT(
		game.window.Exists(), "GLContext must be constructed after SDL window construction"
	);

	context_ = SDL_GL_CreateContext(game.window.GetSDLWindow());
	PTGN_ASSERT(context_ != nullptr, SDL_GetError());

	GLVersion gl_version;

	PTGN_INFO("Initialized OpenGL version: ", gl_version);
	PTGN_INFO("Created OpenGL context");

	// From: https://nullprogram.com/blog/2023/01/08/
	// Set a non-zero SDL_GL_SetSwapInterval so that SDL_GL_SwapWindow synchronizes.
	SDL_GL_SetSwapInterval(1);

	LoadGLFunctions();
}

GLContext::~GLContext() {
	SDL_GL_DeleteContext(context_);
	PTGN_INFO("Destroyed OpenGL context");
}

} // namespace impl

} // namespace ptgn