#include <functional>
#include <string_view>

#include "app/application.h"
#include "app/context.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "platform/window/window.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/ui/button.h"
#include "runtime/ui/dropdown.h"

using namespace ptgn;

class DropdownScene : public Scene {
public:
	Button CreateButton(std::string_view content, const std::function<void()>& on_activate) {
		Button button{ CreateTextButton(*this, content, color::White) };
		button.SetBackgroundColor(color::Gray);
		button.SetBackgroundColor(color::LightGray, ButtonState::Hover);
		button.SetBackgroundColor(color::DarkGray, ButtonState::Pressed);
		button.OnActivate(on_activate);
		button.SetBorderColor(color::Red);
		button.SetBorderWidth(3.0f);
		return button;
	}

	Dropdown CreateDropdown(bool open = false) {
		Dropdown d = CreateDropdownButton(*this, open)
						 .SetText("Dropdown", color::Yellow)
						 .SetBackgroundColor(color::Gray)
						 .SetBackgroundColor(color::LightGray, ButtonState::Hover)
						 .SetBackgroundColor(color::DarkGray, ButtonState::Pressed)
						 .SetSize({ 200, 100 })
						 .SetBorderColor(color::Gold)
						 .SetBorderWidth(3.0f)
						 .SetButtonSize(V2_float{ 100, 50 })
						 .SetDropdownDirection(Origin::CenterBottom);
		// SetDrawOrigin(d, Origin::Center);
		SetPosition(d, -app().renderer.GetGameSize() * 0.5f + V2_float{ 400, 200 });
		return d;
	}

	void OnEnter() override {
		input.SetInteractiveSettings({ .enabled = true });

		Dropdown dropdown  = CreateDropdown();
		Dropdown dropdown2 = CreateDropdown(false);
		Dropdown dropdown3 = CreateDropdown(true);
		Dropdown dropdown4 = CreateDropdown(false);

		dropdown.AddButton(CreateButton("First", []() { PTGN_LOG("Pressed first"); }));
		dropdown.AddButton(CreateButton("Second", []() { PTGN_LOG("Pressed second"); }));
		dropdown.AddButton(dropdown2);
		// dropdown.SetDropdownOrigin(Origin::CenterTop);
		dropdown2.AddButton(CreateButton("Third", []() { PTGN_LOG("Pressed third"); }));
		dropdown2.AddButton(CreateButton("Fourth", []() { PTGN_LOG("Pressed fourth"); }));
		dropdown2.AddButton(CreateButton("Fifth", []() { PTGN_LOG("Pressed fifth"); }));
		dropdown2.AddButton(dropdown3);
		dropdown2.SetText("Dropdown 2", color::Yellow);

		dropdown2.SetDropdownOrigin(Origin::CenterRight);
		dropdown3.AddButton(CreateButton("Sixth", []() { PTGN_LOG("Pressed sixth"); }));
		dropdown3.AddButton(dropdown4);
		dropdown3.SetDropdownDirection(Origin::CenterLeft);
		dropdown3.SetDropdownOrigin(Origin::CenterLeft);
		dropdown3.SetText("Dropdown 3", color::Yellow);

		dropdown4.AddButton(CreateButton("Seventh", []() { PTGN_LOG("Pressed seventh"); }));
		dropdown4.AddButton(CreateButton("Eight", []() { PTGN_LOG("Pressed eight"); }));
		dropdown4.AddButton(CreateButton("Ninth", []() { PTGN_LOG("Pressed ninth"); }));
		dropdown4.SetText("Dropdown 4", color::Yellow);
		dropdown4.SetDropdownOrigin(Origin::CenterTop);
		dropdown4.SetDropdownDirection(Origin::CenterTop);
		dropdown2.SetSize({ 200, 50 });
		dropdown3.SetSize({ 200, 50 });
		dropdown4.SetSize({ 200, 50 });
		// dropdown3.SetButtonOffset();
	}
};

int main(int, char**) {
	Application game{ "DropdownScene" };
	game.StartWith<DropdownScene>();
}
