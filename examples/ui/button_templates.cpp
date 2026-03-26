

#include <chrono>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "app/application.h"
#include "core/assert.h"
#include "core/math/easing.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "renderer/primitives/color.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/ui/button.h"

using namespace ptgn;

struct MoveButtonConfig {
	V2_float offset{ 20, 0 };
	milliseconds duration{ 100 };
	Ease ease{ Ease::Linear };
};

struct ScaleButtonConfig {
	float scale{ 1.25f };
	milliseconds duration{ 100 };
	Ease ease{ Ease::Linear };
};

struct TextButtonConfig {
	std::optional<std::string_view> content;
	std::optional<Color> text_color{ color::White };
	std::optional<Color> text_color_hover;
	std::optional<Color> text_color_press;
	std::optional<int> text_outline_width;
	std::optional<Color> text_outline_color;

	FontSize font_size;
	FontOrKey font;

	std::optional<TextureOrKey> texture;
	std::optional<TextureOrKey> texture_hover;
	std::optional<TextureOrKey> texture_press;

	std::optional<Color> texture_tint;
	std::optional<Color> texture_tint_hover;
	std::optional<Color> texture_tint_press;

	std::optional<Color> background_color;
	std::optional<Color> background_color_hover;
	std::optional<Color> background_color_press;

	std::optional<V2_float> background_size;

	std::optional<AudioOrKey> sound_press;
	std::optional<AudioOrKey> sound_hover;

	std::optional<MoveButtonConfig> move;
	std::optional<ScaleButtonConfig> scale;
};

static Button CreateTextButton(
	Scene& scene, V2_float position, V2_float size, const TextButtonConfig& config = {}
) {
	auto button = CreateButton(scene, size);
	SetPosition(button, position);

	std::optional<std::variant<Rect, Circle>> shape;

	if (config.background_size.has_value()) {
		shape = Rect{ *config.background_size };
	}

	button.SetBackgroundShape(shape, ButtonState::Idle);

	if (config.background_color.has_value()) {
		button.SetBackgroundColor(*config.background_color, ButtonState::Idle);
	}
	if (config.background_color_hover.has_value()) {
		button.SetBackgroundColor(*config.background_color_hover, ButtonState::Hover);
	}

	if (config.background_color_press.has_value() || config.background_color_hover.has_value()) {
		Color bg_color{ config.background_color_press
							.or_else([&config]() { return config.background_color_hover; })
							.value() };
		button.SetBackgroundColor(bg_color, ButtonState::Press);
	}

	if (config.texture.has_value()) {
		button.SetTexture(*config.texture, ButtonState::Idle);
	}
	if (config.texture_hover.has_value()) {
		button.SetTexture(*config.texture_hover, ButtonState::Hover);
	}
	if (config.texture_press.has_value()) {
		button.SetTexture(*config.texture_press, ButtonState::Press);
	}

	if (config.texture_tint.has_value()) {
		button.SetTextureTint(*config.texture_tint, ButtonState::Idle);
	}

	if (config.texture_tint_hover.has_value()) {
		button.SetTextureTint(*config.texture_tint_hover, ButtonState::Hover);
	}

	if (config.texture_tint_press.has_value() || config.texture_tint_hover.has_value()) {
		Color tint{
			config.texture_tint_press.or_else([&config]() { return config.texture_tint_hover; }
			).value()
		};
		button.SetTextureTint(tint, ButtonState::Press);
	}

	if (config.content.has_value()) {
		TextProperties text_properties;
		if (config.text_outline_width.has_value()) {
			text_properties.outline.width = *config.text_outline_width;
		}
		if (config.text_outline_color.has_value()) {
			text_properties.outline.color = *config.text_outline_color;
		}
		auto idle_color{ config.text_color.value_or(impl::kDefaultButtonTextColor) };
		button.SetText(
			*config.content, idle_color, config.font_size, config.font, text_properties,
			ButtonState::Idle
		);
		auto hover_color{ config.text_color_hover.value_or(idle_color) };
		button.SetText(
			*config.content, hover_color, config.font_size, config.font, text_properties,
			ButtonState::Hover
		);
		button.SetText(
			*config.content, config.text_color_press.value_or(hover_color), config.font_size,
			config.font, text_properties, ButtonState::Press
		);
	}

	button.SetSound(config.sound_hover, ButtonState::Hover);
	button.SetSound(config.sound_press, ButtonState::Press);

	if (config.move.has_value()) {
		button.OnHoverStart([button, config = *config.move]() {
			using enum ButtonState;
			TranslateTo(
				std::vector<Text>{ *button.GetText(Idle), *button.GetText(Hover),
								   *button.GetText(Press) },
				config.offset, config.duration, config.ease
			);
		});

		button.OnHoverStop([button, config = *config.move]() {
			using enum ButtonState;
			TranslateTo(
				std::vector<Text>{ *button.GetText(Idle), *button.GetText(Hover),
								   *button.GetText(Press) },
				V2_float{}, config.duration, config.ease
			);
		});
	}

	if (config.scale.has_value()) {
		auto sf_idle  = button.GetFontSize(ButtonState::Idle);
		auto sf_hover = button.GetFontSize(ButtonState::Hover);
		auto sf_press = button.GetFontSize(ButtonState::Press);

		PTGN_ASSERT(sf_idle.has_value());

		const std::vector<float> start_fonts{ *sf_idle, sf_hover.value_or(*sf_idle),
											  sf_press.value_or(*sf_idle) };

		auto target_fonts{ start_fonts };

		for (auto& target_font : target_fonts) {
			target_font *= config.scale->scale;
		}

		const auto tween_fonts = [button, config = *config.scale](const auto& fonts) {
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
	}

	return button;
}

class ButtonTemplatesScene : public Scene {
public:
	void OnEnter() override {
		ctx().input.SetSettings({ .debug_draw_enabled = true });
		SetBackgroundColor(color::LightGray);
		ctx().asset.Load("hover", "assets/hover.ogg");
		ctx().asset.Load("press", "assets/press.ogg");
		ctx().asset.Load("idle", "assets/big_button_idle.png");
		ctx().asset.Load("hover", "assets/big_button_hover.png");
		ctx().asset.Load("press", "assets/big_button_press.png");

		V2_float size{ 200, 50 };
		V2_float offset{ 0, 70 };

		CreateTextButton(
			*this, offset * 3, size,
			{ .content			  = "Celeste",
			  .text_color_hover	  = color::Green,
			  .text_outline_width = 1,
			  .sound_press		  = "press",
			  .sound_hover		  = "hover",
			  .move				  = MoveButtonConfig{} }
		);

		CreateTextButton(
			*this, offset * 2, size,
			{ .content			  = "Terraria",
			  .text_color		  = color::Gray,
			  .text_color_hover	  = color::Gold,
			  .text_outline_width = 1,
			  .sound_press		  = "press",
			  .sound_hover		  = "hover",
			  .scale			  = ScaleButtonConfig{} }
		);

		CreateTextButton(
			*this, offset * 1, size,
			{ .content				  = "Payday 2",
			  .text_color			  = color::LightBlue,
			  .text_color_hover		  = color::Blue,
			  .text_outline_width	  = 1,
			  .background_color_hover = color::Blue.WithAlpha(0.1f),
			  .sound_press			  = "press",
			  .sound_hover			  = "hover" }
		);

		CreateTextButton(
			*this, offset * 0, size,
			{ .content			= "Enter the Gungeon",
			  .text_color		= color::Gray,
			  .text_color_hover = color::White,
			  .sound_press		= "press",
			  .sound_hover		= "hover" }
		);

		CreateTextButton(
			*this, offset * -1, size,
			{ .content		 = "Rogue Legacy 2",
			  .text_color	 = color::Gray,
			  .texture_hover = "hover",
			  .texture_press = "hover",
			  .sound_press	 = "press",
			  .sound_hover	 = "hover" }
		);

		CreateTextButton(
			*this, offset * -2, size,
			{ .content			  = "Dogs Organized Neatly",
			  .text_color		  = color::Black,
			  .text_color_hover	  = color::White,
			  .texture			  = "idle",
			  .texture_tint_hover = color::Orange,
			  .sound_press		  = "press",
			  .sound_hover		  = "hover" }
		);

		// ctx().asset.Load("idle", "assets/bell.png");
		// ctx().asset.Load("animation_hover", "assets/bell_hover_animation.png");
		// ctx().asset.Load("animation_press", "assets/bell_press_animation.png");
		// ctx().asset.LoadAudio("hover", "assets/hover.ogg");
		// ctx().asset.LoadAudio("press", "assets/bell.ogg");
		// auto hover_animation{ CreateAnimation(
		//	*this, "animation_hover", V2_int{}, { 3, milliseconds{ 400 }, V2_int{ 253, 167 }, -1 }
		//) };
		// auto press_animation{ CreateAnimation(
		//	*this, "animation_press", V2_int{}, { 3, milliseconds{ 200 }, V2_int{ 253, 167 }, 1 }
		//) };
		// b1 = CreateButton(*this, *GetDisplaySize(press_animation));
		// b1.SetTexture("idle")
		//	.SetAnimation(std::move(hover_animation), ButtonState::Hover)
		//	.SetAnimation(std::move(press_animation), ButtonState::Press)
		//	.SetSound("hover", ButtonState::Hover)
		//	.SetSound("press", ButtonState::Press);
		// SetScale(b1, 1.0f);
		// b1.OnPress([]() { PTGN_LOG("Pressed bell!"); });
	}
};

int main(int, char**) {
	Application app{ "ButtonTemplatesScene" };
	app.StartWith<ButtonTemplatesScene>();
}