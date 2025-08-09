// #include <functional>
//
// #include "common/assert.h"
// #include "core/game.h"
// #include "core/script.h"
// #include "core/window.h"
// #include "debug/log.h"
// #include "input/input_handler.h"
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
//				game.scene.Transition<OtherScene>("", "other", {});
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
//		game.window.SetSetting(WindowSetting::Resizable);
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
//	game.Init("EventScene", window_size);
//	game.scene.Enter<EventScene>("");
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

#include "core/script.h"

using namespace ptgn;

struct TestScript : public Script<TestScript, GlobalMouseScript, KeyScript> {
	void OnMouseMove() {
		PTGN_LOG("Mouse moved");
	}

	void OnKeyDown(Key k) {
		PTGN_LOG("Key down: ", k);
	}
};

int main() {
	// Instead of storing scripts in one container, store them in a unordered map of vectors of
	// scripts. When a script is registered, it uses a similar method to Serialize() to retrieve a
	// list of Enums of what maps that scripts should be inserted into.
	// Then, when cycling we simply cycle through that enum type of the unordered map.
	// When removing a script, we again retrieve all the enum types and remove from all those maps.
	// This changes every dynamic cast to simply a found in map check or not. And then we can static
	// cast as well.

	Scripts test;

	test.AddScript<TestScript>();

	test.Invoke(&GlobalMouseScript::OnMouseMove);
	test.Invoke(&KeyScript::OnKeyDown, Key::W);

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
	}
}