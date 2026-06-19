#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/text/text_style.h"
#include "runtime/graphics/tint.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"

using namespace ptgn;

class ButtonTemplatesScene : public Scene {
public:
	struct ButtonTemplateSpec {
		std::string_view content;

		Color text_color{ color::White };
		std::optional<Color> text_color_hover;
		std::optional<Color> text_color_press;
		std::optional<float> text_outline_width;
		Color text_outline_color{ color::Black };

		std::optional<std::string_view> texture;
		std::optional<std::string_view> texture_hover;
		std::optional<std::string_view> texture_press;

		std::optional<Color> texture_tint;
		std::optional<Color> texture_tint_hover;
		std::optional<Color> texture_tint_press;

		std::optional<Color> background_color;
		std::optional<Color> background_color_hover;
		std::optional<Color> background_color_press;

		std::optional<std::string_view> sound_hover;
		std::optional<std::string_view> sound_press;

		std::optional<V2_float> hover_move;
		std::optional<float> hover_scale;
	};

	static void ConfigureShapePart(Entity entity, V2_float size, Color tint, FillStyle fill_style) {
		entity.Add<Rect>(Rect{ size });
		SetTint(entity, tint);
		SetDraw<RectDraw>(entity);
		SetDrawOrigin(entity, Origin::Center);
		SetFillStyle(entity, fill_style);
	}

	static void ConfigureBackground(
		Button button, ButtonVisualState state, V2_float size, Color tint
	) {
		ConfigureShapePart(button.Background(state), size, tint, Solid{});
	}

	static void ConfigureLabel(
		Button button, ButtonVisualState state, std::string_view content, Color tint,
		V2_float padding_size, std::optional<float> outline_width = std::nullopt,
		Color outline_color = color::Black
	) {
		Text label{ button.Label(state) };

		label.Font("arial")
			.Content(content)
			.Color(tint)
			.Size(20.0f)
			.Align(HorizontalAlign::Center, VerticalAlign::Center)
			.Wrap(WrapMode::None)
			.Overflow(OverflowMode::Ellipsis)
			.MaxLines(1);

		if (outline_width.has_value()) {
			label.Outline(outline_color, outline_width.value());
		}

		button.SetLabelPadding(Rect{ padding_size, padding_size }, state);
	}

	static void ConfigureIcon(
		Button button, ButtonVisualState state, std::string_view texture_key,
		std::optional<Color> tint = std::nullopt
	) {
		Sprite icon{ button.Icon(state) };
		icon.SetTexture(texture_key);

		if (tint.has_value()) {
			SetTint(icon, tint.value());
		}
	}

	Button CreateTemplateButton(V2_float position, V2_float size, const ButtonTemplateSpec& spec) {
		Button button{ CreateButton(*this, position, Rect{ size }, Origin::Center) };

		bool has_texture{ spec.texture.has_value() || spec.texture_hover.has_value() ||
						  spec.texture_press.has_value() };

		if (spec.background_color.has_value()) {
			ConfigureBackground(
				button, ButtonVisualState::Idle, size, spec.background_color.value()
			);
		} else if (!has_texture) {
			ConfigureBackground(
				button, ButtonVisualState::Idle, size, color::Gray.WithAlpha(0.35f)
			);
		}

		if (spec.background_color_hover.has_value()) {
			ConfigureBackground(
				button, ButtonVisualState::Hover, size, spec.background_color_hover.value()
			);
		}

		if (spec.background_color_press.has_value()) {
			ConfigureBackground(
				button, ButtonVisualState::Press, size, spec.background_color_press.value()
			);
		}

		if (spec.texture.has_value()) {
			ConfigureIcon(button, ButtonVisualState::Idle, spec.texture.value(), spec.texture_tint);
		}

		if (spec.texture_hover.has_value()) {
			ConfigureIcon(
				button, ButtonVisualState::Hover, spec.texture_hover.value(),
				spec.texture_tint_hover
			);
		} else if (spec.texture_tint_hover.has_value() && spec.texture.has_value()) {
			ConfigureIcon(
				button, ButtonVisualState::Hover, spec.texture.value(), spec.texture_tint_hover
			);
		}

		if (spec.texture_press.has_value()) {
			ConfigureIcon(
				button, ButtonVisualState::Press, spec.texture_press.value(),
				spec.texture_tint_press
			);
		} else if (spec.texture_tint_press.has_value()) {
			auto texture_key{ spec.texture_hover.has_value() ? spec.texture_hover.value()
															 : spec.texture.value_or({}) };

			if (!texture_key.empty()) {
				ConfigureIcon(
					button, ButtonVisualState::Press, texture_key, spec.texture_tint_press
				);
			}
		}

		V2_float label_padding{ 8.0f, 4.0f };

		ConfigureLabel(
			button, ButtonVisualState::Idle, spec.content, spec.text_color, label_padding,
			spec.text_outline_width, spec.text_outline_color
		);

		if (spec.text_color_hover.has_value()) {
			ConfigureLabel(
				button, ButtonVisualState::Hover, spec.content, spec.text_color_hover.value(),
				label_padding, spec.text_outline_width, spec.text_outline_color
			);
		}

		if (spec.text_color_press.has_value()) {
			ConfigureLabel(
				button, ButtonVisualState::Press, spec.content, spec.text_color_press.value(),
				label_padding, spec.text_outline_width, spec.text_outline_color
			);
		}

		button.SetSound(spec.sound_hover, ButtonState::Hover);
		button.SetSound(spec.sound_press, ButtonState::Press);

		if (spec.hover_move.has_value()) {
			button.OnHoverStart([button, position, offset = spec.hover_move.value()]() mutable {
				SetPosition(button, position + offset);
			});

			button.OnHoverStop([button, position]() mutable { SetPosition(button, position); });
		}

		if (spec.hover_scale.has_value()) {
			button.OnHoverStart([button, scale = spec.hover_scale.value()]() mutable {
				SetScale(button, scale);
			});

			button.OnHoverStop([button]() mutable { SetScale(button, 1.0f); });
		}

		button.RefreshVisualState();

		return button;
	}

	void OnEnter() override {
		ctx().interaction.SetDebugSettings({ .draw_enabled = true });
		SetBackgroundColor(color::LightGray);

		ctx().asset.Load("arial", "assets/Arial.ttf");

		ctx().asset.LoadAudio("sound_hover", "assets/hover.ogg");
		ctx().asset.LoadAudio("sound_press", "assets/press.ogg");

		ctx().asset.Load("big_idle", "assets/big_button_idle.png");
		ctx().asset.Load("big_hover", "assets/big_button_hover.png");
		ctx().asset.Load("big_press", "assets/big_button_press.png");

		ctx().asset.Load("bell_idle", "assets/bell.png");
		ctx().asset.Load("bell_hover", "assets/bell_hover_animation.png");
		ctx().asset.Load("bell_press", "assets/bell_press_animation.png");
		ctx().asset.LoadAudio("bell_hover_sound", "assets/hover.ogg");
		ctx().asset.LoadAudio("bell_press_sound", "assets/bell.ogg");

		V2_float size{ 200.0f, 50.0f };
		V2_float offset{ 0.0f, 70.0f };

		CreateTemplateButton(
			offset * -5.0f, size,
			ButtonTemplateSpec{
				.content			= "It Takes Two 1",
				.text_color			= color::Gold,
				.text_color_hover	= color::Brown,
				.text_outline_width = 1.0f,
				.texture			= "big_idle",
				.texture_tint_hover = color::Orange,
				.texture_tint_press = color::Blue,
				.sound_hover		= "sound_hover",
				.sound_press		= "sound_press",
			}
		);

		CreateTemplateButton(
			offset * -4.0f, size,
			ButtonTemplateSpec{
				.content			= "It Takes Two 2",
				.text_color			= color::White,
				.text_color_hover	= color::Brown,
				.text_outline_width = 1.0f,
				.texture_hover		= "big_hover",
				.sound_hover		= "sound_hover",
				.sound_press		= "sound_press",
				.hover_move			= V2_float{ 20.0f, 0.0f },
				.hover_scale		= 1.1f,
			}
		);

		CreateTemplateButton(
			offset * -3.0f, size,
			ButtonTemplateSpec{
				.content	   = "Baba Is You",
				.text_color	   = color::White,
				.texture	   = "big_idle",
				.texture_hover = "big_hover",
				.texture_press = "big_press",
				.sound_hover   = "sound_hover",
				.sound_press   = "sound_press",
			}
		);

		CreateTemplateButton(
			offset * -2.0f, size,
			ButtonTemplateSpec{
				.content			= "Dogs Organized Neatly",
				.text_color			= color::Black,
				.text_color_hover	= color::White,
				.texture			= "big_idle",
				.texture_tint_hover = color::Orange,
				.sound_hover		= "sound_hover",
				.sound_press		= "sound_press",
			}
		);

		CreateTemplateButton(
			offset * -1.0f, size,
			ButtonTemplateSpec{
				.content	   = "Rogue Legacy 2",
				.text_color	   = color::Gray,
				.texture_hover = "big_hover",
				.texture_press = "big_hover",
				.sound_hover   = "sound_hover",
				.sound_press   = "sound_press",
			}
		);

		CreateTemplateButton(
			offset * 0.0f, size,
			ButtonTemplateSpec{
				.content		  = "Enter the Gungeon",
				.text_color		  = color::Gray,
				.text_color_hover = color::White,
				.sound_hover	  = "sound_hover",
				.sound_press	  = "sound_press",
			}
		);

		CreateTemplateButton(
			offset * 1.0f, size,
			ButtonTemplateSpec{
				.content				= "Payday 2",
				.text_color				= color::LightBlue,
				.text_color_hover		= color::Blue,
				.text_outline_width		= 1.0f,
				.background_color_hover = color::Blue.WithAlpha(0.1f),
				.sound_hover			= "sound_hover",
				.sound_press			= "sound_press",
			}
		);

		CreateTemplateButton(
			offset * 2.0f, size,
			ButtonTemplateSpec{
				.content			= "Terraria",
				.text_color			= color::Gray,
				.text_color_hover	= color::Gold,
				.text_outline_width = 1.0f,
				.sound_hover		= "sound_hover",
				.sound_press		= "sound_press",
				.hover_scale		= 1.25f,
			}
		);

		CreateTemplateButton(
			offset * 3.0f, size,
			ButtonTemplateSpec{
				.content			= "Celeste",
				.text_color			= color::White,
				.text_color_hover	= color::Green,
				.text_outline_width = 1.0f,
				.sound_hover		= "sound_hover",
				.sound_press		= "sound_press",
				.hover_move			= V2_float{ 20.0f, 0.0f },
			}
		);

		CreateTemplateButton(
			offset * 4.0f, size,
			ButtonTemplateSpec{
				.content				= "Golf with Friends",
				.text_color				= color::White,
				.text_color_hover		= color::Black,
				.text_outline_width		= 1.0f,
				.background_color		= color::Gray.WithAlpha(0.5f),
				.background_color_hover = color::Gold.WithAlpha(0.5f),
				.sound_hover			= "sound_hover",
				.sound_press			= "sound_press",
			}
		);

		auto bell_hover_animation{ CreateAnimation(
			*this, {}, "bell_hover",
			AnimationConfig{
				.frame_count		= 3,
				.animation_duration = 400ms,
				.frame_size			= { 253, 167 },
				.play_count			= std::nullopt,
			}
		) };

		auto bell_press_animation{ CreateAnimation(
			*this, {}, "bell_press",
			AnimationConfig{
				.frame_count		= 3,
				.animation_duration = 200ms,
				.frame_size			= { 253, 167 },
				.play_count			= 1,
			}
		) };

		V2_float bell_size{ GetDisplaySize(bell_press_animation).value() };

		Button bell{ CreateButton(*this, { 250, 0 }, Rect{ bell_size }, Origin::Center) };

		bell.Icon(ButtonVisualState::Idle).SetTexture("bell_idle");

		bell.SetAnimation(std::move(bell_hover_animation), ButtonVisualState::Hover)
			.SetAnimation(std::move(bell_press_animation), ButtonVisualState::Press)
			.SetSound("bell_hover_sound", ButtonState::Hover)
			.SetSound("bell_press_sound", ButtonState::Press);

		bell.OnPress([]() { PTGN_LOG("Pressed bell!"); });

		bell.RefreshVisualState();
	}
};

int main(int, char**) {
	Application app{ "ButtonTemplatesScene" };
	app.StartWith<ButtonTemplatesScene>();
}