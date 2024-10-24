#pragma once

#include "math/vector2.h"

namespace ptgn {

namespace impl {

class Game;

class Physics {
public:
	[[nodiscard]] V2_float GetGravity() const;
	void SetGravity(const V2_float& gravity);

	// @return Frame time in milliseconds
	[[nodiscard]] float dt() const;

private:
	friend class Game;

	void Init() {
		/* Add stuff here in the future? */
	}

	void Shutdown() {
		/* Add stuff here in the future? */
	}

	V2_float gravity_{ 0.0f, 30.0f };
};

} // namespace impl

} // namespace ptgn