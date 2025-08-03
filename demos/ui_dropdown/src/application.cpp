#include <functional>
#include <string_view>

#include "core/entity.h"
#include "core/game.h"
#include "debug/log.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "ui/button.h"
#include "ui/dropdown.h"

using namespace ptgn;

class ButtonScript : public Script<ButtonScript> {
public:
	ButtonScript() = default;

	explicit ButtonScript(const std::function<void()>& on_activate_callback) :
		on_activate{ on_activate_callback } {}

	void OnButtonActivate() override {
		if (on_activate) {
			std::invoke(on_activate);
		}
	}

	std::function<void()> on_activate;
};

class DropdownScene : public Scene {
public:
	Button CreateButton(std::string_view content, const std::function<void()>& on_activate) {
		Button button{ CreateTextButton(*this, content, color::White) };
		button.SetBackgroundColor(color::Gray);
		button.SetBackgroundColor(color::LightGray, ButtonState::Hover);
		button.SetBackgroundColor(color::DarkGray, ButtonState::Pressed);
		button.AddScript<ButtonScript>(on_activate);
		button.SetBorderColor(color::Red);
		button.SetBorderWidth(3.0f);
		return button;
	}

	Dropdown CreateDropdown(bool open = false) {
		Dropdown d;
		d = CreateDropdownButton(*this, open);
		d.SetText("Dropdown", color::Yellow);
		d.SetBackgroundColor(color::Gray);
		d.SetBackgroundColor(color::LightGray, ButtonState::Hover);
		d.SetBackgroundColor(color::DarkGray, ButtonState::Pressed);
		d.SetPosition({ 400, 200 });
		d.SetSize({ 200, 100 });
		d.SetOrigin(Origin::Center);
		d.SetBorderColor(color::Gold);
		d.SetBorderWidth(3.0f);
		d.SetButtonSize({ 100, 50 });
		d.SetDropdownDirection(Origin::CenterBottom);
		return d;
	}

	void Enter() override {
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

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("DropdownScene", { 800, 800 });
	game.scene.Enter<DropdownScene>("");
	return 0;
}
