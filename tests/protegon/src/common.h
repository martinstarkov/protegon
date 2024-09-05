#pragma once

#include "protegon/game.h"
#include "utility/debug.h"

using namespace ptgn;

const static std::string test_instructions{ "'1' (--test); '2' (++test), 'ESC' (++category)" };
const static std::vector<Key> test_switch_keys{ Key::ONE, Key::TWO };
extern V2_float ws;
extern V2_float center;

namespace ptgn {

struct Test {
	virtual ~Test() = default;

	virtual void Init() {}

	virtual void Update(float dt) {}

	virtual void Update() {}

	virtual void Draw() {}

	virtual void Run(float dt) final {
		if (!initialized_) {
			Init();
			initialized_ = true;
		}
		Update(dt);
		Update();
		Draw();
	}

private:
	bool initialized_{ false };
};

// TODO: Make this a template to get count.
void CheckForTestSwitch(int& current_test, int test_count, const std::vector<Key>& keys) {
	PTGN_ASSERT(keys.size() == 2);
	if (game.input.KeyDown(keys[0])) {
		current_test--;
		current_test = Mod(current_test, test_count);
	} else if (game.input.KeyDown(keys[1])) {
		current_test++;
		current_test = Mod(current_test, test_count);
	}
	if (game.input.KeyDown(Key::ESCAPE)) {
		game.PopLoopFunction();
	}
}

template <typename T, typename... Ts>
void TestLoop(
	float dt, const std::string& instructions, int& current_test, int test_count,
	const std::vector<Key>& keys, const T& function, const std::string& name, const Ts&... message
) {
	game.window.SetTitle((instructions + ": [" + std::to_string(current_test) + "] " + name).c_str()
	);
	// PTGN_LOG("[", current_test, "] ", name, message...);

	Game::UpdateFunction loop_function{ std::function(function) };

	CheckForTestSwitch(current_test, (int)test_count, keys);

	game.renderer.DrawPoint(game.input.GetMousePosition(), color::Red, 2.0f);

	if (std::holds_alternative<std::function<void(float)>>(loop_function)) {
		std::invoke(std::get<std::function<void(float)>>(loop_function), dt);
	} else {
		std::invoke(std::get<std::function<void(void)>>(loop_function));
	}
}

} // namespace ptgn