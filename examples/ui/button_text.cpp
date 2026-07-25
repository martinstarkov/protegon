#include "app/application.h"
#include "core/editor.h"
#include "core/event/event.h"
#include "core/event/key_event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "renderer/text/text_layout.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/text/text.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"

using namespace ptgn;

class ButtonTextScene : public Scene {
public:
	Button disabled_button;

	static constexpr V2_float button_size{ 260, 86 };
	static constexpr Padding text_padding{ 14, 8 };

	Button CreateTextButton(V2_float position) {
		Button button{ CreateButton(*this, position, button_size) };

		button.Background().Border().TextAutoBox(true).TextPadding(text_padding);

		return button;
	}

	void OnEnter() override {
		ctx().debug.settings.interaction.draw_enabled = true;

		SetBackgroundColor(color::LightGray);

		{
			// Basic auto-boxed centered text.
			Button button{ CreateTextButton({ -170, -250 }) };

			button.Text()
				.Content("Auto-boxed\ncentered text")
				.Color(color::Black)
				.Size(21.0f)
				.Align(HorizontalAlign::Center, VerticalAlign::Center);

			button.OnPress([]() { PTGN_LOG("Pressed centered text button"); });
		}

		{
			// Button padding + word wrap.
			Button button{ CreateTextButton({ 170, -250 }) };

			button.TextPadding({ 22, 10 });

			button.Text()
				.Content("This text uses the button padding and wraps inside the button.")
				.Color(color::Black)
				.Size(16.0f)
				.Align(HorizontalAlign::Center, VerticalAlign::Center)
				.Wrap(WrapMode::Word)
				.Overflow(OverflowMode::Clip);

			button.OnPress([]() { PTGN_LOG("Pressed wrapped text button"); });
		}

		{
			// Ellipsis in a fixed button text box.
			Button button{ CreateTextButton({ -170, -125 }) };

			button.Text()
				.Content("A very long single-line button text that should end with ellipsis")
				.Color(color::Black)
				.Size(18.0f)
				.Align(HorizontalAlign::Left, VerticalAlign::Center)
				.Wrap(WrapMode::None)
				.Overflow(OverflowMode::Ellipsis)
				.MaxLines(1);

			button.OnPress([]() { PTGN_LOG("Pressed ellipsis text button"); });
		}

		{
			// State-specific texts. No base text is created, so the texts do not overlap.
			Button button{ CreateButton(*this, { 170, -125 }, button_size) };

			button.Background().Border();

			button.TextAutoBox(true, ButtonVisualState::Idle);
			button.TextAutoBox(true, ButtonVisualState::Hover);
			button.TextAutoBox(true, ButtonVisualState::Press);

			button.TextPadding({ 14, 8 }, ButtonVisualState::Idle);
			button.TextPadding({ 14, 8 }, ButtonVisualState::Hover);
			button.TextPadding({ 14, 8 }, ButtonVisualState::Press);

			button.Text(ButtonVisualState::Idle)
				.Content("Idle text")
				.Color(color::Black)
				.Size(22.0f)
				.Align(HorizontalAlign::Center, VerticalAlign::Center);

			button.Text(ButtonVisualState::Hover)
				.Clear()
				.Content("Hover text")
				.Color(color::Blue)
				.Size(24.0f)
				.Bold(true)
				.Align(HorizontalAlign::Center, VerticalAlign::Center);

			button.Text(ButtonVisualState::Press)
				.Clear()
				.Content("Pressed text")
				.Color(color::Red)
				.Size(21.0f)
				.Align(HorizontalAlign::Center, VerticalAlign::Center)
				.OuterGlow(color::Red.WithAlpha(140), 3.0f, 2.0f);

			button.OnPress([]() { PTGN_LOG("Pressed state-specific text button"); });
		}

		// Disabled-state text. Press Q/E to disable/enable.
		disabled_button = CreateButton(*this, { -170, 0 }, button_size);

		disabled_button.Background().Border();

		disabled_button.TextAutoBox(true, ButtonVisualState::Idle);
		disabled_button.TextAutoBox(true, ButtonVisualState::Hover);
		disabled_button.TextAutoBox(true, ButtonVisualState::Disabled);

		disabled_button.Text(ButtonVisualState::Idle)
			.Content("Q to Disable")
			.Color(color::Black)
			.Size(23.0f)
			.Align(HorizontalAlign::Center, VerticalAlign::Center);

		disabled_button.Text(ButtonVisualState::Hover)
			.Clear()
			.Content("Hover enabled")
			.Color(color::Blue)
			.Size(23.0f)
			.Align(HorizontalAlign::Center, VerticalAlign::Center);

		disabled_button.Text(ButtonVisualState::Disabled)
			.Clear()
			.Content("E to Enable")
			.Color(color::White)
			.Size(23.0f)
			.Align(HorizontalAlign::Center, VerticalAlign::Center)
			.Strikethrough();

		disabled_button.OnPress([]() { PTGN_LOG("Pressed disable-demo button"); });

		{
			// Rich text inside a button text.
			Button button{ CreateTextButton({ 170, 0 }) };

			Text text{ button.Text() };

			text.Content("Rich ")
				.Color(color::Black)
				.Size(20.0f)
				.Align(HorizontalAlign::Center, VerticalAlign::Center)
				.Wrap(WrapMode::Word)
				.Overflow(OverflowMode::Clip);

			text.Content("red ").Color(color::Red).Bold(true);

			text.Content("underlined ").Color(color::Blue).Bold(false).Underline();

			text.Content("text").Color(color::Black).Underline(false);

			button.OnPress([]() { PTGN_LOG("Pressed rich text button"); });
		}
	}

	void OnEvent(Event event) override {
		event.Dispatch<event::KeyPressed>([this](const auto& key) {
			if (key == Key::Q) {
				disabled_button.Disable();
				PTGN_LOG("Disabled bottom-left button");
			}

			if (key == Key::E) {
				disabled_button.Enable();
				PTGN_LOG("Enabled bottom-left button");
			}
		});
	}
};

int main(int, char**) {
	Application app{ "ButtonTextScene: Q/E to disable/enable one button" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<ButtonTextScene>();
}