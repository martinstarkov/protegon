#pragma once

#include "common/type_info.h"
#include "serialization/json.h"

namespace ptgn {

class JSONArchiver {
public:
	template <typename T>
	void SetComponents(const std::vector<T>& components) {
		if constexpr (tt::has_to_json_v<T>) {
			constexpr auto class_name{ type_name_without_namespaces<T>() };
			j[class_name]["components"] = components;
		}
	}

	template <typename T>
	void SetArrays(
		const std::vector<ecs::impl::Index>& dense_set,
		const std::vector<ecs::impl::Index>& sparse_set
	) {
		if constexpr (tt::has_to_json_v<T>) {
			constexpr auto class_name{ type_name_without_namespaces<T>() };
			j[class_name]["dense_set"]	= dense_set;
			j[class_name]["sparse_set"] = sparse_set;
		}
	}

	template <typename T>
	[[nodiscard]] std::vector<T> GetComponents() const {
		if constexpr (tt::has_from_json_v<T>) {
			constexpr auto class_name{ type_name_without_namespaces<T>() };
			if (!j.contains(class_name)) {
				return {};
			}
			std::vector<T> vector;
			j.at(class_name).at("components").get_to(vector);
			return vector;
		} else {
			return {};
		}
	}

	// @return dense_set, sparse_set
	template <typename T>
	[[nodiscard]] std::pair<std::vector<ecs::impl::Index>, std::vector<ecs::impl::Index>> GetArrays(
	) const {
		if constexpr (tt::has_from_json_v<T>) {
			constexpr auto class_name{ type_name_without_namespaces<T>() };
			if (!j.contains(class_name)) {
				return {};
			}
			std::vector<ecs::impl::Index> dense_set;
			std::vector<ecs::impl::Index> sparse_set;
			j.at(class_name).at("dense_set").get_to(dense_set);
			j.at(class_name).at("sparse_set").get_to(sparse_set);
			return { dense_set, sparse_set };
		} else {
			return {};
		}
	}

	json j;
};

} // namespace ptgn