/*#include "components/draw.h"
#include "core/game.h"
#include "protegon/protegon.h"
#include "rendering/resources/texture.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class Sandbox : public Scene {
public:
	Sprite s1;

	void Enter() override {
		game.texture.Load("test", "resources/test.png");

		s1 = CreateSprite(manager, "test");

		s1.SetPosition(camera.primary.GetPosition());
	}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Sandbox");
	game.scene.Enter<Sandbox>("sandbox");
}
*/

#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "components/transform.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/script.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "serialization/component_registry.h"
#include "serialization/json.h"

using namespace ptgn;

// Base ScriptComponent class
class TweenScript {
public:
	virtual ~TweenScript() = default;

	virtual void OnCreate(Entity& entity)			= 0;
	virtual void OnUpdate(Entity& entity, float dt) = 0;

	virtual json Serialize() const			= 0;
	virtual void Deserialize(const json& j) = 0;
};

class TweenMove : public Script<TweenMove, TweenScript> {
public:
	TweenMove() {}

	float targetX  = 0.0f;
	float targetY  = 0.0f;
	float duration = 1.0f;
	float elapsed  = 0.0f;
	V2_float startPos;

	void OnCreate(Entity& entity) override {
		startPos = entity.GetPosition();
	}

	void OnUpdate(Entity& entity, float dt) override {
		elapsed			+= dt;
		float t			 = std::min(elapsed / duration, 1.0f);
		V2_float newPos	 = { startPos.x + (targetX - startPos.x) * t,
							 startPos.y + (targetY - startPos.y) * t };
		entity.SetPosition(newPos);
	}

	PTGN_SERIALIZER_REGISTER(TweenMove, targetX, targetY, duration)
};

class ScriptComponentContainer {
	std::vector<std::unique_ptr<TweenScript>> scripts;

public:
	void AddScript(std::string_view type_name, const json& config, Entity& owner) {
		auto script = ScriptRegistry<TweenScript>::Instance().Create(type_name);
		if (script) {
			script->Deserialize(config);
			script->OnCreate(owner);
			scripts.push_back(std::move(script));
		}
	}

	void UpdateAll(Entity& owner, float dt) {
		for (const auto& script : scripts) {
			script->OnUpdate(owner, dt);
		}
	}

	const auto& GetScripts() const {
		return scripts;
	}
};

json SerializeScripts(const ScriptComponentContainer& container) {
	json arr = json::array();
	for (const auto& script : container.GetScripts()) {
		arr.push_back(script->Serialize());
	}
	return arr;
}

void DeserializeScripts(ScriptComponentContainer& container, const json& arr, Entity& owner) {
	for (const auto& scriptJson : arr) {
		std::string type = scriptJson.at("type");
		container.AddScript(type, scriptJson, owner);
	}
}

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Manager manager;
	Entity entity{ manager.CreateEntity() };
	/*
	entity.Add<Transform>(V2_float{ 30, 50 });
	entity.Add<BoxCollider>(V2_float{ 100, 120 });
	manager.Refresh();

	json j = manager;

	std::string s = j.dump(4);

	PTGN_LOG("Manager: ", s);

	Manager manager2;

	j.get_to(manager2);
	*/

	ScriptComponentContainer scriptContainer;

	// Simulate loading from JSON
	json scriptJson = {
		{ { "type", "TweenMove" }, { "targetX", 20.0 }, { "targetY", 25.0 }, { "duration", 3.0 } }
	};

	DeserializeScripts(scriptContainer, scriptJson, entity);

	float dt = 0.1f; // Simulated delta time

	// Simulate update loop
	for (int i = 0; i <= 30; ++i) {
		V2_float pos = entity.GetPosition();
		PTGN_LOG("Time: ", static_cast<float>(i) * dt, "s - Position: ", pos);
		scriptContainer.UpdateAll(entity, dt);
	}

	return 0;
}

/*

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include "math/hash.h"
#include "serialization/fwd.h"
#include "serialization/json.h"
#include "serialization/serializable.h"
#include "utility/type_info.h"
#include "utility/type_traits.h"

using namespace ptgn;

template <class Base, class... Args>
class Factory {
public:
	using FactoryBase = typename Base;

	virtual ~Factory() = default;

	virtual std::string_view GetName() const {
		return name_;
	}

	template <typename... Ts>
	static std::unique_ptr<Base> create(std::string_view class_name, Ts&&... args) {
		auto it = data().find(Hash(class_name));
		if (it == data().end()) {
			std::cout << "Failed to find hash for " << class_name << std::endl;
		}
		auto ptr{ it->second(std::forward<Ts>(args)...) };
		ptr->name_ = class_name;
		return ptr;
	}

	template <typename T, typename... Ts>
	static std::unique_ptr<Base> create(Ts&&... args) {
		constexpr std::string_view class_name{ type_name<T>() };
		auto it = Factory::data().find(Hash(class_name));
		if (it == Factory::data().end()) {
			std::cout << "Failed to find constructor hash for " << class_name << std::endl;
		}
		auto ptr{ it->second(std::forward<Ts>(args)...) };
		ptr->name_ = class_name;
		return ptr;
	}

	static std::unique_ptr<Base> create(const json& j) {
		std::string_view class_name{ j["name"] };
		auto it = Factory::dataJ().find(Hash(class_name));
		if (it == Factory::dataJ().end()) {
			std::cout << "Failed to find json constructor hash for " << class_name << std::endl;
		}
		auto ptr{ it->second(j) };
		ptr->name_ = class_name;
		return ptr;
	}

	virtual void to_json_impl(json& j) const {
		PTGN_ERROR("Fail");
	}

	virtual void from_json_impl(const json& j) {
		PTGN_ERROR("Fail");
	}

	template <typename T>
	struct Registrar : public Base {
		friend T;

		void to_json_impl(json& j) const final {
			if constexpr (nlohmann::detail::has_to_json<json, T>::value) {
				j		  = *static_cast<const T*>(this);
				j["name"] = GetName();
			} else {
				// TODO: Update error msg.
				PTGN_ERROR("Fail");
			}
		}

		void from_json_impl(const json& j) final {
			if constexpr (nlohmann::detail::has_from_json<json, T>::value) {
				j.get_to(*static_cast<T*>(this));
			} else {
				// TODO: Update error msg.
				PTGN_ERROR("Fail");
			}
		}

		virtual std::string_view GetName() const {
			return type_name<T>();
		}

		static bool registerT() {
			constexpr std::string_view class_name{ type_name<T>() };
			std::cout << "Registering constructor hash for " << class_name << std::endl;
			Factory::data()[Hash(class_name)] = [](Args... args) -> std::unique_ptr<Base> {
				return std::make_unique<T>(std::forward<Args>(args)...);
			};
			return true;
		}

		// TODO: Combine into register T.
		static bool registerTJ() {
			constexpr std::string_view class_name{ type_name<T>() };
			std::cout << "Registering json constructor hash for " << class_name << std::endl;
			Factory::dataJ()[Hash(class_name)] = [](const json& j) -> std::unique_ptr<Base> {
				if constexpr (nlohmann::detail::has_from_json<json, T>::value) {
					auto ptr{ std::make_unique<T>() };
					j.get_to(*ptr);
					return ptr;
				} else {
					// TODO: Update error msg.
					PTGN_ERROR("Fail");
				}
			};
			return true;
		}

		static bool registered;
		static bool registeredJ;

		// private:
		Registrar() : Base(Key{}) {
			(void)registered;
			(void)registeredJ;
		}
	};

	friend Base;

	static auto& data() {
		static std::unordered_map<std::size_t, FactoryFuncType> s;
		return s;
	}

	static auto& dataJ() {
		static std::unordered_map<std::size_t, FactoryFuncTypeJ> s;
		return s;
	}

private:
	class Key {
		Key(){};
		template <class T>
		friend struct Registrar;
	};

	using FactoryFuncType  = std::unique_ptr<Base> (*)(Args...);
	using FactoryFuncTypeJ = std::unique_ptr<Base> (*)(const json&);
	Factory()			   = default;

	std::string_view name_;
};

template <class Base, class... Args>
template <class T>
bool Factory<Base, Args...>::template Registrar<T>::registered =
	Factory<Base, Args...>::template Registrar<T>::registerT();

template <class Base, class... Args>
template <class T>
bool Factory<Base, Args...>::template Registrar<T>::registeredJ =
	Factory<Base, Args...>::template Registrar<T>::registerTJ();

// TODO: Move to type_traits namespace.
template <typename, typename = std::void_t<>>
struct has_factory_base : std::false_type {};

template <typename T>
struct has_factory_base<T, std::void_t<typename T::FactoryBase>> : std::true_type {};

template <typename T>
constexpr bool has_factory_base_v = has_factory_base<T>::value;

template <typename T, typename = void>
struct is_self_factory : std::false_type {};

template <typename T>
struct is_self_factory<T, std::enable_if_t<std::is_same_v<typename T::FactoryBase, T>>> :
	std::true_type {};

template <typename T>
constexpr bool is_self_factory_v = has_factory_base<T>::value && is_self_factory<T>::value;

// TODO: Hide somehow?
template <typename T>
std::enable_if_t<is_self_factory_v<T>> to_json(json& nlohmann_json_j, const T& nlohmann_json_t)
{ nlohmann_json_t.to_json_impl(nlohmann_json_j);
}

template <typename T>
std::enable_if_t<is_self_factory_v<T>> from_json(const json& nlohmann_json_j, T&
nlohmann_json_t) { nlohmann_json_t.from_json_impl(nlohmann_json_j);
}

template <typename ScriptInterface, typename ScriptType>
using Script = typename ScriptInterface::template Registrar<ScriptType>;

// TODO: Consider making this a template class and moving OnUpdate to a base class so pointers
can
// be stored.
struct TweenScript : public Factory<TweenScript, int> {
	TweenScript(Key) {}

	virtual ~TweenScript() override = default;

	virtual void OnUpdate(float f) {}
};

class TweenScript1 : public Script<TweenScript, TweenScript1> {
public:
	TweenScript1() = default;

	TweenScript1(int e) : e(e) {}

	void OnUpdate(float f) override {
		std::cout << "TweenScript1: " << e << " updated with " << f << "\n";
	}

	PTGN_SERIALIZER_REGISTER(TweenScript1, e)

private:
	int e{ 0 };
};

class TweenScript2 : public Script<TweenScript, TweenScript2> {
public:
	TweenScript2() = default;

	TweenScript2(int e) : e(e) {}

	void OnUpdate(float f) override {
		std::cout << "TweenScript2: " << e << " updated with " << f << "\n";
	}

	// PTGN_SERIALIZER_REGISTER(TweenScript2, e)

private:
	int e{ 0 };
};

// Do not serialize all components, but instead use a T system.

// TODO: To serialize an entity, write its
// UUID

// Number of components
// (Write zeros equivalent to the above)

// Size of each component.

// To serialize a component, write its
// Component Id
// Component size
// Component data

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	// static_assert(!nlohmann::detail::has_to_json<json, TweenScript1>::value);
	//  static_assert(nlohmann::detail::has_to_json<json, TweenScript1>::value);
	// static_assert(nlohmann::detail::has_to_json<json, TweenScript>::value);

	{
		std::unique_ptr<TweenScript> test;

		test = TweenScript::create<TweenScript1>(10);

		test->OnUpdate(0.1f);

		json j = *test;

		PTGN_LOG("Serialized script with name: ", test->GetName(), "\n", j.dump(4));

		SaveJson(j, "resources/myscripts.json");
	}
	{
		auto j{ LoadJson("resources/myscripts.json") };

		std::unique_ptr<TweenScript> test2{ std::make_unique<TweenScript1>() };
		j.get_to(*test2);

		test2->OnUpdate(0.5f);

		std::unique_ptr<TweenScript> test;

		test = TweenScript::create(j);

		PTGN_LOG("Deserialized script with name: ", test->GetName());

		test->OnUpdate(0.9f);

		// test = TweenScript::create(from_file, 10);
	}
}

*/