#pragma once

#include <concepts>
#include <string>
#include <string_view>

#include "core/util/hash.h"
#include "serialization/serialize.h"

namespace ptgn {

class KeyHash {
public:
	KeyHash() = default;

	KeyHash(std::string_view KeyHash) : value_{ Hash(KeyHash) } {}	 // NOSONAR

	KeyHash(const char* KeyHash) : value_{ Hash(KeyHash) } {}		 // NOSONAR

	KeyHash(const std::string& KeyHash) : value_{ Hash(KeyHash) } {} // NOSONAR

	KeyHash(std::size_t hash) : value_{ hash } {}					 // NOSONAR

	operator std::size_t() const {									 // NOSONAR
		return value_;
	}

	PTGN_SERIALIZE_VALUE(KeyHash, value_)
private:
	std::size_t value_{ 0 };
};

struct KeyHasher {
	template <std::derived_from<KeyHash> T>
	std::size_t operator()(const T& key) const noexcept {
		return static_cast<std::size_t>(key);
	}
};

} // namespace ptgn

template <>
struct std::hash<ptgn::KeyHash> {
	std::size_t operator()(const ptgn::KeyHash& hash) const {
		return hash;
	}
};