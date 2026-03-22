

#include <optional>
#include <string_view>

#include "app/application.h"
#include "app/context.h"
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
	milliseconds move_duration{ 100 };
	Ease move_ease{ Ease::Linear };
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
			*button.GetText(ButtonState::Idle), config.move_offset, config.move_duration,
			config.move_ease, false
		);
		TranslateTo(
			*button.GetText(ButtonState::Hover), config.move_offset, config.move_duration,
			config.move_ease, false
		);
		TranslateTo(
			*button.GetText(ButtonState::Press), config.move_offset, config.move_duration,
			config.move_ease, false
		);
	});

	button.OnHoverStop([button, config]() {
		TranslateTo(
			*button.GetText(ButtonState::Idle), {}, config.move_duration, config.move_ease, true
		);
		TranslateTo(
			*button.GetText(ButtonState::Hover), {}, config.move_duration, config.move_ease, true
		);
		TranslateTo(
			*button.GetText(ButtonState::Press), {}, config.move_duration, config.move_ease, true
		);
	});
	return button;
}

class ButtonTemplatesScene : public Scene {
public:
	void OnEnter() override {
		input.SetSettings({ .debug_draw_enabled = true });
		SetBackgroundColor(color::LightGray);
		app().asset.LoadAudio("hover", "assets/hover.ogg");
		app().asset.LoadAudio("click", "assets/click.ogg");

		CreateMoveButton(
			*this, { 0, 300 }, { 150, 50 },
			{ .content			= "Click Me!",
			  .text_hover_color = color::Red,
			  .click			= "click",
			  .hover			= "hover" }
		);

		// app().asset.Load("idle", "assets/bell.png");
		// app().asset.Load("animation_hover", "assets/bell_hover_animation.png");
		// app().asset.Load("animation_activate", "assets/bell_click_animation.png");
		// app().asset.LoadAudio("hover", "assets/hover.ogg");
		// app().asset.LoadAudio("click", "assets/bell.ogg");
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