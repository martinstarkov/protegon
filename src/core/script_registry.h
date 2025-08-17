#pragma once

#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "common/type_info.h"
#include "ecs/ecs.h"
#include "math/hash.h"
#include "serialization/json.h"

namespace ptgn {

namespace impl {

/*

template <typename TBaseScript>
class ScriptContainer {
public:
	template <typename T, typename... TArgs>
	T& AddScript(TArgs&&... args) {
		static_assert(
			std::is_base_of_v<TBaseScript, T>,
			"Cannot add script which does not inherit from the base script class"
		);
		static_assert(
			std::is_base_of_v<Script<T, TBaseScript>, T>,
			"Cannot add script which does not inherit from the base script class"
		);
		static_assert(
			std::is_constructible_v<T, TArgs...>,
			"Script must be constructible from the given arguments"
		);
		auto script{ std::make_shared<T>(std::forward<TArgs>(args)...) };
		constexpr auto class_name{ type_name<T>() };
		constexpr auto hash{ Hash(class_name) };
		auto [it, _] = scripts.try_emplace(hash, script);
		return *std::dynamic_pointer_cast<T>(it->second);
	}

	template <typename T>
	[[nodiscard]] bool HasScript() const {
		constexpr auto class_name{ type_name<T>() };
		constexpr auto hash{ Hash(class_name) };
		return scripts.find(hash) != scripts.end();
	}

	template <typename T>
	[[nodiscard]] const T& GetScript() const {
		static_assert(
			std::is_base_of_v<TBaseScript, T>,
			"Cannot get script which does not inherit from the base script class"
		);
		static_assert(
			std::is_base_of_v<Script<T, TBaseScript>, T>,
			"Cannot get script which does not inherit from the base script class"
		);
		constexpr auto class_name{ type_name<T>() };
		constexpr auto hash{ Hash(class_name) };
		auto it{ scripts.find(hash) };
		PTGN_ASSERT(
			it != scripts.end(), "Cannot get script which does not exist in ScriptContainer"
		);
		return *std::dynamic_pointer_cast<T>(it->second);
	}

	template <typename T>
	[[nodiscard]] T& GetScript() {
		return const_cast<T&>(std::as_const(*this).template GetScript<T>());
	}

	template <typename T>
	void RemoveScript() {
		constexpr auto class_name{ type_name<T>() };
		constexpr auto hash{ Hash(class_name) };
		scripts.erase(hash);
	}

	void RemoveScript(std::size_t hash) {
		scripts.erase(hash);
	}

	[[nodiscard]] bool IsEmpty() const {
		return scripts.empty();
	}

	[[nodiscard]] std::size_t Size() const {
		return scripts.size();
	}

	friend void to_json(json& j, const ScriptContainer& container) {
		if (container.scripts.empty()) {
			return;
		}
		if (container.scripts.size() == 1) {
			auto it{ container.scripts.begin() };
			j = it->second->Serialize();
		} else {
			j = json::array();
			for (const auto& [key, script] : container.scripts) {
				j.push_back(script->Serialize());
			}
		}
	}

	friend void from_json(const json& j, ScriptContainer& container) {
		container					  = {};
		const auto deserialize_script = [&container](const json& script) {
			std::string class_name{ script.at("type") };
			auto instance{ ScriptRegistry<TBaseScript>::Instance().Create(class_name) };
			if (instance) {
				instance->Deserialize(script);
				container.scripts.try_emplace(Hash(class_name), std::move(instance));
			}
		};
		if (j.is_array()) {
			for (const auto& json_script : j) {
				deserialize_script(json_script);
			}
		} else {
			PTGN_ASSERT(j.contains("type"));
			deserialize_script(j);
		}
	}

	friend bool operator==(const ScriptContainer& a, const ScriptContainer& b) {
		return a.scripts == b.scripts;
	}

	friend bool operator!=(const ScriptContainer& a, const ScriptContainer& b) {
		return !operator==(a, b);
	}

	std::unordered_map<std::size_t, std::shared_ptr<TBaseScript>> scripts;
};

*/

} // namespace impl

} // namespace ptgn