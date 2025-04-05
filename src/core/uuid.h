#pragma once

#include <cstdint>

#include "serialization/serializable.h"

namespace ptgn {

class UUID {
public:
	UUID();
	explicit UUID(std::uint64_t uuid);
	UUID(const UUID&) = default;

	operator std::uint64_t() const;

	PTGN_SERIALIZER_REGISTER_NAMELESS(UUID, uuid_)

private:
	std::uint64_t uuid_{ 0 };
};

} // namespace ptgn

namespace std {

template <typename T>
struct hash;

template <>
struct hash<ptgn::UUID> {
	size_t operator()(const ptgn::UUID& uuid) const {
		return static_cast<uint64_t>(uuid);
	}
};

} // namespace std