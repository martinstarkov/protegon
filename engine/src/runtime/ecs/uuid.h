#pragma once

#include <cstdint>

#include "serialization/json/fwd.h"

namespace ptgn::impl {

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

} // namespace ptgn::impl

template <>
struct std::hash<ptgn::impl::UUID> {
	std::size_t operator()(const ptgn::impl::UUID& uuid) const {
		return static_cast<std::size_t>(uuid);
	}
};