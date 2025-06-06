#pragma once

#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "common/type_info.h"
#include "math/hash.h"
#include "serialization/json.h"

namespace ptgn {

namespace impl {

// Script Registry to hold and create scripts
template <typename Base>
class ScriptRegistry {
public:
	static ScriptRegistry& Instance() {
		static ScriptRegistry instance;
		return instance;
	}

	void Register(std::string_view type_name, std::function<std::shared_ptr<Base>()> factory) {
		registry_[Hash(type_name)] = std::move(factory);
	}

	std::shared_ptr<Base> Create(std::string_view type_name) {
		auto it = registry_.find(Hash(type_name));
		return it != registry_.end() ? it->second() : nullptr;
	}

private:
	std::unordered_map<std::size_t, std::function<std::shared_ptr<Base>()>> registry_;
};

template <typename Derived, typename Base>
class Script : public Base {
public:
	// Constructor ensures that the static variable 'is_registered_' is initialized
	Script() {
		// Ensuring static variable is initialized to trigger registration
		(void)is_registered_;
	}

	json Serialize() const final {
		json j;
		j["type"] = type_name<Derived>();
		if constexpr (tt::has_to_json_v<Derived>) {
			j["data"] = *static_cast<const Derived*>(this);
		} // else script does not have a defined serialization.
		return j;
	}

	void Deserialize(const json& j) final {
		if constexpr (tt::has_from_json_v<Derived>) {
			PTGN_ASSERT(
				j.contains("data"), "Failed to deserialize data for type ", type_name<Derived>()
			);
			*static_cast<Derived*>(this) = j.at("data").get<Derived>();
		} // else script does not have a defined deserialization.
	}

private:
	// Static variable for ensuring class is registered once and for all
	static bool is_registered_;

	// The static Register function handles the actual registration of the class
	static bool Register() {
		ScriptRegistry<Base>::Instance().Register(
			type_name<Derived>(),
			[]() -> std::shared_ptr<Base> { return std::make_shared<Derived>(); }
		);
		return true;
	}
};

// Initialize static variable, which will trigger the Register function
template <typename Derived, typename Base>
bool Script<Derived, Base>::is_registered_ = Script<Derived, Base>::Register();

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
		scripts.emplace(hash, script);
		return *script;
	}

	template <typename T>
	[[nodiscard]] bool HasScript() const {
		constexpr auto class_name{ type_name<T>() };
		constexpr auto hash{ Hash(class_name) };
		return scripts.find(hash) != scripts.end();
	}

	template <typename T>
	[[nodiscard]] const T& GetScript() const {
		constexpr auto class_name{ type_name<T>() };
		constexpr auto hash{ Hash(class_name) };
		auto it{ scripts.find(hash) };
		PTGN_ASSERT(
			it != scripts.end(), "Cannot get script which does not exist in ScriptContainer"
		);
		return *it->second;
	}

	template <typename T>
	[[nodiscard]] T& GetScript() {
		return const_cast<T&>(std::as_const(*this).GetScript<T>());
	}

	template <typename T>
	void RemoveScript() {
		constexpr auto class_name{ type_name<T>() };
		constexpr auto hash{ Hash(class_name) };
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
				container.scripts.emplace(Hash(class_name), std::move(instance));
			}
		};
		if (j.is_array()) {
			for (const auto& json_script : j) {
				std::invoke(deserialize_script, json_script);
			}
		} else {
			PTGN_ASSERT(j.contains("type"));
			std::invoke(deserialize_script, j);
		}
	}

	std::unordered_map<std::size_t, std::shared_ptr<TBaseScript>> scripts;
};

} // namespace impl

} // namespace ptgn