#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "ecs/entity.h"
#include "core/scripting/script_interfaces.h"
#include "math/hash.h"
#include "nlohmann/json.hpp"
#include "serialization/json/fwd.h"
#include "serialization/json/json.h"

namespace ptgn {

class Scene;

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

template <typename T, template <typename...> typename Template>
concept DerivedFromTemplate = requires(T* ptr) {
	// Works if T is derived from Template<TArgs...> for some TArgs...
	[]<typename... TArgs>(Template<TArgs...>*) {
	}(ptr);
};

} // namespace impl

template <typename TDerived, typename... TScripts>
class Script : public impl::IScript, public TScripts... {
public:
	// Constructor ensures that the static variable 'is_registered_' is initialized
	Script() {
		// Ensuring static variable is initialized to trigger registration
		(void)is_registered_;
	}

	json Serialize() const final {
		json j;

		constexpr auto name{ type_name<TDerived>() };

		j["type"] = name;
		to_json(j["entity"], entity);
		if constexpr (JsonSerializable<TDerived>) {
			to_json(j["data"], *static_cast<const TDerived*>(this));
		} else {
			using Tuple = std::tuple<TScripts...>;
			SerializeScripts<Tuple>(static_cast<const TDerived*>(this), j);
		}

		return j;
	}

	void Deserialize(const json& j) final {
		constexpr auto name{ type_name<TDerived>() };
		if constexpr (JsonDeserializable<TDerived>) {
			PTGN_ASSERT(j.contains("data"), "Failed to deserialize data for type ", name);
			// PTGN_ASSERT(j.at("data").contains(name), "Failed to deserialize data for type ",
			// name);
			from_json(j.at("data"), *static_cast<TDerived*>(this));
		} else {
			using Tuple = std::tuple<TScripts...>;
			DeserializeScripts<Tuple>(static_cast<TDerived*>(this), j);
		}
		PTGN_ASSERT(j.contains("entity"), "Failed to deserialize entity for type ", name);
		from_json(j.at("entity"), entity);
	}

	constexpr std::size_t GetHash() const final {
		constexpr auto name{ type_name<TDerived>() };
		return Hash(name);
	}

private:
	friend class Scripts;

	constexpr bool HasScriptType(ScriptType type) const final {
		return HasScriptTypeImpl(type);
	}

	static constexpr bool HasScriptTypeImpl(ScriptType type) {
		return std::ranges::find(script_types_, type) != script_types_.end();
	}

	static constexpr std::array<ScriptType, sizeof...(TScripts) + 1> Extract() {
		return { impl::IScript::GetScriptType(), TScripts::GetScriptType()... };
	}

	static constexpr std::array<ScriptType, sizeof...(TScripts) + 1> script_types_{ Extract() };

	template <typename Tuple, std::size_t I = 0>
	static void SerializeScripts(const TDerived* self, json& j) {
		if constexpr (I < std::tuple_size_v<Tuple>) {
			using Base = std::tuple_element_t<I, Tuple>;
			if constexpr (JsonSerializable<Base>) {
				constexpr auto base_name{ type_name<Base>() };
				const Base& base{ *static_cast<const Base*>(self) };
				to_json(j["data"][base_name], base);
				SerializeScripts<Tuple, I + 1>(self, j);
			}
		}
	}

	template <typename Tuple, std::size_t I = 0>
	static void DeserializeScripts(TDerived* self, const json& j) {
		if constexpr (I < std::tuple_size_v<Tuple>) {
			using Base = std::tuple_element_t<I, Tuple>;
			if constexpr (JsonDeserializable<Base>) {
				constexpr auto base_name{ type_name<Base>() };
				PTGN_ASSERT(j.contains("data"), "Failed to deserialize data for type ", base_name);
				PTGN_ASSERT(
					j.at("data").contains(base_name), "Failed to deserialize data for type ",
					base_name
				);
				Base& base{ *static_cast<Base*>(self) };
				from_json(j.at("data").at(base_name), base);
				DeserializeScripts<Tuple, I + 1>(self, j);
			}
		}
	}

	// TODO: Add deserialize tscripts function to iterate parameter pack.

	// Static variable for ensuring class is registered once and for all
	static bool is_registered_;

	// The static Register function handles the actual registration of the class
	static bool Register() {
		constexpr auto name{ type_name<TDerived>() };
		impl::ScriptRegistry<impl::IScript>::Instance().Register(
			name, []() -> std::shared_ptr<impl::IScript> { return std::make_shared<TDerived>(); }
		);
		return true;
	}
};

// Initialize static variable, which will trigger the Register function
template <typename TDerived, typename... TScripts>
bool Script<TDerived, TScripts...>::is_registered_ = Script<TDerived, TScripts...>::Register();

class Scripts {
public:
	// Will call ClearActions after all actions have been executed.
	void InvokeActions() {
		while (!actions_.empty()) {
			auto current_actions{ std::move(actions_) };
			ClearActions();

			for (const auto& action : current_actions) {
				action(*this);
			}
		}
	}

	void ClearActions() {
		actions_.clear();
	}

	// Only add an action if the scripts container already has a script which listens to that type
	// of action.
	template <typename TInterface, typename... TArgs>
	void AddAction(void (TInterface::*func)(TArgs...), TArgs... args) {
		constexpr ScriptType type{ TInterface::GetScriptType() };
		bool has_script{ false };
		for (const auto& script : scripts_) {
			if (script->HasScriptType(type)) {
				has_script = true;
				break;
			}
		}
		if (!has_script) {
			return;
		}
		auto action{ MakeAction(func, std::forward<TArgs>(args)...) };
		actions_.emplace_back(action);
	}

	// Example usage:
	// scripts.Invoke(&KeyScript::OnKeyDown, Key::W);
	template <typename TInterface, typename... TArgs>
	void Invoke(void (TInterface::*func)(TArgs...), TArgs&&... args) {
		constexpr ScriptType type{ TInterface::GetScriptType() };

		// In case the action function removes scripts, this prevents iterator
		// invalidation.
		auto scripts{ scripts_ };
		for (std::shared_ptr<impl::IScript> script : scripts) {
			if (!script->HasScriptType(type)) {
				continue;
			}
			TryInvoke(script.get(), func, std::forward<TArgs>(args)...);
		}
	}

	template <typename TScript, typename... TArgs>
		requires std::constructible_from<
			TScript, TArgs...> // TODO: Fix concept impl::DerivedFromTemplate<TScript, Script>
	TScript& AddScript(TArgs&&... args) {
		auto& script{
			scripts_.emplace_back(std::make_shared<TScript>(std::forward<TArgs>(args)...))
		};
		// Explicit for debugging purposes.
		TScript& s{ *std::dynamic_pointer_cast<TScript>(script) };
		return s;
	}

	template <typename TScript>
	[[nodiscard]] bool HasScript() const {
		constexpr auto name{ type_name<TScript>() };
		constexpr auto hash{ Hash(name) };
		return std::ranges::any_of(scripts_, [](const auto& script) {
			return script->GetHash() == hash;
		});
	}

	template <typename TScript>
	void RemoveScripts() {
		constexpr auto name{ type_name<TScript>() };
		constexpr auto hash{ Hash(name) };
		auto it{ std::remove_if(scripts_.begin(), scripts_.end(), [](const auto& script) {
			return script->GetHash() == hash;
		}) };
		scripts_.erase(it, scripts_.end());
	}

	friend void to_json(json& j, const Scripts& container) {
		if (container.scripts_.empty()) {
			return;
		}
		if (container.scripts_.size() == 1) {
			j = json{};
			const auto& script{ container.scripts_.front() };
			j = script->Serialize();
			// j["entity"] = script->entity;
		} else {
			j = json::array();
			for (const auto& script : container.scripts_) {
				// json object;
				//  object["type"]	 = script->Serialize();
				//  object["entity"] = script->Serialize();
				j.push_back(script->Serialize());
			}
		}
	}

	friend void from_json(const json& j, Scripts& container) {
		container = {};

		const auto deserialize_script = [&container](const json& script) {
			PTGN_ASSERT(script.contains("type"));

			std::string class_name{ script.at("type").get<std::string>() };

			auto instance{ impl::ScriptRegistry<impl::IScript>::Instance().Create(class_name) };

			if (instance) {
				PTGN_ASSERT(script.contains("data"));
				PTGN_ASSERT(script.contains("entity"));

				Entity entity;
				script.at("entity").get_to(entity);
				instance->Deserialize(script);
				PTGN_ASSERT(entity, "Failed to deserialize entity for type: ", class_name);
				instance->entity = entity;
				container.scripts_.emplace_back(instance);
			}
		};
		if (j.is_array()) {
			for (const auto& json_script : j) {
				deserialize_script(json_script);
			}
		} else {
			deserialize_script(j);
		}
	}

	friend bool operator==(const Scripts& a, const Scripts& b) {
		return a.scripts_ == b.scripts_;
	}

	template <typename TInterface, typename... TArgs>
	[[nodiscard]] bool ConditionCheck(
		bool (TInterface::*func)(TArgs...) const, TArgs&&... args
	) const {
		constexpr ScriptType type{ TInterface::GetScriptType() };

		// In case the condition check function removes scripts, this prevents iterator
		// invalidation.
		auto scripts{ scripts_ };
		for (const auto& script : scripts) {
			if (!script->HasScriptType(type)) {
				continue;
			}
			if (const auto* handler = dynamic_cast<const TInterface*>(script.get())) {
				bool result{ (handler->*func)(std::forward<TArgs>(args)...) };
				if (!result) {
					return false;
				}
			}
		}
		return true;
	}

private:
	std::vector<std::shared_ptr<impl::IScript>> scripts_;

	std::vector<std::function<void(Scripts&)>> actions_;

	template <typename TInterface, typename... TArgs>
	auto MakeAction(void (TInterface::*func)(TArgs...), TArgs&&... args) {
		return
			[func, tup = std::make_tuple(std::forward<TArgs>(args)...)](Scripts& scripts) mutable {
				std::apply(
					[&scripts, func](auto&&... unpacked) mutable {
						// If there is ever a crash here, it is most likely because the scripts
						// reference is no longer valid. In this case, just change this lambda to
						// take a copy of scripts into a variable to keep it valid throughout the
						// invokation.
						scripts.Invoke(func, std::forward<decltype(unpacked)>(unpacked)...);
					},
					std::move(tup)
				);
			};
	}

	template <typename TInterface, typename... TArgs>
	static void TryInvoke(
		impl::IScript* script, void (TInterface::*func)(TArgs...), TArgs&&... args
	) {
		if (auto* handler = dynamic_cast<TInterface*>(script)) {
			(handler->*func)(std::forward<TArgs>(args)...);
		}
	}
};

/**
 * @brief Adds a script of type T to the entity.
 *
 * Constructs and attaches a script of the specified type using the provided constructor
 * arguments. If the same script type T already exists on the entity, nothing happens.
 *
 * @tparam T The script type to be added.
 * @tparam TArgs The types of arguments to pass to the script constructor.
 * @param args Arguments forwarded to the script's constructor.
 * @return A reference to the newly added script.
 */
template <typename T, typename... TArgs>
T& AddScript(Entity& entity, TArgs&&... args) {
	auto& scripts{ entity.TryAdd<Scripts>() };

	auto& script{ scripts.AddScript<T>(std::forward<TArgs>(args)...) };

	script.entity = entity;

	script.OnCreate();

	return script;
}

// Same as AddScript but no-op if script type exists on the entity.
template <typename T, typename... TArgs>
void TryAddScript(Entity& entity, TArgs&&... args) {
	auto& scripts{ entity.TryAdd<Scripts>() };

	if (scripts.HasScript<T>()) {
		return;
	}

	auto& script{ scripts.AddScript<T>(std::forward<TArgs>(args)...) };

	script.entity = entity;

	script.OnCreate();
}

/**
 * @brief Checks whether a script of the specified type is attached to the entity.
 *
 * @tparam T The script type to check.
 * @return True if the script exists, false otherwise.
 */
template <typename T>
[[nodiscard]] bool HasScript(const Entity& entity) {
	return entity.Has<Scripts>() && entity.Get<Scripts>().HasScript<T>();
}

/**
 * @brief Removes the scripts of the specified type from the entity.
 *
 * @tparam T The script type to remove.
 */
template <typename T>
void RemoveScripts(Entity& entity) {
	if (!entity.Has<Scripts>()) {
		return;
	}

	auto& scripts{ entity.Get<Scripts>() };

	if (!scripts.HasScript<T>()) {
		return;
	}

	scripts.RemoveScripts<T>();
}

} // namespace ptgn