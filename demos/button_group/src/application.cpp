#include "core/entity.h"
#include "core/game.h"
#include "debug/log.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "ui/button.h"

using namespace ptgn;

class ToggleButtonGroupScene : public Scene {
	ToggleButtonGroup group;

	ToggleButton CreateToggleButtonGroupItem(
		const V2_float& position, const ButtonCallback& on_activate
	) {
		ToggleButton b{ CreateToggleButton(manager, false, on_activate) };
		b.SetPosition(position);
		b.SetSize({ 200, 130 });
		b.SetOrigin(Origin::TopLeft);
		b.SetBackgroundColor(color::LightRed);
		b.SetBackgroundColor(color::Red, ButtonState::Hover);
		b.SetBackgroundColor(color::DarkRed, ButtonState::Pressed);
		b.SetBackgroundColorToggled(color::LightBlue);
		b.SetBackgroundColorToggled(color::Blue, ButtonState::Hover);
		b.SetBackgroundColorToggled(color::DarkBlue, ButtonState::Pressed);
		return b;
	}

	void Enter() override {
		group.Load("1", CreateToggleButtonGroupItem(V2_float{ 50, 50 }, []() { PTGN_LOG("1"); }));
		group.Load("2", CreateToggleButtonGroupItem(V2_float{ 50, 230 }, []() { PTGN_LOG("2"); }));
		group.Load("3", CreateToggleButtonGroupItem(V2_float{ 50, 410 }, []() { PTGN_LOG("3"); }));
		group.Load("4", CreateToggleButtonGroupItem(V2_float{ 50, 590 }, []() { PTGN_LOG("4"); }));
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ToggleButtonGroupScene");
	game.scene.Enter<ToggleButtonGroupScene>("");
	return 0;
}