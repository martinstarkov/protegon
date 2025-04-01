#pragma once

#include "core/transform.h"

namespace ptgn::impl {

// Various transform offsets which do not permanently change the transform of an entity, i.e. camera
// shake, bounce.
struct Offsets {
	Offsets() = default;

	[[nodiscard]] Transform GetTotal() const;

	Transform shake;
	Transform bounce;
};

} // namespace ptgn::impl