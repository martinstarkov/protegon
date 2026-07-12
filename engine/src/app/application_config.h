#pragma once

#include "platform/window.h"
#include "serialization/serialize.h"

namespace ptgn {

/// @brief Configuration data used to initialize an Application.
struct ApplicationConfig {
	WindowConfig window;
	PTGN_REFLECT(ApplicationConfig, window)
};

} // namespace ptgn