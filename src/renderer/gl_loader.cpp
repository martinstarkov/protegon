#include "renderer/gl_loader.h"

namespace ptgn::gl {

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


} // namespace ptgn::gl
