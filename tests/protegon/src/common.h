#pragma once

#include "protegon/game.h"
#include "utility/debug.h"

using namespace ptgn;

const static std::string test_instructions{ "'1' (cycle back); '2' (cycle forward)" };
const static std::vector<Key> test_switch_keys{ Key::ONE, Key::TWO };
extern V2_float ws;
extern V2_float center;

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
}

template <typename T, typename... Ts>
void TestLoop(
		const std::string& instructions, int& current_test, int test_count,
		const std::vector<Key>& keys, const T& function, const std::string& name,
		const Ts&... message
) {
	game.window.SetTitle((instructions + ": [" + std::to_string(current_test) + "] " + name).c_str()
	);
	PTGN_LOG("[", current_test, "] ", name, message...);

	Game::UpdateFunction loop_function{ std::function(function) };

	game.LoopUntilKeyDown(keys, [&](float dt) {
		CheckForTestSwitch(current_test, (int)test_count, keys);

		game.renderer.Clear();

		if (std::holds_alternative<std::function<void(float)>>(loop_function)) {
			std::get<std::function<void(float)>>(loop_function)(dt);
		} else {
			std::get<std::function<void(void)>>(loop_function)();
		}

		game.renderer.Present();
	});
}