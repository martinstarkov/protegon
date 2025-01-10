#include "protegon/protegon.h"

using namespace ptgn;

class DropdownExample : public Scene {
public:
	Button dropdown;

	Button CreateButton(std::string_view content, const ButtonCallback& on_activate) {
		Button button;
		button.Set<ButtonProperty::BackgroundColor>(color::Gray);
		button.Set<ButtonProperty::BackgroundColor>(color::LightGray, ButtonState::Hover);
		button.Set<ButtonProperty::BackgroundColor>(color::DarkGray, ButtonState::Pressed);
		button.Set<ButtonProperty::Text>(Text{ content, color::White });
		button.Set<ButtonProperty::OnActivate>(on_activate);
		button.Set<ButtonProperty::Bordered>(true);
		button.Set<ButtonProperty::BorderColor>(color::DarkGray);
		button.Set<ButtonProperty::BorderThickness>(2.0f);
		return button;
	}

	void Init() override {
		dropdown.SetRect({ { 300, 300 }, { 200, 100 }, Origin::TopLeft });
		dropdown.Set<ButtonProperty::BackgroundColor>(color::Gray);
		dropdown.Set<ButtonProperty::BackgroundColor>(color::LightGray, ButtonState::Hover);
		dropdown.Set<ButtonProperty::BackgroundColor>(color::DarkGray, ButtonState::Pressed);
		dropdown.Set<ButtonProperty::Text>(Text{ "Dropdown", color::Silver });
		dropdown.Set<ButtonProperty::Bordered>(true);
		dropdown.Set<ButtonProperty::BorderColor>(color::Black);
		dropdown.Set<ButtonProperty::BorderThickness>(3.0f);
		Dropdown d;
		d.Add(CreateButton("First", []() { PTGN_LOG("Pressed first"); }));
		d.Add(CreateButton("Second", []() { PTGN_LOG("Pressed second"); }));
		d.Add(CreateButton("Third", []() { PTGN_LOG("Pressed third"); }));
		d.SetButtonSize({ 200, 50 });
		d.SetDropdownDirection(Origin::CenterBottom);
		dropdown.Set<ButtonProperty::Dropdown>(d);
		dropdown.Set<ButtonProperty::OnActivate>([=]() mutable {
			PTGN_LOG("Toggling dropdown");
			d.Toggle();
		});
	}

	void Update() override {
		dropdown.Draw();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Dropdown Example", { 800, 800 });
	game.scene.LoadActive<DropdownExample>("dropdown");
	return 0;
}
