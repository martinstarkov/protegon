#pragma once

#include <ranges>
#include <unordered_map>
#include <vector>

#include "common/assert.h"
#include "common/move_direction.h"
#include "core/entity.h"
#include "core/time.h"
#include "core/timer.h"
#include "input/key.h"
#include "input/mouse.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "serialization/fwd.h"
#include "serialization/json.h"
#include "serialization/serializable.h"

// TODO: Separate scripts into multiple and use event system to dispatch, instead of virtual
// functions.

namespace ptgn {

class Scene;
class Scripts;

enum class ScriptType {
	Draw,
	Window,
	Key,
	GlobalMouse,
	Mouse,
	Drag,
	Dropzone,
	Animation,
	PlayerMove,
	Collision,
	Button,
	Tween
};

namespace impl {

class IScript {
public:
	Entity entity;

	virtual ~IScript() = default;

	virtual void OnUpdate() { /* user implementation */ }

	// TODO: Consider implementing?
	// virtual void OnFixedUpdate([[maybe_unused]] float fixed_dt) {} // Called at fixed intervals
	// (physics).

	// Serialization (do not override these, as this is handled automatically by the
	// ScriptRegistry).
	virtual json Serialize() const			= 0;
	virtual void Deserialize(const json& j) = 0;

	virtual bool HasScriptType(ScriptType type) const = 0;
};

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

template <ScriptType type>
struct BaseScript {
public:
	constexpr static ScriptType GetScriptType() {
		return type;
	}
};

template <typename T, template <typename...> typename Template>
concept DerivedFromTemplate = requires(T* ptr) {
	// Works if T is derived from Template<Args...> for some Args...
	[]<typename... Args>(Template<Args...>*) {
	}(ptr);
};

} // namespace impl

template <typename TDerived, typename... TScripts>
class Script;

struct DrawScript : public impl::BaseScript<ScriptType::Draw> {
	virtual ~DrawScript() = default;

	// Called when entity is shown.
	virtual void OnShow() { /* user implementation */ }

	// Called when entity is hidden.
	virtual void OnHide() { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct WindowScript : public impl::BaseScript<ScriptType::Window> {
	virtual ~WindowScript() = default;

	virtual void OnWindowResized() { /* user implementation */ }

	virtual void OnWindowMoved() { /* user implementation */ }

	virtual void OnWindowMaximized() { /* user implementation */ }

	virtual void OnWindowMinimized() { /* user implementation */ }

	virtual void OnWindowFocusLost() { /* user implementation */ }

	virtual void OnWindowFocusGained() { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct KeyScript : public impl::BaseScript<ScriptType::Key> {
	virtual ~KeyScript() = default;

	virtual void OnKeyDown([[maybe_unused]] Key key) { /* user implementation */ }

	virtual void OnKeyPressed([[maybe_unused]] Key key) { /* user implementation */ }

	virtual void OnKeyUp([[maybe_unused]] Key key) { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct GlobalMouseScript : public impl::BaseScript<ScriptType::GlobalMouse> {
	float mouse_index{ 0.0f };

	virtual ~GlobalMouseScript() = default;

	virtual void OnMouseMove() { /* user implementation */ }

	virtual void OnMouseDown([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMousePressed([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseUp([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseScroll([[maybe_unused]] V2_int scroll_amount) { /* user implementation */ }

private:
	PTGN_SERIALIZER_REGISTER(GlobalMouseScript, mouse_index);

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct MouseScript : public impl::BaseScript<ScriptType::Mouse> {
	virtual ~MouseScript() = default;

	virtual void OnMouseEnter() { /* user implementation */ }

	virtual void OnMouseLeave() { /* user implementation */ }

	virtual void OnMouseMoveOut() { /* user implementation */ }

	virtual void OnMouseMoveOver() { /* user implementation */ }

	virtual void OnMouseDownOver([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseDownOut([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMousePressedOver([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMousePressedOut([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseUpOver([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseUpOut([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseScrollOver(
		[[maybe_unused]] V2_int scroll_amount
	) { /* user implementation */ }

	virtual void OnMouseScrollOut([[maybe_unused]] V2_int scroll_amount) { /* user implementation */
	}

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct DragScript : public impl::BaseScript<ScriptType::Drag> {
	virtual ~DragScript() = default;

	// Triggered when the user start holding left click over a draggable interactive object.
	virtual void OnDragStart([[maybe_unused]] V2_int start_position) { /* user implementation */ }

	// Triggered when the user lets go of left click while dragging a draggable interactive object.
	virtual void OnDragStop([[maybe_unused]] V2_int stop_position) { /* user implementation */ }

	// Triggered every frame while the user is holding left click over a draggable interactive
	// object.
	virtual void OnDrag() { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct DropzoneScript : public impl::BaseScript<ScriptType::Dropzone> {
	virtual ~DropzoneScript() = default;

	virtual void OnDragEnter([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	virtual void OnDragLeave([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	virtual void OnDragOver([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	virtual void OnDragOut([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	// Triggered when the user lets go (by releasing left click) of a draggable interactive object
	// while over top of a dropzone interactive object.
	virtual void OnDrop([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	// Triggered when the user picks up (by pressing left click) a draggable interactive object
	// while over top of a dropzone interactive object.
	virtual void OnPickup([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct AnimationScript : public impl::BaseScript<ScriptType::Animation> {
	virtual ~AnimationScript() = default;

	// TODO: Add animation info or reference to it here.

	virtual void OnAnimationStart() { /* user implementation */ }

	virtual void OnAnimationUpdate() { /* user implementation */ }

	// Called for each repeat of the full animation.
	virtual void OnAnimationRepeat() { /* user implementation */ }

	// Called when the frame of the animation changes
	virtual void OnAnimationFrameChange() { /* user implementation */ }

	// Called once when the animation goes through its first full cycle.
	virtual void OnAnimationComplete() { /* user implementation */ }

	virtual void OnAnimationPause() { /* user implementation */ }

	virtual void OnAnimationResume() { /* user implementation */ }

	virtual void OnAnimationStop() { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct PlayerMoveScript : public impl::BaseScript<ScriptType::PlayerMove> {
	virtual ~PlayerMoveScript() = default;

	// Called on the first frame of player movement.
	virtual void OnMoveStart() { /* user implementation */ }

	// Called every frame that the player is moving.
	virtual void OnMove() { /* user implementation */ }

	// Called on the first frame of player stopping their movement.
	virtual void OnMoveStop() { /* user implementation */ }

	// Called when the movement direction changes. Passed parameter is the difference in direction.
	// If not moving, this is simply the new direction. If moving already, this is the newly added
	// component of movement. To get the current direction instead, simply use GetDirection().
	virtual void OnDirectionChange(
		[[maybe_unused]] MoveDirection direction_difference
	) { /* user implementation */ }

	virtual void OnMoveUpStart() { /* user implementation */ }

	virtual void OnMoveUp() { /* user implementation */ }

	virtual void OnMoveUpStop() { /* user implementation */ }

	virtual void OnMoveDownStart() { /* user implementation */ }

	virtual void OnMoveDown() { /* user implementation */ }

	virtual void OnMoveDownStop() { /* user implementation */ }

	virtual void OnMoveLeftStart() { /* user implementation */ }

	virtual void OnMoveLeft() { /* user implementation */ }

	virtual void OnMoveLeftStop() { /* user implementation */ }

	virtual void OnMoveRightStart() { /* user implementation */ }

	virtual void OnMoveRight() { /* user implementation */ }

	virtual void OnMoveRightStop() { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct CollisionScript : public impl::BaseScript<ScriptType::Collision> {
	virtual ~CollisionScript() = default;

	// Must return true for collision to be checked.
	// Defaults to true.
	virtual bool PreCollisionCheck([[maybe_unused]] Entity other) {
		return true;
	}

	virtual void OnCollisionStart([[maybe_unused]] Collision collision) { /* user implementation */
	}

	virtual void OnCollision([[maybe_unused]] Collision collision) { /* user implementation */ }

	virtual void OnCollisionStop([[maybe_unused]] Collision collision) { /* user implementation */ }

	virtual void OnRaycastHit([[maybe_unused]] Collision collision) { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct ButtonScript : public impl::BaseScript<ScriptType::Button> {
	virtual ~ButtonScript() = default;

	virtual void OnButtonHoverStart() { /* user implementation */ }

	virtual void OnButtonHoverStop() { /* user implementation */ }

	virtual void OnButtonActivate() { /* user implementation */ }

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

struct TweenScript : public impl::BaseScript<ScriptType::Tween> {
	// TODO: Add or allow access somehow.
	// Tween tween;
	float test{ 0.0f };

	virtual ~TweenScript() = default;

	virtual void OnComplete() { /* user implementation */ }

	virtual void OnRepeat() { /* user implementation */ }

	virtual void OnYoyo() { /* user implementation */ }

	virtual void OnStart() { /* user implementation */ }

	virtual void OnStop() { /* user implementation */ }

	virtual void OnProgress([[maybe_unused]] float progress) { /* user implementation */ }

	virtual void OnPause() { /* user implementation */ }

	virtual void OnResume() { /* user implementation */ }

	virtual void OnReset() { /* user implementation */ }

private:
	PTGN_SERIALIZER_REGISTER(TweenScript, test);

	template <typename TDerived, typename... TScripts>
	friend class Script;
};

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
		if constexpr (tt::has_to_json_v<TDerived>) {
			to_json(j["data"], *static_cast<const TDerived*>(this));
		} else {
			using Tuple = std::tuple<TScripts...>;
			SerializeScripts<Tuple>(static_cast<const TDerived*>(this), j);
		}

		return j;
	}

	void Deserialize(const json& j) final {
		if constexpr (tt::has_from_json_v<TDerived>) {
			constexpr auto name{ type_name<TDerived>() };
			PTGN_ASSERT(j.contains("data"), "Failed to deserialize data for type ", name);
			// PTGN_ASSERT(j.at("data").contains(name), "Failed to deserialize data for type ",
			// name);
			from_json(j.at("data"), *static_cast<TDerived*>(this));
		} else {
			using Tuple = std::tuple<TScripts...>;
			DeserializeScripts<Tuple>(static_cast<TDerived*>(this), j);
		}
	}

private:
	friend class Scripts;

	virtual bool HasScriptType(ScriptType type) const final {
		return HasScriptTypeImpl(type);
	}

	static constexpr bool HasScriptTypeImpl(ScriptType type) {
		return std::ranges::find(script_types_, type) != script_types_.end();
	}

	static constexpr std::array<ScriptType, sizeof...(TScripts)> Extract() {
		return { TScripts::GetScriptType()... };
	}

	static constexpr std::array<ScriptType, sizeof...(TScripts)> script_types_{ Extract() };

	template <typename Tuple, std::size_t I = 0>
	static void SerializeScripts(const TDerived* self, json& j) {
		if constexpr (I < std::tuple_size_v<Tuple>) {
			using Base = std::tuple_element_t<I, Tuple>;
			if constexpr (tt::has_to_json_v<Base>) {
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
			if constexpr (tt::has_from_json_v<Base>) {
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
	void InvokeActions() {
		for (auto& action : actions_) {
			std::invoke(action, *this);
		}
	}

	template <typename TInterface, typename... Args>
	void AddAction(void (TInterface::*func)(Args...), Args&&... args) {
		auto action{ MakeAction(func, std::forward<Args>(args)...) };
		actions_.emplace_back(action);
	}

	// Example usage:
	// scripts.Invoke(&KeyScript::OnKeyDown, Key::W);
	template <typename TInterface, typename... Args>
	void Invoke(void (TInterface::*func)(Args...), Args&&... args) {
		constexpr ScriptType type{ TInterface::GetScriptType() };

		for (auto& script : scripts_) {
			if (!script->HasScriptType(type)) {
				continue;
			}
			TryInvoke(script.get(), func, std::forward<Args>(args)...);
		}
	}

	// TODO: Set script entity somewhere.
	template <typename TScript, typename... TArgs>
		requires // TODO: Fix concept impl::DerivedFromTemplate<TScript, Script> &&
		std::constructible_from<TScript, TArgs...>
	TScript& AddScript(TArgs&&... args) {
		auto& script{
			scripts_.emplace_back(std::make_shared<TScript>(std::forward<TArgs>(args)...))
		};
		// Explicit for debugging purposes.
		TScript& s{ *std::dynamic_pointer_cast<TScript>(script) };
		return s;
	}

	// TODO: Implement.
	/*template <typename TScript>
	[[nodiscard]] bool HasScript() const { return false; }*/

	// TODO: Implement.
	/*template <typename T>
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
		// TODO: When removing a script, we again retrieve all the enum types and remove from all
	those maps.
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

		// TODO: Do not serialize repeated ptrs.
		std::unordered_set<Base*> serialized;

		std::cout << "Serializing" << std::endl;

		for (const auto& [type, scripts] : container) {
			for (auto& script : scripts) {
				if (serialized.count(script.get())) {
					continue;
				}
				script->Serialize(j);
				serialized.emplace(script.get());
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
				std::invoke(deserialize_script, json_script);
			}
		} else {
			PTGN_ASSERT(j.contains("type"));
			std::invoke(deserialize_script, j);
		}
	}

	friend bool operator==(const ScriptContainer& a, const ScriptContainer& b) {
		return a.scripts == b.scripts;
	}

	friend bool operator!=(const ScriptContainer& a, const ScriptContainer& b) {
		return !operator==(a, b);
	}
	*/
private:
	std::vector<std::shared_ptr<impl::IScript>> scripts_;

	std::vector<std::function<void(Scripts&)>> actions_;

	template <typename TInterface, typename... Args>
	auto MakeAction(void (TInterface::*func)(Args...), Args&&... args) {
		return
			[func, tup = std::make_tuple(std::forward<Args>(args)...)](Scripts& scripts) mutable {
				std::apply(
					[&scripts, func](auto&&... unpackedArgs) {
						scripts.Invoke(func, std::forward<decltype(unpackedArgs)>(unpackedArgs)...);
					},
					std::move(tup)
				);
			};
	}

	template <typename TInterface, typename... Args>
	static void TryInvoke(
		impl::IScript* script, void (TInterface::*func)(Args...), Args&&... args
	) {
		if (auto* handler = dynamic_cast<TInterface*>(script)) {
			(handler->*func)(std::forward<Args>(args)...);
		}
	}
};

// class Scripts : public impl::ScriptContainer<impl::IScript> {
// public:
//	static void Update(Scene& scene, float dt);
//
//	template <auto TCallback, typename... TArgs>
//	static void Invoke(Scene& scene, TArgs&&... args) {
//		auto entities{ GetEntities(scene) };
//		for (const auto& entity : entities) {
//			Invoke<TCallback>(entity, std::forward<TArgs>(args)...);
//		}
//	}
//
//	template <auto TCallback, typename... TArgs>
//	static void Invoke(Entity entity, TArgs&&... args) {
//		if (!entity.IsAlive() || !entity.Has<Scripts>()) {
//			return;
//		}
//		// Copy on purpose to prevent iterator invalidation.
//		auto scripts{ entity.Get<Scripts>().scripts };
//		for (const auto& [key, script] : scripts) {
//			PTGN_ASSERT(script != nullptr, "Cannot invoke nullptr script");
//			std::invoke(TCallback, script, std::forward<TArgs>(args)...);
//		}
//	}
//
// private:
//	[[nodiscard]] static std::vector<Entity> GetEntities(Scene& scene);
// };

// namespace impl {
//
// template <typename T, typename TScriptType>
//[[nodiscard]] const auto& GetScriptInfo(const Entity& entity) {
//	PTGN_ASSERT(entity.Has<Scripts>(), "Entity has no scripts");
//
//	[[maybe_unused]] const auto& script{ entity.Get<Scripts>() };
//
//	constexpr auto class_name{ type_name<T>() };
//
//	PTGN_ASSERT(script.HasScript<T>(), "Entity does not have the specified script: ", class_name);
//
//	PTGN_ASSERT(entity.Has<TScriptType>(), "Entity does not have script info for ", class_name);
//
//	const auto& scripts{ entity.Get<TScriptType>() };
//
//	return scripts;
// }
//
// } // namespace impl
//
///**
// * @brief Adds a script of type T to the entity.
// *
// * Constructs and attaches a script of the specified type using the provided constructor
// * arguments. If the same script type T already exists on the entity, nothing happens.
// *
// * @tparam T The script type to be added.
// * @tparam TArgs The types of arguments to pass to the script constructor.
// * @param args Arguments forwarded to the script's constructor.
// * @return A reference to the newly added script.
// */
// template <typename T, typename... TArgs>
// T& AddScript(Entity& entity, TArgs&&... args) {
//	auto& scripts{ entity.TryAdd<Scripts>() };
//
//	auto& script{ scripts.AddScript<T>(std::forward<TArgs>(args)...) };
//
//	script.entity = entity;
//	script.OnCreate();
//
//	return script;
//}
//
// template <auto TCallback, typename... TArgs>
// void InvokeScript(const Entity& entity, TArgs&&... args) {
//	Scripts::Invoke<TCallback>(entity, std::forward<TArgs>(args)...);
//}
//
///**
// * @brief Adds a script that executes continuously for a specified duration.
// *
// * The script will be updated over the given time duration, then automatically stopped and
// * removed.
// *
// * @tparam T The script type to be added.
// * @tparam TArgs The types of arguments to pass to the script constructor.
// * @param execution_duration The total duration (in milliseconds) the script should be executed
// * for.
// * @param args Arguments forwarded to the script's constructor.
// * @return A reference to the newly added timed script.
// */
// template <typename T, typename... TArgs>
// T& AddTimerScript(Entity& entity, milliseconds execution_duration, TArgs&&... args) {
//	auto& script{ AddScript<T>(entity, std::forward<TArgs>(args)...) };
//
//	PTGN_ASSERT(
//		execution_duration >= milliseconds{ 0 }, "Timer script must have a positive duration"
//	);
//
//	auto& timer_scripts{ entity.TryAdd<impl::ScriptTimers>() };
//
//	constexpr auto class_name{ type_name<T>() };
//	constexpr auto hash{ Hash(class_name) };
//
//	timer_scripts.timers.emplace(hash, ScriptTimerInfo{ Timer{ true }, execution_duration });
//
//	script.OnTimerStart();
//
//	script.OnTimerUpdate(0.0f);
//
//	return script;
//}
//
//// TODO: Get rid of these in favor of a better timer system.
//
// template <typename T>
//[[nodiscard]] const ScriptTimerInfo& GetTimerScriptInfo(const Entity& entity) {
//	const auto& scripts{ impl::GetScriptInfo<T, impl::ScriptTimers>(entity) };
//
//	constexpr auto class_name{ type_name<T>() };
//	constexpr auto hash{ Hash(class_name) };
//
//	auto it{ scripts.timers.find(hash) };
//
//	PTGN_ASSERT(
//		it != scripts.timers.end(), "Entity script ", class_name, " does not have timer info"
//	);
//
//	return it->second;
//}
//
// template <typename T>
//[[nodiscard]] ScriptTimerInfo& GetTimerScriptInfo(Entity& entity) {
//	return const_cast<ScriptTimerInfo&>(GetTimerScriptInfo<T>(std::as_const(entity)));
//}
//
///**
// * @brief Adds a script that executes repeatedly with a fixed delay between executions.
// *
// * The script will be called a specified number of times with a fixed delay between each call,
// * then automatically stopped and removed. Optionally, it can be triggered immediately upon
// * adding.
// *
// * @tparam T The script type to be added.
// * @tparam TArgs The types of arguments to pass to the script constructor.
// * @param execution_delay The delay (in milliseconds) between each script execution.
// * @param execution_count The total number of times the script will be executed (-1 for
// * infinite execution, which can be stopped with RemoveScript<T>()).
// * @param execute_immediately If true, the script is executed once immediately before the first
// * delay.
// * @param args Arguments forwarded to the script's constructor.
// * @return A reference to the newly added repeat script.
// */
// template <typename T, typename... TArgs>
// T& AddRepeatScript(
//	Entity& entity, milliseconds execution_delay, int execution_count, bool execute_immediately,
//	TArgs&&... args
//) {
//	auto& script{ AddScript<T>(entity, std::forward<TArgs>(args)...) };
//
//	PTGN_ASSERT(
//		execution_delay >= milliseconds{ 0 }, "Repeat script must have a positive execution_delay"
//	);
//
//	bool infinite_execution{ execution_count == -1 };
//
//	PTGN_ASSERT(
//		infinite_execution || execution_count > 0,
//		"Repeated script execution count must be above 0 or -1 for infinite execution"
//	);
//
//	script.OnRepeatStart();
//
//	int current_executions{ 0 };
//
//	if (execute_immediately) {
//		script.OnRepeatUpdate(current_executions);
//		current_executions++;
//
//		if (!infinite_execution && current_executions >= execution_count) {
//			script.OnRepeatStop();
//			return script;
//		}
//
//		// More than one script execution requested.
//	}
//
//	auto& repeat_scripts{ entity.TryAdd<impl::ScriptRepeats>() };
//
//	constexpr auto class_name{ type_name<T>() };
//	constexpr auto hash{ Hash(class_name) };
//
//	repeat_scripts.repeats.emplace(
//		hash,
//		ScriptRepeatInfo{ Timer{ true }, execution_delay, current_executions, execution_count }
//	);
//
//	return script;
//}
//
//// TODO: Get rid of these in favor of a better repeat system.
//
// template <typename T>
//[[nodiscard]] const ScriptRepeatInfo& GetRepeatScriptInfo(const Entity& entity) {
//	const auto& scripts{ impl::GetScriptInfo<T, impl::ScriptRepeats>(entity) };
//
//	constexpr auto class_name{ type_name<T>() };
//	constexpr auto hash{ Hash(class_name) };
//
//	auto it{ scripts.repeats.find(hash) };
//
//	PTGN_ASSERT(
//		it != scripts.repeats.end(), "Entity script ", class_name, " does not have repeat info"
//	);
//
//	return it->second;
//}
//
// template <typename T>
//[[nodiscard]] ScriptRepeatInfo& GetRepeatScriptInfo(Entity& entity) {
//	return const_cast<ScriptRepeatInfo&>(GetRepeatScriptInfo<T>(std::as_const(entity)));
//}
//
///**
// * @brief Checks whether a script of the specified type is attached to the entity.
// *
// * @tparam T The script type to check.
// * @return True if the script exists, false otherwise.
// */
// template <typename T>
//[[nodiscard]] bool HasScript(const Entity& entity) {
//	return entity.Has<Scripts>() && entity.Get<Scripts>().HasScript<T>();
//}
//
///**
// * @brief Retrieves a const reference to the script of the specified type.
// *
// * @tparam T The script type to retrieve.
// * @return A const reference to the base script.
// * @throws Assert if the script is not found.
// */
// template <typename T>
//[[nodiscard]] const T& GetScript(const Entity& entity) {
//	PTGN_ASSERT(entity.Has<Scripts>());
//	auto& scripts{ entity.Get<Scripts>() };
//	return scripts.GetScript<T>();
//}
//
///**
// * @brief Retrieves a mutable reference to the script of the specified type.
// *
// * @tparam T The script type to retrieve.
// * @return A reference to the base script.
// * @throws Assert if the script is not found.
// */
// template <typename T>
//[[nodiscard]] T& GetScript(Entity& entity) {
//	return const_cast<T&>(GetScript<T>(std::as_const(entity)));
//}
//
///**
// * @brief Removes the script of the specified type from the entity.
// *
// * @tparam T The script type to remove.
// */
// template <typename T>
// void RemoveScript(Entity& entity) {
//	if (!entity.Has<Scripts>()) {
//		return;
//	}
//
//	auto& scripts{ entity.Get<Scripts>() };
//
//	if (!scripts.HasScript<T>()) {
//		return;
//	}
//
//	auto& script{ scripts.GetScript<T>() };
//
//	if (entity.Has<impl::ScriptTimers>()) {
//		// TODO: Consider replacing with a different callback / hook for early timer stop?
//		script.OnTimerStop();
//
//		auto& timers{ entity.Get<impl::ScriptTimers>().timers };
//
//		constexpr auto class_name{ type_name<T>() };
//		constexpr auto hash{ Hash(class_name) };
//
//		timers.erase(hash);
//
//		if (timers.empty()) {
//			entity.Remove<impl::ScriptTimers>();
//		}
//	}
//
//	if (entity.Has<impl::ScriptRepeats>()) {
//		// TODO: Consider replacing with a different callback / hook for early repeat stop?
//		script.OnRepeatStop();
//
//		auto& repeats{ entity.Get<impl::ScriptRepeats>().repeats };
//
//		constexpr auto class_name{ type_name<T>() };
//		constexpr auto hash{ Hash(class_name) };
//
//		repeats.erase(hash);
//
//		if (repeats.empty()) {
//			entity.Remove<impl::ScriptRepeats>();
//		}
//	}
//
//	script.OnDestroy();
//
//	scripts.RemoveScript<T>();
//
//	if (scripts.IsEmpty()) {
//		entity.Remove<Scripts>();
//	}
//}

} // namespace ptgn