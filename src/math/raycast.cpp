#include "math/raycast.h"

#include "utility/debug.h"

namespace ptgn {

bool Raycast::Occurred() const {
	PTGN_ASSERT(t >= 0.0f);
	return t >= 0.0f && t < 1.0f && !normal.IsZero();
}

} // namespace ptgn