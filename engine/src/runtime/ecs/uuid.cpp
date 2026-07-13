#include "runtime/ecs/uuid.h"

#include <cstdint>
#include <nlohmann/json.hpp>

#include "core/math/rng.h"
#include "serialization/json/fwd.h"

namespace ptgn {

UUID::UUID() : uuid_{ RandomPositiveNumber<int>() } {}

UUID::UUID(int uuid) : uuid_{ uuid } {}

UUID::operator int() const {
	return uuid_;
}

void to_json(json& j, const UUID& uuid) {
	j = static_cast<int>(uuid);
}

void from_json(const json& j, UUID& uuid) {
	uuid = UUID{ j.get<int>() };
}

} // namespace ptgn