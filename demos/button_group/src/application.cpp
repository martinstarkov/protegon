#include "core/game.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "ui/button.h"
#include "utility/log.h"

using namespace ptgn;

class ToggleButtonGroupExample : public Scene {
	ToggleButtonGroup g;

	V2_float size{ 200, 130 };
	float x1{ 50 };
	float x2{ 400 };
	float y{ 50 };
	float y_step{ 130 };

	ToggleButton CreateToggleButton(const V2_float& position, const ButtonCallback& on_activate) {
		ToggleButton b{ manager };
		b.SetPosition(position);
		b.SetRect(size, Origin::TopLeft);
		b.SetBackgroundColor(color::LightRed);
		b.SetBackgroundColor(color::Red, ButtonState::Hover);
		b.SetBackgroundColor(color::DarkRed, ButtonState::Pressed);
		b.SetBackgroundColorToggled(color::LightBlue);
		b.SetBackgroundColorToggled(color::Blue, ButtonState::Hover);
		b.SetBackgroundColorToggled(color::DarkBlue, ButtonState::Pressed);
		b.OnActivate(on_activate);
		return b;
	}

	void Enter() override {
		g.Clear();

		y_step = 180;

		g.Load("1", CreateToggleButton(V2_float{ x1, y + y_step * 0 }, []() { PTGN_LOG("1"); }));
		g.Load("2", CreateToggleButton(V2_float{ x1, y + y_step * 1 }, []() { PTGN_LOG("2"); }));
		g.Load("3", CreateToggleButton(V2_float{ x1, y + y_step * 2 }, []() { PTGN_LOG("3"); }));
		g.Load("4", CreateToggleButton(V2_float{ x1, y + y_step * 3 }, []() { PTGN_LOG("4"); }));
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ToggleButtonGroupExample");
	game.scene.Enter<ToggleButtonGroupExample>("toggle_button_group_example");
	return 0;
}