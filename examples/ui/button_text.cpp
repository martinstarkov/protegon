#include "app/application.h"
#include "core/editor.h"
#include "core/event/event.h"
#include "core/event/key_event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/tint.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"

using namespace ptgn;

constexpr V2_int logical_size{ 800, 800 };

class ButtonTextScene : public Scene {
public:
	Button disabled_button;

	static constexpr V2_float button_size{ 260, 86 };

	static void ConfigureBackground(Button button, ButtonVisualState state, Color color) {
		Entity background{ button.Background(state) };

		background.Add<Rect>(Rect{ button_size });
		SetDraw<RectDraw>(background);
		SetTint(background, color);
		SetDrawOrigin(background, Origin::Center);
	}

	static void ConfigureBasicBackgrounds(Button button) {
		ConfigureBackground(button, ButtonVisualState::Idle, color::White);
		ConfigureBackground(button, ButtonVisualState::Hover, color::LightGray);
		ConfigureBackground(button, ButtonVisualState::Press, color::Gray);
		ConfigureBackground(button, ButtonVisualState::Disabled, color::DarkGray);
	}

	Button CreateTextButton(V2_float position) {
		Button button{ CreateButton(*this, position, Rect{ button_size }, Origin::Center) };

		ConfigureBasicBackgrounds(button);

		button.SetLabelAutoBox(true);
		button.SetLabelPadding(Rect{ { 14, 8 }, { 14, 8 } });

		return button;
	}

	void OnEnter() override {
		ctx().renderer.SetLogicalSize(logical_size);
		ctx().asset.Load("arial", "assets/Arial.ttf");
		ctx().interaction.SetDebugSettings({ .draw_enabled = true });

		SetBackgroundColor(color::LightGray);

		// Basic auto-boxed centered text.
		{
			Button button{ CreateTextButton({ -170, -250 }) };

			button.Label()
				.Font("arial")
				.Content("Auto-boxed\ncentered label")
				.Color(color::Black)
				.Size(21.0f)
				.Align(HorizontalAlign::Center, VerticalAlign::Center);

			button.OnPress([]() { PTGN_LOG("Pressed centered text button"); });
		}

		// Button padding + word wrap.
		{
			Button button{ CreateTextButton({ 170, -250 }) };

			button.SetLabelPadding(Rect{ { 22, 10 }, { 22, 10 } });

			button.Label()
				.Font("arial")
				.Content("This label uses the button padding and wraps inside the button.")
				.Color(color::Black)
				.Size(16.0f)
				.Align(HorizontalAlign::Center, VerticalAlign::Center)
				.Wrap(WrapMode::Word)
				.Overflow(OverflowMode::Clip);

			button.OnPress([]() { PTGN_LOG("Pressed wrapped text button"); });
		}

		// Ellipsis in a fixed button label box.
		{
			Button button{ CreateTextButton({ -170, -125 }) };

			button.Label()
				.Font("arial")
				.Content("A very long single-line button label that should end with ellipsis")
				.Color(color::Black)
				.Size(18.0f)
				.Align(HorizontalAlign::Left, VerticalAlign::Center)
				.Wrap(WrapMode::None)
				.Overflow(OverflowMode::Ellipsis)
				.MaxLines(1);

			button.OnPress([]() { PTGN_LOG("Pressed ellipsis text button"); });
		}

		// State-specific labels. No base label is created, so the labels do not overlap.
		{
			Button button{
				CreateButton(*this, { 170, -125 }, Rect{ button_size }, Origin::Center)
			};

			ConfigureBasicBackgrounds(button);

			button.SetLabelAutoBox(true, ButtonVisualState::Idle);
			button.SetLabelAutoBox(true, ButtonVisualState::Hover);
			button.SetLabelAutoBox(true, ButtonVisualState::Press);

			button.SetLabelPadding(Rect{ { 14, 8 }, { 14, 8 } }, ButtonVisualState::Idle);
			button.SetLabelPadding(Rect{ { 14, 8 }, { 14, 8 } }, ButtonVisualState::Hover);
			button.SetLabelPadding(Rect{ { 14, 8 }, { 14, 8 } }, ButtonVisualState::Press);

			button.Label(ButtonVisualState::Idle)
				.Font("arial")
				.Content("Idle label")
				.Color(color::Black)
				.Size(22.0f)
				.Align(HorizontalAlign::Center, VerticalAlign::Center);

			button.Label(ButtonVisualState::Hover)
				.Font("arial")
				.Content("Hover label")
				.Color(color::Blue)
				.Size(24.0f)
				.Bold(true, 0.2f)
				.Align(HorizontalAlign::Center, VerticalAlign::Center);

			button.Label(ButtonVisualState::Press)
				.Font("arial")
				.Content("Pressed label")
				.Color(color::Red)
				.Size(21.0f)
				.Align(HorizontalAlign::Center, VerticalAlign::Center)
				.OuterGlow(color::Red.WithAlpha(140), 3.0f, 2.0f);

			button.OnPress([]() { PTGN_LOG("Pressed state-specific text button"); });
		}

		// Disabled-state label. Press Q/E to disable/enable.
		{
			disabled_button = CreateButton(*this, { -170, 0 }, Rect{ button_size }, Origin::Center);

			ConfigureBasicBackgrounds(disabled_button);

			disabled_button.SetLabelAutoBox(true, ButtonVisualState::Idle);
			disabled_button.SetLabelAutoBox(true, ButtonVisualState::Hover);
			disabled_button.SetLabelAutoBox(true, ButtonVisualState::Disabled);

			disabled_button.Label(ButtonVisualState::Idle)
				.Font("arial")
				.Content("Q to Disable")
				.Color(color::Black)
				.Size(23.0f)
				.Align(HorizontalAlign::Center, VerticalAlign::Center);

			disabled_button.Label(ButtonVisualState::Hover)
				.Font("arial")
				.Content("Hover enabled")
				.Color(color::Blue)
				.Size(23.0f)
				.Align(HorizontalAlign::Center, VerticalAlign::Center);

			disabled_button.Label(ButtonVisualState::Disabled)
				.Font("arial")
				.Content("E to Enable")
				.Color(color::White)
				.Size(23.0f)
				.Align(HorizontalAlign::Center, VerticalAlign::Center)
				.Strikethrough();

			disabled_button.OnPress([]() { PTGN_LOG("Pressed disable-demo button"); });
		}

		// Rich text inside a button label.
		{
			Button button{ CreateTextButton({ 170, 0 }) };

			Text label{ button.Label() };

			label.Content("Rich ")
				.Font("arial")
				.Color(color::Black)
				.Size(20.0f)
				.Align(HorizontalAlign::Center, VerticalAlign::Center)
				.Wrap(WrapMode::Word)
				.Overflow(OverflowMode::Clip);

			label.Content("red ").Font("arial").Color(color::Red).Bold(true, 0.18f);

			label.Content("underlined ").Font("arial").Color(color::Blue).Bold(false).Underline();

			label.Content("text").Font("arial").Color(color::Black).Underline(false);

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
	Application app{ "ButtonTextScene: Q/E to disable/enable one button", logical_size };
	PTGN_WITH_EDITOR(app);
	app.StartWith<ButtonTextScene>();
}