#pragma once

#include "common/type_info.h"
#include "serialization/json.h"

namespace ptgn {

class JSONArchiver {
public:
	template <typename T>
	void FromVector(const std::vector<T>& value) {
		if constexpr (tt::has_to_json_v<T>) {
			constexpr auto class_name{ type_name_without_namespaces<T>() };
			j[class_name] = value;
		}
	}

	template <typename T>
	std::vector<T> ToVector() const {
		if constexpr (tt::has_from_json_v<T>) {
			constexpr auto class_name{ type_name_without_namespaces<T>() };
			std::vector<T> vector;
			j.at(class_name).get_to(vector);
			return vector;
		} else {
			return {};
		}
	}

	json j;
};

} // namespace ptgn