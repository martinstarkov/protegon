#pragma once

#include <string_view>

#include "utility/macro.h"

namespace ptgn {

namespace impl {

template <typename T>
struct JsonKeyValuePair {
	std::string_view key;
	T& value;

	JsonKeyValuePair(std::string_view key, T& value) : key{ key }, value{ value } {}
};

} // namespace impl

template <typename T>
impl::JsonKeyValuePair<T> KeyValue(std::string_view key, T& value) {
	return { key, value };
}

} // namespace ptgn

#define PTGN_KEY_VALUE_PAIR(x, instance) ptgn::KeyValue(#x, instance.x)

#define PTGN_KEY_VALUE_PAIR_LIST(instance, ...) \
	PTGN_MAP_LIST_DATA(PTGN_KEY_VALUE_PAIR, instance, __VA_ARGS__)

#define PTGN_SERIALIZER_REGISTER(...)                            \
	template <typename Archive>                                  \
	void Serialize(Archive& archive) const {                     \
		archive(PTGN_KEY_VALUE_PAIR_LIST((*this), __VA_ARGS__)); \
	}                                                            \
	template <typename Archive>                                  \
	void Deserialize(Archive& archive) {                         \
		archive(PTGN_KEY_VALUE_PAIR_LIST((*this), __VA_ARGS__)); \
	}

#define PTGN_SERIALIZER_REGISTER_NAMELESS(...) \
	template <typename Archive>                \
	void Serialize(Archive& archive) const {   \
		archive(__VA_ARGS__);                  \
	}                                          \
	template <typename Archive>                \
	void Deserialize(Archive& archive) {       \
		archive(__VA_ARGS__);                  \
	}
