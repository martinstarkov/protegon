#pragma once

#include <cstdint>
#include <functional>

#include "serialization/json/serializable.h"

namespace ptgn {

class UUID {
public:
	UUID();
	explicit UUID(std::uint64_t uuid);

	operator std::uint64_t() const;

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(UUID, uuid_)

private:
	std::uint64_t uuid_{ 0 };
};

} // namespace ptgn

template <>
struct std::hash<ptgn::UUID> {
	std::size_t operator()(const ptgn::UUID& uuid) const {
		return static_cast<std::uint64_t>(uuid);
	}
};