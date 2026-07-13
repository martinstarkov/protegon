#pragma once

#include <cstdint>

#include "serialization/json/fwd.h"

namespace ptgn {

class UUID {
public:
	UUID();

	explicit UUID(int uuid);

	operator int() const; // NOSONAR

	friend void to_json(json& j, const UUID& uuid);
	friend void from_json(const json& j, UUID& uuid);

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