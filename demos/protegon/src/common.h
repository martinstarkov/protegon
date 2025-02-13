#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/game.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/math.h"
#include "math/vector2.h"
#include "renderer/renderer.h"
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
		game.window.Center();
		game.renderer.SetClearColor(color::White);
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
					game.camera.ResetPrimary();
					Shutdown();
					game.window.SetTitle("");
					game.window.Center();
					game.event.window.Unsubscribe(this);
					Deinit();
				})
			);
			Setup();
			Init();
			initialized_ = true;
			return;
		}
		Update();
		Draw();
	}

	float dt{ 0.0f };

	void Deinit() {
		initialized_ = false;
	}

protected:
	V2_float ws;	 // window size
	V2_float center; // window center
private:
	friend void CheckForTestSwitch(
		const std::vector<std::shared_ptr<Test>>& tests, int& current_test
	);

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
		tests[current_test]->Shutdown();
		tests[current_test]->Deinit();
		game.window.SetTitle("");
		game.window.SetSize({ 800, 800 });
		game.window.Center();
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
	}
}

void AddTests(const std::vector<std::shared_ptr<Test>>& tests) {
	// Lambda capture will keep this alive as long as is necessary.
	auto test_idx = std::make_shared<int>(0);
}

} // namespace ptgn