#include "math/rng.h"

#include <functional>

namespace ptgn {

bool FlipCoin() {
	static RNG<int> rng{ 0, 1 };
	return rng();
}

} // namespace ptgn