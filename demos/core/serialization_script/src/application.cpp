// #include <functional>
//
// #include "core/assert.h"
// #include "core/app/application.h"
//
// #include "core/app/window.h"
// #include "core/log.h"
// #include "core/input/input_handler.h"
// #include "math/vector2.h"
// #include "scene/scene.h"
// #include "scene/scene_manager.h"
//
// using namespace ptgn;
//
// constexpr V2_int window_size{ 800, 800 };
//
// class OtherScene : public Scene {
//	void Enter() {
//		PTGN_LOG("Entered other scene");
//	}
// };
//
// struct TestScript : public Script<TestScript> {
//	void OnKeyDown(Key k) {
//		if (entity.GetId() == 4) {
//			if (k == Key::R) {
//				// PTGN_LOG("Removing test script from ", entity.GetId());
//				// entity.RemoveScript<TestScript>();
//
//				// entity.Destroy();
//				// PTGN_LOG("Destroying entity: ", entity.GetId());
//				Application::Get().scene_.Transition<OtherScene>("", "other", {});
//			} else {
//				PTGN_WARN("Should not be here after pressing R");
//			}
//		} else {
//			PTGN_LOG("Key down script for: ", entity.GetId(), ", key: ", k);
//		}
//	}
// };
//
// struct TestScript2 : public Script<TestScript2> {
//	void OnKeyDown(Key k) {
//		PTGN_LOG("Key down on ", entity.GetId(), ": ", k);
//	}
//
//	void OnKeyPressed(Key k) {
//		PTGN_LOG("Key pressed on ", entity.GetId(), ": ", k);
//	}
//
//	void OnKeyUp(Key k) {
//		PTGN_LOG("Key up on ", entity.GetId(), ": ", k);
//	}
// };
//
// struct TestScript3 : public Script<TestScript3> {
//	void OnWindowResized(V2_int size) {
//		PTGN_LOG("Entity: ", entity.GetId(), ", window resized: ", size);
//	}
// };
//
// class EventScene : public Scene {
// public:
//	Entity e1;
//	Entity e2;
//
//	void Enter() override {
//		Application::Get().window_.SetResizable();
//
//		e1 = CreateEntity();
//		e2 = CreateEntity();
//		AddScript<TestScript>(e1);
//		AddScript<TestScript2>(e1);
//		AddScript<TestScript3>(e1);
//		AddScript<TestScript>(e2);
//		AddScript<TestScript2>(e2);
//	}
//
//	void Update() override {}
// };
//
// int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
//	Application::Get().Init("EventScene", window_size);
//	Application::Get().scene_.Enter<EventScene>("");
//	return 0;
// }

/*
#include <functional>
#include <iostream>
#include <optional>
#include <utility>
#include <variant>

// === Helper ===
#define VARIANT(...) __VA_ARGS__

// === Event Enums ===
enum class MouseEvent {
	Down,
	Move
};
enum class KeyEvent {
	Down,
	Up
};

// === Handler Struct Generator ===
#define DEFINE_HANDLER_FIELD(EnumName, name, type) std::optional<type> EnumName##_##name;

// Trait to detect if T is a specialization of std::variant
template <typename T>
struct is_variant : std::false_type {};

template <typename... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type {};

template <typename T>
constexpr bool is_variant_v = is_variant<std::remove_cv_t<std::remove_reference_t<T>>>::value;

// === Dispatch Helpers ===
template <typename Func, typename... Args>
void dispatch_helper(Func& fn, Args&&... args) {
	if constexpr (is_variant_v<Func>) {
		// For variant: visit all alternatives, call if invocable
		std::visit(
			[&](auto&& f) {
				using F = decltype(f);
				if constexpr (std::is_invocable_v<F, Args...>) {
					f(std::forward<Args>(args)...);
				}
			},
			fn
		);
	} else {
		// Plain std::function or callable
		if constexpr (std::is_invocable_v<Func, Args...>) {
			fn(std::forward<Args>(args)...);
		}
	}
}

// === Dispatcher Generator ===
#define DEFINE_DISPATCH_CASE(EnumType, name, type)                                               \
	case EnumType::name:                                                                         \
		if (EnumType##_##name) dispatch_helper(*EnumType##_##name, std::forward<Args>(args)...); \
		break;

#define DEFINE_REGISTER_CASE(EnumType, name, type) \
	case EnumType::name: {                         \
		EnumType##_##name = Function;              \
		break;                                     \
	}

#define DEFINE_DISPATCHER(EnumType, LIST)                       \
	LIST(EnumType, DEFINE_HANDLER_FIELD)                        \
	template <typename... Args>                                 \
	void Dispatch(EnumType event, Args&&... args) {             \
		switch (event) { LIST(EnumType, DEFINE_DISPATCH_CASE) } \
	}                                                           \
	template <auto Function>                                    \
	void Register(EnumType event) {                             \
		switch (event) { LIST(EnumType, DEFINE_REGISTER_CASE) } \
	}

// === Event Macros ===
#define MOUSE_EVENT_LIST(EnumType, X)                                                         \
	X(EnumType, Down, VARIANT(std::variant<std::function<void()>, std::function<void(int)>>)) \
	X(EnumType, Move, std::function<void(int, int)>)

#define KEY_EVENT_LIST(EnumType, X)             \
	X(EnumType, Down, std::function<void(int)>) \
	X(EnumType, Up, std::function<void()>)

struct Registry {
	DEFINE_DISPATCHER(MouseEvent, MOUSE_EVENT_LIST)
	DEFINE_DISPATCHER(KeyEvent, KEY_EVENT_LIST)
};

struct TestClass {
	static void mouseDown() {
		std::cout << "Mouse down (0 args)\n";
	}

	static void mouseDownArg(int arg) {
		std::cout << "Mouse down (1 arg) " << arg << "\n";
	}

	static void mouseMove(int x, int y) {
		std::cout << "Mouse move to " << x << "," << y << "\n";
	}

	static void keyDownArg(int k) {
		std::cout << "Key down (1 arg) " << k << "\n";
	}

	static void keyUp() {
		std::cout << "Key up (0 arg)\n";
	}
};

// === Example ===
int main() {
	Registry r;

	r.Register<&TestClass::mouseDown>(MouseEvent::Down);
	// r.Register<&TestClass::mouseDownArg>(MouseEvent::Down);
	// r.Register<&TestClass::mouseMove>(MouseEvent::Move);
	// r.Register<&TestClass::keyDownArg>(KeyEvent::Down);
	// r.Register<&TestClass::keyUp>(KeyEvent::Up);

	// Dispatch some events
	r.Dispatch(MouseEvent::Down);
	r.Dispatch(MouseEvent::Move, 200, 400);
	r.Dispatch(KeyEvent::Down, 65);
	r.Dispatch(KeyEvent::Up);

	return 0;
}

*/
#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/app/manager.h"
#include "core/scripting/script.h"

using namespace ptgn;

struct TestScript : public Script<TestScript, GlobalMouseScript, KeyScript> {
	void OnMouseMove() {
		PTGN_LOG("Mouse moved 1");
	}

	void OnKeyDown(Key k) {
		PTGN_LOG("Key down 1: ", k);
	}
};

struct TestScript2 : public Script<TestScript2, GlobalMouseScript> {
	void OnMouseMove() {
		PTGN_LOG("Mouse moved 2");
	}
};

struct TestScript3 : public Script<TestScript3, TweenScript> {};

struct TestScript4 : public Script<TestScript4, GlobalMouseScript, TweenScript> {};

int main() {
	// Instead of storing scripts in one container, store them in a unordered map of vectors of
	// scripts. When a script is registered, it uses a similar method to Serialize() to retrieve a
	// list of Enums of what maps that scripts should be inserted into.
	// Then, when cycling we simply cycle through that enum type of the unordered map.
	// When removing a script, we again retrieve all the enum types and remove from all those maps.
	// This changes every dynamic cast to simply a found in map check or not. And then we can static
	// cast as well.

	// TODO: Consider using script_types as a hash type thing instead of having a map with separate
	// function pointers.

	Manager m;

	auto e1{ m.CreateEntity() };
	auto e2{ m.CreateEntity() };
	auto e3{ m.CreateEntity() };
	auto e4{ m.CreateEntity() };

	Scripts test;

	auto& script1 = AddScript<TestScript>(e1);
	auto& script2 = AddScript<TestScript2>(e1);
	auto& script3 = AddScript<TestScript3>(e1);
	auto& script4 = AddScript<TestScript4>(e1);

	// script3.test		= 69.0f;
	// script4.test		= 79.0f;
	// script4.mouse_index = 33.0f;

	test.AddAction(&GlobalMouseScript::OnMouseMove);
	test.AddAction(&KeyScript::OnKeyDown, Key::W);
	test.AddAction(&GlobalMouseScript::OnMouseMove);

	test.InvokeActions();

	json j1 = script1.Serialize();
	PTGN_LOG("script1: ", j1.dump(4));

	json j2 = script2.Serialize();
	PTGN_LOG("script2: ", j2.dump(4));

	json j3 = script3.Serialize();
	PTGN_LOG("script3: ", j3.dump(4));

	json j4 = script4.Serialize();
	PTGN_LOG("script4: ", j4.dump(4));

	const auto create_script_from_json = [&](const json& j) {
		std::string class_name{ j.at("type").get<std::string>() };
		auto instance{ impl::ScriptRegistry<impl::IScript>::Instance().Create(class_name) };
		if (instance) {
			instance->entity = m.CreateEntity();
			instance->Deserialize(j);
		}
		return instance;
	};

	auto script1_remade = std::dynamic_pointer_cast<TestScript>(create_script_from_json(j1));
	auto script2_remade = std::dynamic_pointer_cast<TestScript2>(create_script_from_json(j2));
	auto script3_remade = std::dynamic_pointer_cast<TestScript3>(create_script_from_json(j3));
	auto script4_remade = std::dynamic_pointer_cast<TestScript4>(create_script_from_json(j4));

	PTGN_ASSERT(script1_remade);
	PTGN_ASSERT(script2_remade);
	PTGN_ASSERT(script3_remade);
	PTGN_ASSERT(script4_remade);

	// PTGN_ASSERT(script1_remade->mouse_index == 0.0f);
	// PTGN_ASSERT(script2_remade->mouse_index == 0.0f);
	// PTGN_ASSERT(script3_remade->test == 69.0f);
	// PTGN_ASSERT(script4_remade->test == 79.0f);
	// PTGN_ASSERT(script4_remade->mouse_index == 33.0f);

	PTGN_LOG("Scripts deserialized correctly");
	/*
	std::weak_ptr<CollisionScript> weak = entity.GetComponent<CollisionScript>();

	std::unordered_map<ScriptType, std::vector<std::function<void()>>> queues;

	queues[ScriptType::Collision].emplace_back([weak]() {
		if (auto script = weak.lock()) {
			script->OnCollisionStart(...);
		}
	});

	for (auto& [type, queue] : queues) {
		for (auto& cb : queue) {
			cb();
		}
		queue.clear();
	}*/
}