#pragma once

#include "core/input/key.h"

namespace ptgn::event {

/// @brief Fired once during the frame when a key is first pressed down.
struct KeyPressed {
	Key key;

	operator Key() const { // NOSONAR
		return key;
	}
};

/// @brief Fired every frame while a key is held down including the initial press.
struct KeyHeld {
	Key key;

	operator Key() const { // NOSONAR
		return key;
	}
};

/// @brief Fired once during the frame when a key is released after being pressed down.
struct KeyReleased {
	Key key;

	operator Key() const { // NOSONAR
		return key;
	}
};

} // namespace ptgn::event