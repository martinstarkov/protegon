#include "math/rng.h"

#include <functional>

namespace ptgn {

bool FlipCoin() {
	static RNG<int> rng{ 0, 1 };
	return std::invoke(rng);
}

} // namespace ptgn