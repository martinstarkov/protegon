#include "opengl_instance.h"

#include "SDL.h"
#include "SDL_image.h"

#include "protegon/debug.h"
#include "renderer/gl_loader.h"

namespace ptgn {

namespace gl {

OpenGLInstance::OpenGLInstance() {
#ifndef PTGN_PLATFORM_MACOS
	initialized_ = InitOpenGL();
	// TODO: Potentially make this initialization optional in the future.
#else
	// TODO: Figure out what to do here for Apple.
	initialized_ = false;
#endif
	PTGN_CHECK(initialized_, "Failed to initialize OpenGL");
}

OpenGLInstance::~OpenGLInstance() {
	// TODO: Figure out if something else needs to be done here.
	initialized_ = false;
}

bool OpenGLInstance::IsInitialized() const {
	return initialized_;
}

bool OpenGLInstance::InitOpenGL() {
#ifndef PTGN_PLATFORM_MACOS

#define GLE(name, caps_name) \
	name = (PFNGL##caps_name##PROC)SDL_GL_GetProcAddress(PTGN_STRINGIFY_MACRO(gl##name));
	GL_LIST
#undef GLE

	// PTGN_INFO("OpenGL Version: ", glGetString(GL_VERSION));

// For debugging which commands were not initialized.
#define GLE(name, caps_name)                  \
	if (!(name)) {                        \
		PTGN_ERROR("Failed to load ", PTGN_STRINGIFY_MACRO(name)); \
	}
	GL_LIST
#undef GLE

// Check that each of the loaded gl functions was found.
#define GLE(name, caps_name) name &&
	return GL_LIST true;
#undef GLE

#else
	// TODO: Figure out if something needs to be done separately on apple.
	return true;
#endif
}

} // namespace gl

} // namespace ptgn