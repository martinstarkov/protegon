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
		e.Add<TextureKey>("sheep");
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
		w.WriteEntity<Transform, Visible, TextureKey, TargetPosition, Tween>(sheep);
	}

	void Enter() override {
		camera.primary.SetPosition(game.window.GetCenter());

		game.texture.Load("sheep", "resources/test.png");

		sheep = CreateSheep(manager, V2_float{ 0, 0 });
		if (FileExists("resources/sheep.bin")) {
			FileStreamReader r{ "resources/sheep.bin" };
			r.ReadEntity<Transform, Visible, TextureKey, TargetPosition, Tween>(sheep);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Sandbox", { 1280, 720 }, color::Transparent);
	game.scene.Enter<Sandbox>("sandbox");
	return 0;
}
*/

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
#include "serialization/serializable.h"
#include "serialization/type_traits.h"
#include "utility/macro.h"
#include "utility/type_info.h"

using namespace ptgn;

template <class Base, class... Args>
class Factory {
public:
	virtual ~Factory() = default;

	virtual std::string_view GetName() const {
		return name_;
	}

	template <
		typename BasicJsonType, nlohmann::detail::enable_if_t<
									nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>
	friend void to_json(BasicJsonType& nlohmann_json_j, const Factory& nlohmann_json_t) {
		static_cast<const Base*>(&nlohmann_json_t)->to_json_impl(nlohmann_json_j);
	}

	template <
		typename BasicJsonType, nlohmann::detail::enable_if_t<
									nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>
	friend void from_json(const BasicJsonType& nlohmann_json_j, Factory& nlohmann_json_t) {
		static_cast<Base*>(&nlohmann_json_t)->from_json_impl(nlohmann_json_j);
	}

	virtual void to_json_impl(json& j) const {}

	virtual void from_json_impl(const json& j) {}

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
		auto it = data().find(Hash(class_name));
		if (it == data().end()) {
			std::cout << "Failed to find constructor hash for " << class_name << std::endl;
		}
		auto ptr{ it->second(std::forward<Ts>(args)...) };
		ptr->name_ = class_name;
		return ptr;
	}

	static std::unique_ptr<Base> create(const json& j) {
		std::string_view class_name{ j["name"] };
		auto it = dataJ().find(Hash(class_name));
		if (it == dataJ().end()) {
			std::cout << "Failed to find json constructor hash for " << class_name << std::endl;
		}
		auto ptr{ it->second(j) };
		ptr->name_ = class_name;
		return ptr;
	}

	template <class T>
	struct Registrar : public Base {
		friend T;

		void to_json_impl(json& j) const final {
			PTGN_ASSERT(
				tt::is_to_json_convertible_v<T>,
				"Cannot serialize script type without a to_json function"
			);
			j		  = *static_cast<const T*>(this);
			j["name"] = GetName();
		}

		void from_json_impl(const json& j) final {
			PTGN_ASSERT(
				tt::is_from_json_convertible_v<T>,
				"Cannot deserialize script type without a from_json function"
			);
			j.get_to(*static_cast<T*>(this));
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

		static bool registerTJ() {
			constexpr std::string_view class_name{ type_name<T>() };
			std::cout << "Registering json constructor hash for " << class_name << std::endl;
			Factory::dataJ()[Hash(class_name)] = [](const json& j) -> std::unique_ptr<Base> {
				auto ptr{ std::make_unique<T>() };
				j.get_to(*ptr);
				return ptr;
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

private:
	class Key {
		Key(){};
		template <class T>
		friend struct Registrar;
	};

	using FactoryFuncType  = std::unique_ptr<Base> (*)(Args...);
	using FactoryFuncTypeJ = std::unique_ptr<Base> (*)(const json&);
	Factory()			   = default;

	static auto& data() {
		static std::unordered_map<std::size_t, FactoryFuncType> s;
		return s;
	}

	static auto& dataJ() {
		static std::unordered_map<std::size_t, FactoryFuncTypeJ> s;
		return s;
	}

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

struct TweenScript : Factory<TweenScript, int> {
	TweenScript(Key) {}

	virtual ~TweenScript() override = default;

	virtual void OnUpdate(float f) {}
};

template <typename BaseScript, typename TScript>
using Script = typename BaseScript::template Registrar<TScript>;

template <typename TScript>
using TweenScriptClass = Script<TweenScript, TScript>;

class TweenScript1 : public TweenScriptClass<TweenScript1> {
public:
	TweenScript1() = default;

	TweenScript1(int e) : e(e) {}

	void OnUpdate(float f) override {
		std::cout << "TweenScript1: " << e << " updated with " << f << "\n";
	}

	// PTGN_SERIALIZER_REGISTER(TweenScript1, e)

private:
	int e{ 0 };
};

class TweenScript2 : public TweenScriptClass<TweenScript1> {
public:
	TweenScript2() = default;

	TweenScript2(int e) : e(e) {}

	void OnUpdate(float f) override {
		std::cout << "TweenScript2: " << e << " updated with " << f << "\n";
	}

	PTGN_SERIALIZER_REGISTER(TweenScript2, e)

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

		std::unique_ptr<TweenScript> test;

		test = TweenScript::create(j);

		PTGN_LOG("Deserialized script with name: ", test->GetName());

		test->OnUpdate(0.9f);

		// test = TweenScript::create(from_file, 10);
	}

	/*
	std::string_view from_file{ "TweenScript1" };

	std::cout << "Deserializing script with name: " << from_file << std::endl;

	// TODO: Check if from_file is a TweenScript.
	test = TweenScript::create(from_file, 10);

	test->OnUpdate(0.9f);
	*/

	// std::cout << "Start\n";
	// auto x = TweenScript::create("TweenScript1", 3);
	// auto y = TweenScript::create("TweenScript2", 2);
	// auto x = create_tween_script<TweenScript1>(3);
	// auto y = create_tween_script<TweenScript2>(2);
	// x->OnUpdate(0.1f);
	// y->OnUpdate(0.1f);
	// std::cout << "Name of x for serialization: " << x->GetName() << "\n";
	// std::cout << "Name of y for serialization: " << y->GetName() << "\n";
	// std::cout << "Stop\n";
	return 0;
}

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
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
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
	e1.Add<TextureKey>("sheep1");
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
		PTGN_ASSERT(e2.Has<TextureKey>());
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