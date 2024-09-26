#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/window.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "protegon/game.h"
#include "protegon/math.h"
#include "protegon/vector2.h"
#include "utility/debug.h"

using namespace ptgn;

const static std::string test_instructions{ "'ESC' (++category), '1' (--test); '2' (++test)" };
const static std::array<Key, 2> test_switch_keys{ Key::ONE, Key::TWO };
const static Key test_category_switch_key{ Key::ESCAPE };

namespace ptgn {

struct Test {
	virtual ~Test() = default;

	void Setup() {
		ws	   = game.window.GetSize();
		center = game.window.GetCenter();
	}

	virtual void Init() {
		/**/
	}

	virtual void Update() {
		/**/
	}

	virtual void Draw() {
		/**/
	}

	virtual void Shutdown() {
		/**/
	}

	virtual void Run() final {
		dt = game.dt();
		if (!initialized_) {
			game.event.window.Subscribe(
				WindowEvent::Quit, this, std::function([this](const WindowQuitEvent&) {
					game.camera.Reset();
					Shutdown();
					game.window.SetTitle("");
					game.event.window.Unsubscribe(this);
					game.PopBackLoopFunction();
				})
			);
			Setup();
			Init();
			initialized_ = true;
		}
		Update();
		Draw();
	}

	float dt{ 0.0f };

protected:
	V2_float ws;	 // window size
	V2_float center; // window center
private:
	bool initialized_{ false };
};

struct EntityTest : public Test {
public:
	ecs::Manager manager;
	ecs::Entity entity;

	EntityTest() {
		entity = manager.CreateEntity();
		manager.Refresh();
	}
};

void CheckForTestSwitch(const std::vector<std::shared_ptr<Test>>& tests, int& current_test) {
	PTGN_ASSERT(test_switch_keys.size() == 2);
	int test_count{ static_cast<int>(tests.size()) };
	auto shutdown = [&]() {
		game.camera.Reset();
		tests[current_test]->Shutdown();
		game.window.SetTitle("");
		game.event.window.Unsubscribe(tests[current_test].get());
	};

	if (game.input.KeyDown(test_switch_keys[0])) {
		shutdown();
		current_test--;
		current_test = Mod(current_test, test_count);
	} else if (game.input.KeyDown(test_switch_keys[1])) {
		shutdown();
		current_test++;
		current_test = Mod(current_test, test_count);
	}
	if (game.input.KeyDown(test_category_switch_key)) {
		shutdown();
		game.PopBackLoopFunction();
	}
}

void AddTests(const std::vector<std::shared_ptr<Test>>& tests) {
	// Lambda capture will keep this alive as long as is necessary.
	auto test_idx = std::make_shared<int>(0);

	game.PushFrontLoopFunction([tests, test_idx]() {
		PTGN_ASSERT(*test_idx < tests.size());

		auto& current_test = tests[*test_idx];

		if (game.window.GetTitle().empty()) {
			game.window.SetTitle(test_instructions + std::string(": ") + std::to_string(*test_idx));
		}

		current_test->Run();

		CheckForTestSwitch(tests, *test_idx);
	});
}

} // namespace ptgn