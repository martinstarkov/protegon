#pragma once

#include "utility/macro.h"

PTGN_HAS_TEMPLATE_FUNCTION(Serialize);
PTGN_HAS_TEMPLATE_FUNCTION(Deserialize);

namespace ptgn {

namespace impl {

template <typename T>
struct JsonKeyValuePair {
	std::string_view key;
	T& value;

	JsonKeyValuePair(std::string_view key, T& value) : key{ key }, value{ value } {}
};

template <typename T>
JsonKeyValuePair<T> KeyValue(std::string_view key, T& value) {
	return { key, value };
}

} // namespace impl

} // namespace ptgn

#define PTGN_KEY_VALUE_PAIR(x, instance) ptgn::impl::KeyValue(#x, instance.x)

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