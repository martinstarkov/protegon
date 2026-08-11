#pragma once

#ifdef PTGN_EDITOR

#include "editor/editor.h"

#else

#define PTGN_WITH_EDITOR(application, ...) static_cast<void>(0)

#endif
