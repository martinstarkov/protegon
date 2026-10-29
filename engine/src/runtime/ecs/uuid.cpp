#include "runtime/ecs/uuid.h"

#include <cstdint>

#include "core/math/rng.h"

namespace ptgn {

UUID::UUID() : uuid_{ RandomPositiveNumber<int>() } {}

UUID::UUID(int uuid) : uuid_{ uuid } {}

UUID::operator int() const {
	return uuid_;
}

} // namespace ptgn