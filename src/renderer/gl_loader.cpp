#include "gl_loader.h"

#define GLE(name, caps_name) PFNGL##caps_name##PROC gl##name;
GL_LIST
PFNGLACTIVETEXTUREARBPROC pglActiveTexture;
#undef GLE