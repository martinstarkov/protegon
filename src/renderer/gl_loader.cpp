#include "gl_loader.h"

namespace ptgn {

namespace gl {

#define GLE(name, caps_name) PFNGL##caps_name##PROC name;
GL_LIST
#undef GLE

} // namespace gl

} // namespace ptgn