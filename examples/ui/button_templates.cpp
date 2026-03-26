

#include <optional>
#include <string_view>

#include "app/application.h"
#include "core/math/easing.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "renderer/primitives/color.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset.h"
#include "runtime/asset/asset_manager.h"
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
		TranslateTo(
			*button.GetText(ButtonState::Idle), config.move_offset, config.duration, config.ease,
			false
		);
		TranslateTo(
			*button.GetText(ButtonState::Hover), config.move_offset, config.duration, config.ease,
			false
		);
		TranslateTo(
			*button.GetText(ButtonState::Press), config.move_offset, config.duration, config.ease,
			false
		);
	});

	button.OnHoverStop([button, config]() {
		TranslateTo(*button.GetText(ButtonState::Idle), {}, config.duration, config.ease, true);
		TranslateTo(*button.GetText(ButtonState::Hover), {}, config.duration, config.ease, true);
		TranslateTo(*button.GetText(ButtonState::Press), {}, config.duration, config.ease, true);
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

	V2_float scale{ 1.25f };
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

	button.OnHoverStart([button, config, starting_font_idle = button.GetFontSize(ButtonState::Idle),
						 starting_font_hover = button.GetFontSize(ButtonState::Hover),
						 starting_font_press = button.GetFontSize(ButtonState::Press)]() {
		impl::AddTweenEffect<impl::ScaleEffect, V2_float>(
			*button.GetText(ButtonState::Idle),
			V2_float{ starting_font_idle->GetValue() } * config.scale, config.duration, config.ease,
			false,
			[](Entity e) {
				return V2_float{ Button(GetParent(e)).GetFontSize(ButtonState::Idle)->GetValue() };
			},
			[](Entity e, V2_float v) { Button(GetParent(e)).SetFontSize(v.x, ButtonState::Idle); }
		);
		impl::AddTweenEffect<impl::ScaleEffect, V2_float>(
			*button.GetText(ButtonState::Hover),
			V2_float{ starting_font_hover.value_or(*starting_font_idle).GetValue() } * config.scale,
			config.duration, config.ease, false,
			[](Entity e) {
				return V2_float{ Button(GetParent(e)).GetFontSize(ButtonState::Hover)->GetValue() };
			},
			[](Entity e, V2_float v) { Button(GetParent(e)).SetFontSize(v.x, ButtonState::Hover); }
		);
		impl::AddTweenEffect<impl::ScaleEffect, V2_float>(
			*button.GetText(ButtonState::Press),
			V2_float{ starting_font_press.value_or(*starting_font_idle).GetValue() } * config.scale,
			config.duration, config.ease, false,
			[](Entity e) {
				return V2_float{ Button(GetParent(e)).GetFontSize(ButtonState::Press)->GetValue() };
			},
			[](Entity e, V2_float v) { Button(GetParent(e)).SetFontSize(v.x, ButtonState::Press); }
		);
	});

	button.OnHoverStop([button, config, starting_font_idle = button.GetFontSize(ButtonState::Idle),
						starting_font_hover = button.GetFontSize(ButtonState::Hover),
						starting_font_press = button.GetFontSize(ButtonState::Press)]() {
		PTGN_ASSERT(starting_font_idle.has_value());
		impl::AddTweenEffect<impl::ScaleEffect, V2_float>(
			*button.GetText(ButtonState::Idle), V2_float{ starting_font_idle->GetValue() },
			config.duration, config.ease, true,
			[](Entity e) {
				return V2_float{ Button(GetParent(e)).GetFontSize(ButtonState::Idle)->GetValue() };
			},
			[](Entity e, V2_float v) { Button(GetParent(e)).SetFontSize(v.x, ButtonState::Idle); }
		);
		impl::AddTweenEffect<impl::ScaleEffect, V2_float>(
			*button.GetText(ButtonState::Hover),
			V2_float{ starting_font_hover.value_or(*starting_font_idle).GetValue() },
			config.duration, config.ease, true,
			[](Entity e) {
				return V2_float{ Button(GetParent(e)).GetFontSize(ButtonState::Hover)->GetValue() };
			},
			[](Entity e, V2_float v) { Button(GetParent(e)).SetFontSize(v.x, ButtonState::Hover); }
		);
		impl::AddTweenEffect<impl::ScaleEffect, V2_float>(
			*button.GetText(ButtonState::Press),
			V2_float{ starting_font_press.value_or(*starting_font_idle).GetValue() },
			config.duration, config.ease, true,
			[](Entity e) {
				return V2_float{ Button(GetParent(e)).GetFontSize(ButtonState::Press)->GetValue() };
			},
			[](Entity e, V2_float v) { Button(GetParent(e)).SetFontSize(v.x, ButtonState::Press); }
		);
	});
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
			  .scale			= V2_float{ 1.25f } }
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