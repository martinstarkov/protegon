#include "components/draw.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/script.h"
#include "core/script_interfaces.h"
#include "core/window.h"
#include "debug/log.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "ui/button.h"

using namespace ptgn;

struct ToggleButtonScript : public Script<ToggleButtonScript, ButtonScript> {
	ToggleButtonScript() {}

	explicit ToggleButtonScript(int number) : number{ number } {}

	void OnButtonActivate() override {
		PTGN_LOG("Clicked ", number);
	}

	int number{ 0 };
};

class ToggleButtonGroupScene : public Scene {
	ToggleButtonGroup group;

	ToggleButton CreateToggleButtonGroupItem(const V2_float& position, int number) {
		ToggleButton b{ CreateToggleButton(*this, false) };
		SetPosition(b, position);
		b.SetSize({ 200, 130 });
		SetDrawOrigin(b, Origin::TopLeft);
		b.SetBackgroundColor(color::LightRed);
		b.SetBackgroundColor(color::Red, ButtonState::Hover);
		b.SetBackgroundColor(color::DarkRed, ButtonState::Pressed);
		b.SetBackgroundColorToggled(color::LightBlue);
		b.SetBackgroundColorToggled(color::Blue, ButtonState::Hover);
		b.SetBackgroundColorToggled(color::DarkBlue, ButtonState::Pressed);
		AddScript<ToggleButtonScript>(b, number);
		return b;
	}

	void Enter() override {
		game.window.SetResizable();
		group = CreateToggleButtonGroup(*this);
		group.Load("1", CreateToggleButtonGroupItem(V2_float{ -100, -300 - 130 / 2 }, 1));
		group.Load("2", CreateToggleButtonGroupItem(V2_float{ -100, -100 - 130 / 2 }, 2));
		group.Load("3", CreateToggleButtonGroupItem(V2_float{ -100, 100 - 130 / 2 }, 3));
		group.Load("4", CreateToggleButtonGroupItem(V2_float{ -100, 300 - 130 / 2 }, 4));
		group.SetActive("3");
	}

	void Update() override {
		if (input.KeyDown(Key::I)) {
			PTGN_LOG("Active Toggle Button ID: ", group.GetActive().GetId());
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ToggleButtonGroupScene");
	game.scene.Enter<ToggleButtonGroupScene>("");
	return 0;
}