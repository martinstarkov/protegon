

#include <chrono>
#include <optional>
#include <string_view>
#include <vector>

#include "app/application.h"
#include "core/assert.h"
#include "core/math/easing.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "renderer/primitives/color.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/ui/button.h"

using namespace ptgn;

struct MoveButtonConfig {
	std::string_view content;
	Color text_color{ color::White };
	Color text_hover_color{ color::Green };
	Color text_click_color{ text_hover_color };

	FontSize font_size;
	FontOrKey font;

	int text_outline_width{ 1 };
	Color text_outline_color{ color::Black };

	AudioOrKey click{};
	AudioOrKey hover{};

	V2_float move_offset{ 20, 0 };
	milliseconds duration{ 100 };
	Ease ease{ Ease::Linear };
};

static Button CreateMoveButton(
	Scene& scene, V2_float position, V2_float size, const MoveButtonConfig& config = {}
) {
	auto button = CreateButton(scene, size);
	SetPosition(button, position);

	TextProperties text_properties;
	text_properties.outline.width = config.text_outline_width;
	text_properties.outline.color = config.text_outline_color;

	button.SetText(
		config.content, config.text_color, config.font_size, config.font, text_properties,
		ButtonState::Idle
	);
	button.SetText(
		config.content, config.text_hover_color, config.font_size, config.font, text_properties,
		ButtonState::Hover
	);
	button.SetText(
		config.content, config.text_click_color, config.font_size, config.font, text_properties,
		ButtonState::Press
	);

	button.SetSound(config.click, ButtonState::Press);
	button.SetSound(config.hover, ButtonState::Hover);

	button.OnHoverStart([button, config]() {
		using enum ButtonState;
		TranslateTo(
			std::vector<Text>{ *button.GetText(Idle), *button.GetText(Hover),
							   *button.GetText(Press) },
			config.move_offset, config.duration, config.ease
		);
	});

	button.OnHoverStop([button, config]() {
		using enum ButtonState;
		TranslateTo(
			std::vector<Text>{ *button.GetText(Idle), *button.GetText(Hover),
							   *button.GetText(Press) },
			V2_float{}, config.duration, config.ease
		);
	});
	return button;
}

struct ScaleButtonConfig {
	std::string_view content;
	Color text_color{ color::Gray };
	Color text_hover_color{ color::Gold };
	Color text_click_color{ text_hover_color };

	FontSize font_size;
	FontOrKey font;

	int text_outline_width{ 1 };
	Color text_outline_color{ color::Black };

	AudioOrKey click{};
	AudioOrKey hover{};

	float scale{ 1.25f };
	milliseconds duration{ 100 };
	Ease ease{ Ease::Linear };
};

static Button CreateScaleButton(
	Scene& scene, V2_float position, V2_float size, const ScaleButtonConfig& config = {}
) {
	auto button = CreateButton(scene, size);
	SetPosition(button, position);

	TextProperties text_properties;
	text_properties.outline.width = config.text_outline_width;
	text_properties.outline.color = config.text_outline_color;

	button.SetText(
		config.content, config.text_color, config.font_size, config.font, text_properties,
		ButtonState::Idle
	);
	button.SetText(
		config.content, config.text_hover_color, config.font_size, config.font, text_properties,
		ButtonState::Hover
	);
	button.SetText(
		config.content, config.text_click_color, config.font_size, config.font, text_properties,
		ButtonState::Press
	);

	button.SetSound(config.click, ButtonState::Press);
	button.SetSound(config.hover, ButtonState::Hover);

	struct ButtonTextFontSize {};

	auto sf_idle  = button.GetFontSize(ButtonState::Idle);
	auto sf_hover = button.GetFontSize(ButtonState::Hover);
	auto sf_press = button.GetFontSize(ButtonState::Press);

	PTGN_ASSERT(sf_idle.has_value());

	const std::vector<float> start_fonts{ *sf_idle, sf_hover.value_or(*sf_idle),
										  sf_press.value_or(*sf_idle) };

	auto target_fonts{ start_fonts };

	for (auto& target_font : target_fonts) {
		target_font *= config.scale;
	}

	const auto tween_fonts = [button, config](const auto& fonts) {
		using enum ButtonState;
		std::vector<Text> texts{
			*button.GetText(Idle),
			*button.GetText(Hover),
			*button.GetText(Press),
		};

		ScaleTextSize(texts, fonts, config.duration, config.ease);
	};

	button.OnHoverStop([tween_fonts, start_fonts]() { tween_fonts(start_fonts); });
	button.OnHoverStart([tween_fonts, target_fonts]() { tween_fonts(target_fonts); });

	return button;
}

class ButtonTemplatesScene : public Scene {
public:
	void OnEnter() override {
		ctx().input.SetSettings({ .debug_draw_enabled = true });
		SetBackgroundColor(color::LightGray);
		ctx().asset.LoadAudio("hover", "assets/hover.ogg");
		ctx().asset.LoadAudio("click", "assets/click.ogg");

		CreateMoveButton(
			*this, { 0, 300 }, { 150, 50 },
			{ .content			= "Click Me!",
			  .text_hover_color = color::Red,
			  .click			= "click",
			  .hover			= "hover" }
		);

		CreateScaleButton(
			*this, { 0, 200 }, { 150, 50 },
			{ .content			= "Click Me!",
			  .text_hover_color = color::Gold,
			  .click			= "click",
			  .hover			= "hover",
			  .scale			= 1.25f }
		);

		// ctx().asset.Load("idle", "assets/bell.png");
		// ctx().asset.Load("animation_hover", "assets/bell_hover_animation.png");
		// ctx().asset.Load("animation_activate", "assets/bell_click_animation.png");
		// ctx().asset.LoadAudio("hover", "assets/hover.ogg");
		// ctx().asset.LoadAudio("click", "assets/bell.ogg");
		// auto hover_animation{ CreateAnimation(
		//	*this, "animation_hover", V2_int{}, { 3, milliseconds{ 400 }, V2_int{ 253, 167 }, -1 }
		//) };
		// auto activate_animation{ CreateAnimation(
		//	*this, "animation_activate", V2_int{}, { 3, milliseconds{ 200 }, V2_int{ 253, 167 }, 1 }
		//) };
		// b1 = CreateButton(*this, *GetDisplaySize(activate_animation));
		// b1.SetTexture("idle")
		//	.SetAnimation(std::move(hover_animation), ButtonState::Hover)
		//	.SetAnimation(std::move(activate_animation), ButtonState::Press)
		//	.SetSound("hover", ButtonState::Hover)
		//	.SetSound("click", ButtonState::Press);
		// SetScale(b1, 1.0f);
		// b1.OnActivate([]() { PTGN_LOG("Clicked bell!"); });
	}
};

int main(int, char**) {
	Application app{ "ButtonTemplatesScene" };
	app.StartWith<ButtonTemplatesScene>();
}