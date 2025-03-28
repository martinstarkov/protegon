#include <string_view>

#include "components/draw.h"
#include "components/input.h"
#include "core/transform.h"
#include "core/game.h"
#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "protegon/protegon.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/text.h"
#include "scene/scene.h"
#include "ui/button.h"
#include "utility/log.h"

using namespace ptgn;

template <typename T>
Button CreateColorButton(
	ecs::Manager& manager, std::string_view text_content, const V2_float& position,
	const V2_float& size, const T& activate = nullptr, Origin origin = Origin::Center
) {
	Button b{ manager };
	b.SetPosition(position);
	b.SetRect(size, origin);
	b.SetBackgroundColor(color::Pink);
	b.SetBackgroundColor(color::Red, ButtonState::Hover);
	b.SetBackgroundColor(color::DarkRed, ButtonState::Pressed);
	b.OnActivate(activate);
	/*b.SetTextContent(text_content);
	b.SetTextColor(color::White);
	b.SetBordered(true);
	b.SetBorderColor(color::Cyan);
	b.SetBorderThickness(5.0f);*/
	return b;
}

/*
class ButtonExample : public Scene {
public:
	std::string Str(ButtonState s) const {
		switch (s) {
			case ButtonState::Default: return "default";
			case ButtonState::Hover:   return "hover";
			case ButtonState::Pressed: return "pressed";
		}
		PTGN_ERROR("Invalid button state");
	}

	std::string Str(impl::InternalButtonState s) const {
		switch (s) {
			case impl::InternalButtonState::HeldOutside:  return "held outside";
			case impl::InternalButtonState::Hover:		  return "hover";
			case impl::InternalButtonState::HoverPressed: return "hover pressed";
			case impl::InternalButtonState::IdleDown:	  return "idle down";
			case impl::InternalButtonState::IdleUp:		  return "idle up";
			case impl::InternalButtonState::Pressed:	  return "pressed";
		}
		PTGN_ERROR("Invalid internal button state");
	}

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

	template <typename T>
	Button CreateColorButton(
		std::string_view text_content, const V2_float& pos, const V2_float& size,
		const T& activate = nullptr, Origin origin = Origin::TopLeft
	) {
		Button b;
		b.SetRect(Rect{ pos, size, origin });
		b.Set<ButtonProperty::BackgroundColor>(color::Black);
		b.Set<ButtonProperty::BackgroundColor>(color::Silver, ButtonState::Hover);
		b.Set<ButtonProperty::BackgroundColor>(color::DarkBrown, ButtonState::Pressed);
		b.Set<ButtonProperty::BackgroundColor>(color::DarkRed, ButtonState::Default, false, true);

		b.Set<ButtonProperty::Text>(Text{ text_content, color::White });
		b.Set<ButtonProperty::OnActivate>(activate);
		b.Set<ButtonProperty::Bordered>(true);
		b.Set<ButtonProperty::BorderColor>(color::Cyan);
		b.Set<ButtonProperty::BorderThickness>(5.0f);
		return b;
	}

	template <typename T>
	Button CreateColorToggleButton(
		std::string_view text_content, const V2_float& pos, const V2_float& size,
		const T& activate = nullptr, Origin origin = Origin::TopLeft
	) {
		Button b{ CreateColorButton(text_content, pos, size, activate, origin) };
		b.Set<ButtonProperty::Toggleable>(true);
		b.Set<ButtonProperty::BackgroundColor>(color::Gray, ButtonState::Default);
		b.Set<ButtonProperty::BackgroundColor>(color::Pink, ButtonState::Default, true);
		b.Set<ButtonProperty::BackgroundColor>(color::Orange, ButtonState::Hover, true);
		b.Set<ButtonProperty::BackgroundColor>(color::Red, ButtonState::Pressed, true);
		b.Set<ButtonProperty::BackgroundColor>(color::Gray, ButtonState::Default, false, true);
		b.Set<ButtonProperty::BackgroundColor>(color::Pink, ButtonState::Default, true, true);
		return b;
	}

	Button button;
	Button toggle_button;
	Button textured_button;
	Button textured_toggle_button;
	Button disabled_button;
	Button disabled_toggle_button;
	Button disabled_toggle_button2;
	Button disabled_textured_button;
	Button disabled_textured_toggle_button;
	Button disabled_textured_toggle_button2;

	V2_float size{ 200, 70 };
	float x1{ 50 };
	float x2{ 400 };
	float y{ 50 };
	float y_step{ 130 };

	void Enter() override {
		button			= CreateColorButton("Color", V2_float{ x1, y }, size, []() {
			 PTGN_LOG("Clicked regular button");
		 });
		textured_button = CreateTexturedButton("Texture", V2_float{ x2, y }, size, []() {
			PTGN_LOG("Clicked textured button");
		});

		// Disabled buttons.

		disabled_button =
			CreateColorButton("Disabled Color", V2_float{ x1, y + y_step }, size, []() {
				PTGN_WARN("Cannot click disabled regular button. Something went wrong?");
			});
		disabled_textured_button =
			CreateTexturedButton("Disabled Texture", V2_float{ x2, y + y_step }, size, []() {
				PTGN_WARN("Cannot click disabled textured button. Something went wrong?");
			});

		// Toggle buttons.

		toggle_button =
			CreateColorToggleButton("Color Toggle", V2_float{ x1, y + y_step * 2 }, size, []() {
				PTGN_LOG("Clicked toggle button");
			});
		textured_toggle_button = CreateTexturedToggleButton(
			"Texture Toggle", V2_float{ x2, y + y_step * 2 }, size,
			[]() { PTGN_LOG("Clicked textured toggle button"); }
		);

		// Disabled toggle buttons.

		disabled_toggle_button = CreateColorToggleButton(
			"Disabled Color Toggle", V2_float{ x1, y + y_step * 3 }, size,
			[]() { PTGN_WARN("Cannot click disabled toggle button. Something went wrong?"); }
		);
		disabled_toggle_button2 = CreateColorToggleButton(
			"Disabled Color Toggle 2", V2_float{ x1, y + y_step * 4 }, size,
			[]() { PTGN_WARN("Cannot click disabled toggle button. Something went wrong?"); }
		);
		disabled_textured_toggle_button = CreateTexturedToggleButton(
			"Disabled Texture Toggle", V2_float{ x2, y + y_step * 3 }, size,
			[]() {
				PTGN_WARN("Cannot click disabled textured toggle button.  Something went wrong?");
			}
		);
		disabled_textured_toggle_button2 = CreateTexturedToggleButton(
			"Disabled Texture Toggle 2", V2_float{ x2, y + y_step * 4 }, size,
			[]() {
				PTGN_WARN("Cannot click disabled textured toggle button.  Something went wrong?");
			}
		);

		disabled_button.Disable();
		disabled_toggle_button.Disable();
		disabled_textured_button.Disable();
		disabled_textured_toggle_button.Disable();

		disabled_toggle_button2.Set<ButtonProperty::Toggled>(true);
		disabled_textured_toggle_button2.Set<ButtonProperty::Toggled>(true);
		disabled_toggle_button2.Disable();
		disabled_textured_toggle_button2.Disable();
	}

	void DrawStateLabels(const Button& b) {
		Text{ Str(b.GetState()), color::Green }.Draw({ b.GetRect().Center() -
													   V2_float{ 0.0f, 50.0f } });
		Text{ Str(b.GetInternalState()), color::Orange }.Draw({ b.GetRect().Center() +
																V2_float{ 0.0f, 50.0f } });
	}

	void Update() override {
		auto& m = game.event.mouse;
		button.Draw();
		toggle_button.Draw();
		textured_button.Draw();
		textured_toggle_button.Draw();
		DrawStateLabels(button);
		DrawStateLabels(textured_button);
		DrawStateLabels(toggle_button);
		DrawStateLabels(textured_toggle_button);
		disabled_button.Draw();
		disabled_toggle_button.Draw();
		disabled_textured_button.Draw();
		disabled_textured_toggle_button.Draw();
		disabled_toggle_button2.Draw();
		disabled_textured_toggle_button2.Draw();
		DrawStateLabels(disabled_button);
		DrawStateLabels(disabled_textured_button);
		DrawStateLabels(disabled_toggle_button);
		DrawStateLabels(disabled_toggle_button2);
		DrawStateLabels(disabled_textured_toggle_button);
		DrawStateLabels(disabled_textured_toggle_button2);
	}
};

*/

class ButtonExampleScene : public Scene {
public:
	V2_float size{ 200, 100 /*200, 70*/ };
	float x1{ 0 /*50*/ };
	float x2{ 0 /*400*/ };
	float y{ 50 };
	float y_step{ 130 };

	Button b1;
	ToggleButton b2;

	void Enter() override {
		b1 = CreateColorButton(
			manager, "Color", V2_float{ x1, y }, size, []() { PTGN_LOG("Clicked regular button"); },
			Origin::TopLeft
		);

		b2 = ToggleButton{ manager };
		b2.SetPosition(V2_float{ x1, y + 1 * (size.y + y_step) });
		b2.SetRect(size, Origin::TopLeft);
		b2.SetBackgroundColor(color::LightRed);
		b2.SetBackgroundColor(color::Red, ButtonState::Hover);
		b2.SetBackgroundColor(color::DarkRed, ButtonState::Pressed);
		b2.OnActivate([]() { PTGN_LOG("Toggled button"); });
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
	game.Init("Button Example", { 1280, 720 }, color::Transparent);
	game.scene.Enter<ButtonExampleScene>("button_example_scene");
	return 0;
}
