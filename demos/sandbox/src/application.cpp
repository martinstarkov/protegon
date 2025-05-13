#include "components/draw.h"
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

	void Update() override { /**/ }
};

int main() {
	game.Init("Sandbox");
	game.scene.Enter<Sandbox>("sandbox");
}

/*
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

// Entity class for position and transform
struct Vec2 {
	float x = 0.0f;
	float y = 0.0f;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(Vec2, x, y)
};

struct TransformComponent {
	Vec2 position;
};

class Entity {
	TransformComponent transform;

public:
	TransformComponent& GetComponent() {
		return transform;
	}

	void SetPosition(Vec2 pos) {
		transform.position = pos;
	}

	Vec2 GetPosition() const {
		return transform.position;
	}
};

// Base ScriptComponent class
class ScriptComponent {
public:
	virtual ~ScriptComponent() = default;

	virtual void OnCreate(Entity& entity)			= 0;
	virtual void OnUpdate(Entity& entity, float dt) = 0;

	virtual std::string GetTypeName() const = 0;
	virtual json Serialize() const			= 0;
	virtual void Deserialize(const json& j) = 0;
};

// Script Registry to hold and create scripts
class ScriptRegistry {
public:
	static ScriptRegistry& Instance() {
		static ScriptRegistry instance;
		return instance;
	}

	void Register(
		const std::string& typeName, std::function<std::unique_ptr<ScriptComponent>()> factory
	) {
		registry[typeName] = std::move(factory);
	}

	std::unique_ptr<ScriptComponent> Create(const std::string& typeName) {
		auto it = registry.find(typeName);
		return it != registry.end() ? it->second() : nullptr;
	}

private:
	std::unordered_map<std::string, std::function<std::unique_ptr<ScriptComponent>()>> registry;
};

template <typename Derived>
class ScriptComponentBase : public ScriptComponent {
public:
	// Constructor ensures that the static variable 'isRegistered' is initialized
	ScriptComponentBase() {
		// Ensuring static variable is initialized to trigger registration
		(void)isRegistered;
	}

	std::string GetTypeName() const override {
		return Derived::StaticTypeName();
	}

	json Serialize() const override {
		json j	  = *static_cast<const Derived*>(this); // Cast to Derived and serialize
		j["type"] = Derived::StaticTypeName();
		return j;
	}

	void Deserialize(const json& j) override {
		*static_cast<Derived*>(this) = j.get<Derived>(); // Deserialize into derived type
	}

private:
	// Static variable for ensuring class is registered once and for all
	static bool isRegistered;

	// The static Register function handles the actual registration of the class
	static bool Register() {
		ScriptRegistry::Instance().Register(
			Derived::StaticTypeName(),
			[]() -> std::unique_ptr<ScriptComponent> { return std::make_unique<Derived>(); }
		);
		return true;
	}
};

// Initialize static variable, which will trigger the Register function
template <typename Derived>
bool ScriptComponentBase<Derived>::isRegistered = ScriptComponentBase<Derived>::Register();

#define DEFINE_SCRIPT_COMPONENT(TYPE, ...)  \
public:                                     \
	using Base = ScriptComponentBase<TYPE>; \
	TYPE() : Base() {}                      \
	static std::string StaticTypeName() {   \
		return #TYPE;                       \
	}                                       \
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(TYPE, __VA_ARGS__)

class TweenMove : public ScriptComponentBase<TweenMove> {
public:
	float targetX = 0.0f, targetY = 0.0f;
	float duration = 1.0f;
	float elapsed  = 0.0f;
	Vec2 startPos;

	void OnCreate(Entity& entity) override {
		startPos = entity.GetComponent().position;
	}

	void OnUpdate(Entity& entity, float dt) override {
		elapsed		+= dt;
		float t		 = std::min(elapsed / duration, 1.0f);
		Vec2 newPos	 = { startPos.x + (targetX - startPos.x) * t,
						 startPos.y + (targetY - startPos.y) * t };
		entity.SetPosition(newPos);
	}

	DEFINE_SCRIPT_COMPONENT(TweenMove, targetX, targetY, duration)
};

class ScriptComponentContainer {
	std::vector<std::unique_ptr<ScriptComponent>> scripts;

public:
	void AddScript(const std::string& typeName, const json& config, Entity& owner) {
		auto script = ScriptRegistry::Instance().Create(typeName);
		if (script) {
			script->Deserialize(config);
			script->OnCreate(owner);
			scripts.push_back(std::move(script));
		}
	}

	void UpdateAll(Entity& owner, float dt) {
		for (auto& script : scripts) {
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

int main() {
	Entity entity;
	ScriptComponentContainer scriptContainer;

	// Simulate loading from JSON
	json scriptJson = {
		{ { "type", "TweenMove" }, { "targetX", 10.0 }, { "targetY", 5.0 }, { "duration", 2.0 } }
	};

	DeserializeScripts(scriptContainer, scriptJson, entity);

	// Simulate update loop
	for (int i = 0; i <= 20; ++i) {
		float dt = 0.1f; // Simulated delta time
		scriptContainer.UpdateAll(entity, dt);

		Vec2 pos = entity.GetPosition();
		std::cout << "Time: " << i * dt << "s - Position: (" << pos.x << ", " << pos.y << ")\n";
	}

	return 0;
}

*/

/*
#include "components/movement.h"
#include "core/game.h"
#include "core/game_object.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/collision/collider.h"
#include "renderer/color.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "serialization/file_stream_reader.h"
#include "serialization/file_stream_writer.h"
#include "serialization/stream_reader.h"
#include "serialization/stream_writer.h"
#include "utility/file.h"
#include "utility/log.h"

using namespace ptgn;

class Sandbox : public Scene {
public:
	struct OnCollision {
		virtual ~OnCollision() = default;

		virtual void Start(Collision c) {}

		virtual void Continue(Collision c) {}

		virtual void Stop(Collision c) {}
	};

	struct CollisionCallback {
		template <typename T>
		CollisionCallback(T&& t) : ptr{ std::make_unique<T>(t) } {}

		std::unique_ptr<OnCollision> ptr;
	};

	struct OnPlayerCollision : public OnCollision {
		OnPlayerCollision(ecs::Entity player) : player{ player } {}

		ecs::Entity player;

		void Start(Collision c) override {
			PTGN_LOG("Start | Player id: ", player.GetId());
		}

		void Continue(Collision c) override {
			PTGN_LOG("Continue | Player id: ", player.GetId());
		}

		void Stop(Collision c) override {
			PTGN_LOG("Stop | Player id: ", player.GetId());
		}

		static void Serialize(StreamWriter* w, const OnPlayerCollision& p) {
			w->Write(p.player.GetId());
		}

		static void Deserialize(StreamReader* r, OnPlayerCollision& p) {
			ecs::Index player_index;
			r->Read(player_index);
			PTGN_LOG("Deserialized player index: ", player_index);
		}
	};

	struct TargetPosition {
		V2_float start;
		V2_float stop;
	};

	ecs::Entity CreateSheep(ecs::Manager& m, const V2_float& position) {
		auto e = manager.CreateEntity();
		e.Add<Transform>(position);
		e.Add<Visible>();
		e.Add<TextureHandle>("sheep");
		e.Add<TargetPosition>();
		V2_float ws{ game.window.GetSize() };
		std::array<V2_float, 4> coordinates{
			V2_float{},
			V2_float{ ws.x, 0.0f },
			ws,
			V2_float{ 0.0f, ws.y },
		};
		e.Add<Tween>()
			.During(milliseconds{ 4000 })
			.Repeat(-1)
			.OnRepeat([e, coordinates]() {
				auto index = e.Get<Tween>().GetRepeats() % coordinates.size();
				PTGN_ASSERT(index < coordinates.size());
				e.Get<TargetPosition>().start = e.Get<Transform>().position;
				e.Get<TargetPosition>().stop  = coordinates[index];
			})
			.OnUpdate([e](float f) {
				e.Get<Transform>().position =
					Lerp(e.Get<TargetPosition>().start, e.Get<TargetPosition>().stop, f);
			})
			.Start();
		return e;
	}

	ecs::Entity sheep;

	void Exit() override {
		FileStreamWriter w{ "resources/sheep.bin" };
		w.WriteEntity<Transform, Visible, TextureHandle, TargetPosition, Tween>(sheep);
	}

	void Enter() override {
		camera.primary.SetPosition(game.window.GetCenter());

		game.texture.Load("sheep", "resources/test.png");

		sheep = CreateSheep(manager, V2_float{ 0, 0 });
		if (FileExists("resources/sheep.bin")) {
			FileStreamReader r{ "resources/sheep.bin" };
			r.ReadEntity<Transform, Visible, TextureHandle, TargetPosition, Tween>(sheep);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Sandbox", { 1280, 720 }, color::Transparent);
	game.scene.Enter<Sandbox>("sandbox");
	return 0;
}
*/

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
std::enable_if_t<is_self_factory_v<T>> to_json(json& nlohmann_json_j, const T& nlohmann_json_t) {
	nlohmann_json_t.to_json_impl(nlohmann_json_j);
}

template <typename T>
std::enable_if_t<is_self_factory_v<T>> from_json(const json& nlohmann_json_j, T& nlohmann_json_t) {
	nlohmann_json_t.from_json_impl(nlohmann_json_j);
}

template <typename ScriptInterface, typename ScriptType>
using Script = typename ScriptInterface::template Registrar<ScriptType>;

// TODO: Consider making this a template class and moving OnUpdate to a base class so pointers can
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

int main() {
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

/*

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
#include "core/transform.h"
#include "core/uuid.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/texture.h"
#include "serialization/binary_archive.h"
#include "serialization/fwd.h"
#include "serialization/json.h"
#include "serialization/serializable.h"
#include "serialization/type_traits.h"
#include "utility/assert.h"
#include "utility/log.h"
#include "utility/time.h"
#include "vfx/light.h"

using namespace ptgn;

class MyData {
public:
	MyData() : id(0), message(""), value(0.0f) {}

	int id;
	std::string message;
	float value;

	PTGN_SERIALIZER_REGISTER(MyData, id, message, value)
};

int main() {
	Manager m;

	auto e0 = m.CreateEntity();
	e0.Add<Transform>(V2_float{ -69, -69 });

	auto e1 = m.CreateEntity();
	e1.Add<Draggable>(V2_float{ 1, 1 }, V2_float{ 30, 40 }, true);
	e1.Add<Transform>(V2_float{ 30, 50 }, 2.14f, V2_float{ 2.0f });
	e1.Add<impl::AnimationInfo>(5, V2_float{ 32, 32 }, V2_float{ 0, 0 }, 0);
	e1.Add<Enabled>(true);
	e1.Add<Visible>(false);
	e1.Add<Depth>(22);
	e1.Add<DisplaySize>(V2_float{ 300, 400 });
	e1.Add<Tint>(color::Blue);
	e1.Add<LineWidth>(3.5f);
	e1.Add<TextureHandle>("sheep1");
	e1.Add<TextureCrop>(V2_float{ 1, 2 }, V2_float{ 11, 12 });
	e1.Add<RigidBody>();
	e1.Add<Interactive>();
	e1.Add<PointLight>()
		.SetRadius(250.0f)
		.SetIntensity(1.0f)
		.SetFalloff(3.0f)
		.SetColor(color::Pink)
		.SetAmbientIntensity(0.2f)
		.SetAmbientColor(color::Blue);
	e1.Add<impl::Offsets>();
	e1.Add<Circle>(25.0f);
	e1.Add<Arc>(25.0f, DegToRad(30.0f), DegToRad(60.0f));
	e1.Add<Ellipse>(V2_float{ 30, 40 });
	e1.Add<Capsule>(V2_float{ 100, 100 }, V2_float{ 200, 200 }, 35.0f);
	e1.Add<Line>(V2_float{ 200, 200 }, V2_float{ 300, 300 });
	e1.Add<Rect>(V2_float{ 100, 100 }, Origin::TopLeft);
	e1.Add<Polygon>(std::vector<V2_float>{ V2_float{ 200, 200 }, V2_float{ 300, 300 },
										   V2_float{ 600, 600 } });
	e1.Add<Triangle>(V2_float{ 0, 0 }, V2_float{ -300, -300 }, V2_float{ 600, 600 });
	e1.Add<Lifetime>(milliseconds{ 300 }).Start();

	{
		BinaryOutputArchive binary_output("resources/mydata.bin");
		binary_output.Write(e1);
	}
	{
		BinaryInputArchive binary_input("resources/mydata.bin");
		Entity e2;
		binary_input.Read(e2);

		std::cout << "Binary: transform=" << e2.Get<Transform>() << std::endl;
	}

	{
		json j = e1;

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
		PTGN_ASSERT(e2.Has<impl::AnimationInfo>());
		PTGN_ASSERT(e2.Has<TextureCrop>());
		PTGN_ASSERT(e2.Has<Enabled>());
		PTGN_ASSERT(e2.Has<Visible>());
		PTGN_ASSERT(e2.Has<Depth>());
		PTGN_ASSERT(e2.Has<DisplaySize>());
		PTGN_ASSERT(e2.Has<Tint>());
		PTGN_ASSERT(e2.Has<PointLight>());
		PTGN_ASSERT(e2.Has<LineWidth>());
		PTGN_ASSERT(e2.Has<TextureHandle>());
		PTGN_ASSERT(e2.Has<RigidBody>());
		PTGN_ASSERT(e2.Has<Interactive>());
		PTGN_ASSERT(e2.Has<impl::Offsets>());
		PTGN_ASSERT(e2.Has<Circle>());
		PTGN_ASSERT(e2.Has<Arc>());
		PTGN_ASSERT(e2.Has<Ellipse>());
		PTGN_ASSERT(e2.Has<Capsule>());
		PTGN_ASSERT(e2.Has<Line>());
		PTGN_ASSERT(e2.Has<Rect>());
		PTGN_ASSERT(e2.Has<Polygon>());
		PTGN_ASSERT(e2.Has<Triangle>());
		PTGN_ASSERT(e2.Has<Lifetime>());

		PTGN_LOG("Successfully deserialized all entity components");
	}

	{
		BinaryOutputArchive binary_output("resources/mydata.bin");
		MyData data1;
		data1.id	  = 123;
		data1.message = "Binary Data";
		data1.value	  = 3.14f;
		binary_output.Write(data1);
	}

	{
		BinaryInputArchive binary_input("resources/mydata.bin");
		MyData data2;
		binary_input.Read(data2);

		std::cout << "Binary: id=" << data2.id << ", message=\"" << data2.message
				  << "\", value=" << data2.value << std::endl;
	}

	{
		JsonOutputArchive json_output("resources/mydata.json");
		MyData data3;
		data3.id	  = 456;
		data3.message = "JSON Data";
		data3.value	  = 2.71f;

		json_output.Write("data3", data3);
	}

	{
		JsonInputArchive json_input("resources/mydata.json");
		MyData data4;

		json_input.Read("data3", data4);

		std::cout << "JSON: id=" << data4.id << ", message=\"" << data4.message
				  << "\", value=" << data4.value << std::endl;
	}

return 0;
}
*/