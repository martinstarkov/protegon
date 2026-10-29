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

	KeyHash(std::string_view key_hash) : value_{ Hash(key_hash) } {}	 // NOSONAR

	KeyHash(const char* key_hash) : value_{ Hash(key_hash) } {}		 // NOSONAR

	KeyHash(const std::string& key_hash) : value_{ Hash(key_hash) } {} // NOSONAR

	KeyHash(std::size_t key_hash) : value_{ key_hash } {}					 // NOSONAR

	operator std::size_t() const {									 // NOSONAR
		return value_;
	}

	PTGN_REFLECT_VALUE(KeyHash, value_)
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
	std::size_t operator()(const ptgn::KeyHash& key_hash) const {
		return key_hash;
	}
};