#pragma once

#include "debug/config.h"

#define PTGN_DEBUGBREAK() ((void)0)

#ifdef PTGN_DEBUG

#if defined(PTGN_PLATFORM_WINDOWS)

#undef PTGN_DEBUGBREAK
#define PTGN_DEBUGBREAK() __debugbreak()

#elif defined(PTGN_PLATFORM_LINUX)

#include <signal.h>
#undef PTGN_DEBUGBREAK
#define PTGN_DEBUGBREAK() raise(SIGTRAP)

#endif

#endif