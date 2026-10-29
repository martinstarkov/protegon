#pragma once

#include <cstdint>

#include "serialization/serialize.h"

namespace ptgn {

class UUID {
public:
	UUID();

	explicit UUID(int uuid);

	operator int() const; // NOSONAR

	PTGN_REFLECT_VALUE(UUID, uuid_)

	constexpr bool operator==(const UUID&) const = default;
private:
	int uuid_{ 0 };
};

} // namespace ptgn

template <>
struct std::hash<ptgn::UUID> {
	std::size_t operator()(const ptgn::UUID& uuid) const {
		return static_cast<std::size_t>(uuid);
	}
};