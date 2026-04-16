#pragma once

#include <nlohmann/json.hpp>

namespace ptgn {

using json = ::nlohmann::json;

template <typename T>
concept JsonSerializable = nlohmann::detail::has_to_json<json, T>::value;

template <typename T>
concept JsonDeserializable = nlohmann::detail::has_from_json<json, T>::value;

template <typename T>
concept JsonConvertible = JsonSerializable<T> && JsonDeserializable<T>;

} // namespace ptgn
