#pragma once

#include "core/event/event.h"
#include "core/input/key.h"

namespace ptgn::event {

/// @brief Fired once during the frame when a key is first pressed down.
struct KeyPressed : public Event<KeyPressed> {
	KeyPressed() = default;

	explicit KeyPressed(Key key) : key{ key } {}

	Key key;

	operator Key() const { // NOSONAR
		return key;
	}
};

/// @brief Fired every frame while a key is held down including the initial press.
struct KeyHeld : public Event<KeyHeld> {
	KeyHeld() = default;

	explicit KeyHeld(Key key) : key{ key } {}

	Key key;

	operator Key() const { // NOSONAR
		return key;
	}
};

/// @brief Fired once during the frame when a key is released after being pressed down.
struct KeyReleased : public Event<KeyReleased> {
	KeyReleased() = default;

	explicit KeyReleased(Key key) : key{ key } {}

	Key key;

	operator Key() const { // NOSONAR
		return key;
	}
};

} // namespace ptgn::event