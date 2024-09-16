#include "renderer/gl_loader.h"

namespace ptgn::gl {

#define GLE(name, caps_name) PFNGL##caps_name##PROC name;
GL_LIST_1
#undef GLE

#ifdef __EMSCRIPTEN__

#define GLE(name, caps_name) PFNGL##caps_name##OESPROC name;
GL_LIST_2
#undef GLE
#define GLE(name, caps_name) PFNGL##caps_name##EXTPROC name;
GL_LIST_3
#undef GLE

#else

#define GLE(name, caps_name) PFNGL##caps_name##PROC name;
GL_LIST_2
GL_LIST_3
#undef GLE

#endif

} // namespace ptgn::gl