#pragma once

#include <utility>
#include <vector>

#include "core/utils/type_info.h"
#include "serialization/json/json.h"

namespace ptgn {

class JSONArchiver {
public:
	template <typename T>
	void SetComponent(const T& component) {
		if constexpr (JsonSerializable<T>) {
			constexpr auto class_name{ type_name_without_namespaces<T>() };
			json json_component = component;
			if (!json_component.empty()) {
				j[class_name] = component;
			} else {
				// Serialize components which are empty json objects for component tracking.
				// This may or may not be useful to have.
				j[class_name] = json{};
			}
		}
	}

	template <typename T>
	[[nodiscard]] bool HasComponent() const {
		if constexpr (JsonDeserializable<T>) {
			constexpr auto class_name{ type_name_without_namespaces<T>() };
			if (!j.contains(class_name)) {
				return false;
			}
			return true;
		} else {
			return false;
		}
	}

	template <typename T>
	[[nodiscard]] T GetComponent() const {
		static_assert(
			std::is_default_constructible_v<T>,
			"Components retrieved from json must be default constructible"
		);
		if constexpr (JsonDeserializable<T>) {
			constexpr auto class_name{ type_name_without_namespaces<T>() };
			if (!j.contains(class_name)) {
				return {};
			}
			T component{};
			json json_component = j.at(class_name);
			if (!json_component.empty()) {
				json_component.get_to(component);
			}
			return component;
		} else {
			return {};
		}
	}

	template <typename T>
	void SetComponents(const std::vector<T>& components) {
		if constexpr (JsonSerializable<T>) {
			constexpr auto class_name{ type_name_without_namespaces<T>() };
			j[class_name]["components"] = components;
		}
	}

	template <typename T>
	void SetArrays(
		const std::vector<ecs::impl::Index>& dense_set,
		const std::vector<ecs::impl::Index>& sparse_set
	) {
		if constexpr (JsonSerializable<T>) {
			constexpr auto class_name{ type_name_without_namespaces<T>() };
			j[class_name]["dense_set"]	= dense_set;
			j[class_name]["sparse_set"] = sparse_set;
		}
	}

	template <typename T>
	[[nodiscard]] std::vector<T> GetComponents() const {
		static_assert(
			std::is_default_constructible_v<T>,
			"Components retrieved from json must be default constructible"
		);
		if constexpr (JsonDeserializable<T>) {
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
		if constexpr (JsonDeserializable<T>) {
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