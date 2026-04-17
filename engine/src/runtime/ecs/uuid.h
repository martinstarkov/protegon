#pragma once

#include <cstdint>

#include "serialization/json/fwd.h"

namespace ptgn::impl {

class UUID {
public:
	UUID();
	explicit UUID(std::uint64_t uuid);

	operator std::uint64_t() const; // NOSONAR

	friend void to_json(json& j, const UUID& uuid);
	friend void from_json(const json& j, UUID& uuid);

private:
	std::uint64_t uuid_{ 0 };
};

} // namespace ptgn::impl

namespace std {

template <>
struct hash<ptgn::impl::UUID> {
	std::size_t operator()(const ptgn::impl::UUID& uuid) const {
		return static_cast<std::uint64_t>(uuid);
	}
};

} // namespace std