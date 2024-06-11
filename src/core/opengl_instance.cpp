#include "opengl_instance.h"

#include <SDL.h>

#include "renderer/gl_loader.h"
#include "protegon/debug.h"

namespace ptgn {

ptgn::OpenGLInstance::OpenGLInstance() {
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

	#define STR(x) #x
	#define GLE(name, caps_name) gl##name = (PFNGL##caps_name##PROC) SDL_GL_GetProcAddress(STR(gl##name));
		GL_LIST
	#undef GLE
	#undef STR

	// Check that each of the loaded gl functions was found.
	#define GLE(name, caps_name) gl##name && 
		return GL_LIST true;
	#undef GLE

	#else
		// TODO: Figure out if something needs to be done separately on apple.
		return true;
	#endif
}

} // namespace ptgn