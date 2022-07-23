#pragma once

#include <cstdlib> // std::size_t

#include "math/Hash.h"

namespace ptgn {

template <typename T>
class Tag {
public:
	~Tag() = default;
	Tag() : id{ GetId<T>() } {}

	bool operator==(const char* rhs) const {
		return id == math::Hash(rhs);
	}
	bool operator!=(const char* rhs) const {
		return !operator==(rhs);
	}
	bool operator==(const std::size_t rhs) const {
		return id == rhs;
	}
	bool operator!=(const std::size_t rhs) const {
		return !operator==(rhs);
	}
private:
	const std::size_t id{ 0 };
	static std::size_t& TagCount() {
		static std::size_t id{ 0 };
		return id;
	}
	template <typename T>
	static GetId() {
		static std::size_t id{ TagCount()++ };
		return id;
	}
};

} // namespace ptgn

template <typename T>
bool operator==(const char* lhs, const ptgn::Tag<T>& rhs) {
	return rhs == lhs;
}

template <typename T>
bool operator!=(const char* lhs, const ptgn::Tag<T>& rhs) {
	return !(rhs == lhs);
}

template <typename T>
bool operator==(const std::size_t lhs, const ptgn::Tag<T>& rhs) {
	return rhs == lhs;
}

template <typename T>
bool operator!=(const std::size_t lhs, const ptgn::Tag<T>& rhs) {
	return !(rhs == lhs);
}