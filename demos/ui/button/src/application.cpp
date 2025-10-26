#include <iostream>
#include <ostream>

#include "core/ecs/components/draw.h"
#include "core/ecs/components/transform.h"
#include "core/app/application.h"
#include "core/scripting/script.h"
#include "core/scripting/script_interfaces.h"
#include "core/app/window.h"
#include "debug/core/log.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"
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
		Origin button_origin{ Origin::TopLeft };

		Application::Get().window_.SetResizable();
		b1 = CreateButton(*this);
		AddScript<ButtonScript1>(b1);
		SetPosition(b1, V2_float{ -100, -150 - 50 });
		b1.SetSize({ 200, 100 });
		SetDrawOrigin(b1, button_origin);
		b1.SetBackgroundColor(color::Pink);
		b1.SetBackgroundColor(color::Red, ButtonState::Hover);
		b1.SetBackgroundColor(color::DarkRed, ButtonState::Pressed);

		b2 = CreateToggleButton(*this, false);
		AddScript<ToggleButtonScript1>(b2);
		SetPosition(b2, V2_float{ -100, 150 - 50 });
		b2.SetSize({ 200, 100 });
		SetDrawOrigin(b2, button_origin);
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
		if (input.KeyDown(Key::Q)) {
			b1.Disable();
			b2.Disable();
			PTGN_LOG("Disabled both buttons");
		}
		if (input.KeyDown(Key::E)) {
			b1.Enable();
			b2.Enable();
			PTGN_LOG("Enabled both buttons");
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("ButtonScene: Q/E to disable/enable buttons");
	Application::Get().scene_.Enter<ButtonScene>("");
	return 0;
}
