#include <iostream>
#include <ostream>

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

class ButtonScene : public Scene {
public:
	Button b1;
	ToggleButton b2;

	void Enter() override {
		b1 = CreateButton(manager, []() { PTGN_LOG("Clicked regular button"); });
		b1.SetPosition(V2_float{ 50, 50 });
		b1.SetSize({ 200, 100 });
		b1.SetOrigin(Origin::TopLeft);
		b1.SetBackgroundColor(color::Pink);
		b1.SetBackgroundColor(color::Red, ButtonState::Hover);
		b1.SetBackgroundColor(color::DarkRed, ButtonState::Pressed);

		b2 = CreateToggleButton(manager, false, []() { PTGN_LOG("Toggled button"); });
		b2.SetPosition(V2_float{ 50, 300 });
		b2.SetSize({ 200, 100 });
		b2.SetOrigin(Origin::TopLeft);
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
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ButtonScene");
	game.scene.Enter<ButtonScene>("");
	return 0;
}
