#include "core/ecs/components/uuid.h"

#include <cstdint>
#include <random>

namespace ptgn {

namespace impl {

static std::random_device uuid_random_device;
static std::mt19937_64 uuid_engine(uuid_random_device());
static std::uniform_int_distribution<std::uint64_t> uuid_distribution;

} // namespace impl

UUID::UUID() : uuid_{ impl::uuid_distribution(impl::uuid_engine) } {}

UUID::UUID(std::uint64_t uuid) : uuid_{ uuid } {}

UUID::operator std::uint64_t() const {
	return uuid_;
}

} // namespace ptgn