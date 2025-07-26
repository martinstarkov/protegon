#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iosfwd>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "components/common.h"
#include "components/draw.h"
#include "components/input.h"
#include "components/lifetime.h"
#include "components/offsets.h"
#include "core/entity.h"
#include "core/manager.h"
#include "math/math.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "serialization/binary_archive.h"
#include "serialization/fwd.h"
#include "serialization/json.h"
#include "serialization/serializable.h"

using namespace ptgn;

class MyData {
public:
	MyData() : id(0), message(""), value(0.0f) {}

	int id;
	std::string message;
	float value;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(MyData, id, message, value)
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Manager manager;
	Entity entity{ manager.CreateEntity() };
	entity.SetPosition({ 30, 50 });

	/*json entity_json = entity.Serialize<Transform, UUID>();

	PTGN_LOG(entity_json.dump(4));

	Entity test{ manager.CreateEntity() };
	test.Deserialize<Transform, UUID>(entity_json);

	json entity_json2 = test.Serialize<Transform, UUID>();

	PTGN_LOG(entity_json2.dump(4));*/

	/*
	entity.Add<BoxCollider>(V2_float{ 100, 120 });
	manager.Refresh();

	json j = manager;

	std::string s = j.dump(4);

	PTGN_LOG("Manager: ", s);

	Manager manager2;

	j.get_to(manager2);
	*/

	/*json j;
	j["type"] = "TweenMove";
	j["data"] = { { "target_x", 20.0 }, { "target_y", 25.0 }, { "duration", 3.0 } };*/

	/*
	json j = json::array();
	j.push_back(
		{ { "type", "TweenMove" },
		  { "data", { { "target_x", 20.0 }, { "target_y", 25.0 }, { "duration", 3.0 } } } }
	);
	j.push_back({ { "type", "TweenMove2" } });

	PTGN_LOG(j.dump(4));

	j.get_to(script_container);

	// script_container.AddScript<TweenMove>(30.0f, 35.0f, 3.0f);
	// script_container.AddScript<TweenMove2>();

	for (const auto& script : script_container.scripts) {
		script->OnCreate(entity);
	}

	float dt = 0.1f; // Simulated delta time

	// Simulate update loop
	for (int i = 0; i <= 30; ++i) {
		V2_float pos = entity.GetPosition();
		PTGN_LOG("Time: ", static_cast<float>(i) * dt, "s - Position: ", pos);
		for (const auto& script : script_container.scripts) {
			script->OnUpdate(entity, dt);
		}
	}

	json j2 = script_container;

	PTGN_LOG(j2.dump(4));
	*/

	Manager m;

	auto e0 = m.CreateEntity();
	e0.SetPosition(V2_float{ -69, -69 });

	auto e1 = m.CreateEntity();
	e1.Add<Draggable>(V2_float{ 1, 1 }, V2_float{ 30, 40 }, true);
	e1.SetTransform({ V2_float{ 30, 50 }, 2.14f, V2_float{ 2.0f } });
	e1.Enable();
	e1.Hide();
	e1.SetDepth(22);
	auto tint_color{ color::Blue };
	e1.Add<Tint>(tint_color);
	e1.Add<LineWidth>(3.5f);
	e1.Add<TextureHandle>("sheep1");
	e1.Add<TextureCrop>(V2_float{ 1, 2 }, V2_float{ 11, 12 });
	e1.Add<RigidBody>();
	e1.Add<Interactive>();
	e1.Add<impl::Offsets>(
	); // Transforms will be serialized as nulls because they are default values.
	e1.Add<Lifetime>(milliseconds{ 300 }).Start();

	{
		json j = e1.Serialize();

		SaveJson(j, "resources/mydata.json");

		PTGN_LOG("Successfully serialized all entity components: ", j.dump(4));

		RNG<float> rng{ 3, 0.5f, 1.5f };
		json j2 = rng;

		PTGN_LOG("Successfully serialized rng: ", j2.dump(4));

		RNG<float> rng2;
		j2.get_to(rng2);

		PTGN_ASSERT(rng2.GetSeed() == 3);
		PTGN_ASSERT(rng2.GetMin() == 0.5f);
		PTGN_ASSERT(rng2.GetMax() == 1.5f);
	}

	{
		auto j{ LoadJson("resources/mydata.json") };

		Entity e2{ m.CreateEntity(j) };

		PTGN_ASSERT(e2.Has<Transform>());
		PTGN_ASSERT(e2.Has<UUID>());
		PTGN_ASSERT(e2.Has<Draggable>());
		PTGN_ASSERT(e2.Has<TextureCrop>());
		PTGN_ASSERT(e2.Has<Enabled>());
		PTGN_ASSERT(e2.Has<Visible>());
		PTGN_ASSERT(e2.Has<Depth>());
		PTGN_ASSERT(e2.Has<Tint>());
		PTGN_ASSERT(e2.Get<Tint>() == tint_color);
		PTGN_ASSERT(e2.Has<LineWidth>());
		PTGN_ASSERT(e2.Has<TextureHandle>());
		PTGN_ASSERT(e2.Has<RigidBody>());
		PTGN_ASSERT(e2.Has<Interactive>());
		PTGN_ASSERT(e2.Has<impl::Offsets>());
		PTGN_ASSERT(e2.Get<impl::Offsets>().bounce == Transform{});
		PTGN_ASSERT(e2.Has<Lifetime>());

		PTGN_LOG("Successfully deserialized all entity components");
	}

	/*{
		JsonOutputArchive json_output("resources/mydata.json");
		MyData data3;
		data3.id	  = 456;
		data3.message = "JSON Data";
		data3.value	  = 2.71f;

		json_output.Write("data3", data3);
	}*/

	/*{
		JsonInputArchive json_input("resources/mydata.json");
		MyData data4;

		json_input.Read("data3", data4);

		std::cout << "JSON: id=" << data4.id << ", message=\"" << data4.message
				  << "\", value=" << data4.value << std::endl;
	}*/

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

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(TweenScript1, e)

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

	// PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(TweenScript2, e)

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