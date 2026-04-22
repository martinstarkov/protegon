#include "runtime/ecs/uuid.h"

#include <cstdint>
#include <nlohmann/json.hpp>

#include "core/math/rng.h"
#include "serialization/json/fwd.h"

namespace ptgn::impl {

UUID::UUID() : uuid_{ static_cast<std::uint64_t>(RandomPositiveNumber<int>()) } {}

UUID::UUID(std::uint64_t uuid) : uuid_{ uuid } {}

UUID::operator std::uint64_t() const {
	return uuid_;
}

void to_json(json& j, const UUID& uuid) {
	j = static_cast<std::uint64_t>(uuid);
}

void from_json(const json& j, UUID& uuid) {
	uuid = UUID{ j.get<std::uint64_t>() };
}

} // namespace ptgn::impl