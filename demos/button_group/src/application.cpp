#include "protegon/protegon.h"

using namespace ptgn;

class ToggleButtonGroupExample : public Scene {
	ToggleButtonGroup g;

	V2_float size{ 200, 70 };
	float x1{ 50 };
	float x2{ 400 };
	float y{ 50 };
	float y_step{ 130 };

	Texture t_default{ "resources/default.png" };
	Texture t_hover{ "resources/hover.png" };
	Texture t_pressed{ "resources/pressed.png" };
	Texture t_default_disabled{ "resources/default_disabled.png" };
	Texture t_toggled_default{ "resources/toggled_default.png" };
	Texture t_toggled_hover{ "resources/toggled_hover.png" };
	Texture t_toggled_pressed{ "resources/toggled_pressed.png" };
	Texture t_toggled_default_disabled{ "resources/toggled_default_disabled.png" };

	template <typename T>
	Button CreateTexturedButton(
		std::string_view text_content, const V2_float& pos, const V2_float& size,
		const T& activate = nullptr, Origin origin = Origin::TopLeft
	) {
		Button b;
		b.SetRect(Rect{ pos, size, origin });
		b.Set<ButtonProperty::Texture>(t_default);
		b.Set<ButtonProperty::Texture>(t_hover, ButtonState::Hover);
		b.Set<ButtonProperty::Texture>(t_pressed, ButtonState::Pressed);
		b.Set<ButtonProperty::Texture>(t_default_disabled, ButtonState::Default, false, true);

		b.Set<ButtonProperty::Text>(Text{ text_content, color::White });
		b.Set<ButtonProperty::OnActivate>(activate);
		b.Set<ButtonProperty::Bordered>(true);
		b.Set<ButtonProperty::BorderColor>(color::Cyan);
		b.Set<ButtonProperty::BorderThickness>(5.0f);
		return b;
	}

	template <typename T = ButtonCallback>
	Button CreateTexturedToggleButton(
		std::string_view text_content, const V2_float& pos, const V2_float& size,
		const T& activate = nullptr, Origin origin = Origin::TopLeft
	) {
		Button b{ CreateTexturedButton(text_content, pos, size, activate, origin) };

		b.Set<ButtonProperty::Toggleable>(true);
		b.Set<ButtonProperty::Texture>(t_toggled_default, ButtonState::Default, true, false);
		b.Set<ButtonProperty::Texture>(t_toggled_hover, ButtonState::Hover, true, false);
		b.Set<ButtonProperty::Texture>(t_toggled_pressed, ButtonState::Pressed, true, false);
		b.Set<ButtonProperty::Texture>(
			t_toggled_default_disabled, ButtonState::Default, true, true
		);
		return b;
	}

	void Enter() override {
		g.Clear();

		size   = V2_float{ 200, 130 };
		y_step = 180;

		g.Load("1", CreateTexturedToggleButton("1", V2_float{ x1, y + y_step * 0 }, size, []() {
				   PTGN_LOG("1");
			   }));
		g.Load("2", CreateTexturedToggleButton("2", V2_float{ x1, y + y_step * 1 }, size, []() {
				   PTGN_LOG("2");
			   }));
		g.Load("3", CreateTexturedToggleButton("3", V2_float{ x1, y + y_step * 2 }, size, []() {
				   PTGN_LOG("3");
			   }));
		g.Load("4", CreateTexturedToggleButton("4", V2_float{ x1, y + y_step * 3 }, size, []() {
				   PTGN_LOG("4");
			   }));
	}

	void Update() override {
		auto& m = game.event.mouse;
		g.Draw();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ToggleButtonGroupExample");
	game.scene.Enter<ToggleButtonGroupExample>("toggle_button_group_example");
	return 0;
}
