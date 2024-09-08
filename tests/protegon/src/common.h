#pragma once

#include <array>
#include <string>
#include <string_view>

#include "protegon/game.h"
#include "utility/debug.h"

using namespace ptgn;

const static std::string test_instructions{ "'1' (--test); '2' (++test), 'ESC' (++category)" };
const static std::array<Key, 2> test_switch_keys{ Key::ONE, Key::TWO };
const static Key test_category_switch_key{ Key::ESCAPE };

namespace ptgn {

struct Test {
	virtual ~Test() = default;

	virtual std::string_view GetName() const final {
		return PTGN_FULL_FUNCTION_SIGNATURE;
	}

	void Setup() {
		ws	   = game.window.GetSize();
		center = game.window.GetCenter();
	}

	virtual void Init() {}

	virtual void Update(float dt) {}

	virtual void Update() {}

	virtual void Draw() {}

	virtual void Run(float dt) final {
		if (!initialized_) {
			Setup();
			Init();
			initialized_ = true;
		}
		Update(dt);
		Update();
		Draw();
	}

protected:
	V2_float ws;	 // window size
	V2_float center; // window center
private:
	bool initialized_{ false };
};

// TODO: Make this a template to get count.
void CheckForTestSwitch(int& current_test, int test_count) {
	PTGN_ASSERT(test_switch_keys.size() == 2);
	if (game.input.KeyDown(test_switch_keys[0])) {
		current_test--;
		current_test = Mod(current_test, test_count);
	} else if (game.input.KeyDown(test_switch_keys[1])) {
		current_test++;
		current_test = Mod(current_test, test_count);
	}
	if (game.input.KeyDown(test_category_switch_key)) {
		game.PopLoopFunction();
	}
}

void AddTests(
	const std::vector<std::shared_ptr<Test>>& tests, const V2_int& window_size = { 800, 800 },
	const Color& window_color = color::White
) {
	// Lambda capture will keep this alive as long as is necessary.
	std::shared_ptr<int> test_idx = std::make_shared<int>(0);

	game.PushLoopFunction([=](float dt) {
		game.window.SetSize(window_size);
		game.renderer.SetClearColor(window_color);

		PTGN_ASSERT(*test_idx < tests.size());

		auto& current_test = tests[*test_idx];

		game.window.SetTitle(
			test_instructions + std::string(": ") + std::string(current_test->GetName())
		);

		current_test->Run(dt);

		CheckForTestSwitch(*test_idx, static_cast<int>(tests.size()));
	});
}

} // namespace ptgn