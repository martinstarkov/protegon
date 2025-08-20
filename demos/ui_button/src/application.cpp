#include <iostream>
#include <ostream>

#include "components/draw.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/script.h"
#include "core/script_interfaces.h"
#include "core/window.h"
#include "debug/log.h"
#include "input/input_handler.h"
#include "input/key.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "ui/button.h"

using namespace ptgn;

struct ButtonScript1 : public Script<ButtonScript1, ButtonScript> {
	void OnButtonActivate() override {
		PTGN_LOG("Clicked regular button");
	}
};

struct ToggleButtonScript1 : public Script<ToggleButtonScript1, ButtonScript> {
	void OnButtonActivate() override {
		PTGN_LOG("Toggled button");
	}
};

class ButtonScene : public Scene {
public:
	Button b1;
	ToggleButton b2;

	void Enter() override {
		game.window.SetSetting(WindowSetting::Resizable);
		b1 = CreateButton(*this);
		AddScript<ButtonScript1>(b1);
		SetPosition(b1, V2_float{ 50, 50 });
		b1.SetSize({ 200, 100 });
		SetDrawOrigin(b1, Origin::TopLeft);
		b1.SetBackgroundColor(color::Pink);
		b1.SetBackgroundColor(color::Red, ButtonState::Hover);
		b1.SetBackgroundColor(color::DarkRed, ButtonState::Pressed);

		b2 = CreateToggleButton(*this, false);
		AddScript<ToggleButtonScript1>(b2);
		SetPosition(b2, V2_float{ 50, 300 });
		b2.SetSize({ 200, 100 });
		SetDrawOrigin(b2, Origin::TopLeft);
		b2.SetBackgroundColor(color::LightRed);
		b2.SetBackgroundColor(color::Red, ButtonState::Hover);
		b2.SetBackgroundColor(color::DarkRed, ButtonState::Pressed);
		b2.SetBackgroundColorToggled(color::LightBlue);
		b2.SetBackgroundColorToggled(color::Blue, ButtonState::Hover);
		b2.SetBackgroundColorToggled(color::DarkBlue, ButtonState::Pressed);
	}

	void Update() override {
		static impl::InternalButtonState state{ impl::InternalButtonState::IdleUp };
		if (auto s{ b1.GetInternalState() }; state != s) {
			state = s;
			std::cout << "Button 1 internal state: " << state << std::endl;
		}
		if (game.input.KeyDown(Key::Q)) {
			b1.Disable();
			b2.Disable();
			PTGN_LOG("Disabled both buttons");
		}
		if (game.input.KeyDown(Key::E)) {
			b1.Enable();
			b2.Enable();
			PTGN_LOG("Enabled both buttons");
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ButtonScene: Q/E to disable/enable buttons");
	game.scene.Enter<ButtonScene>("");
	return 0;
}
