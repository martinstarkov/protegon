#include <chrono>
#include <optional>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"

using namespace ptgn;

class ButtonTemplatesScene : public Scene {
public:
	void OnEnter() override {
		ctx().interaction.SetDebugSettings({ .draw_enabled = true });
		SetBackgroundColor(color::LightGray);

		ctx().asset.Load(
			{
				{ "hover", "assets/hover.ogg" },
				{ "press", "assets/press.ogg" },

				{ "idle", "assets/big_button_idle.png" },
				{ "hover", "assets/big_button_hover.png" },
				{ "press", "assets/big_button_press.png" },

				{ "bell_idle", "assets/bell.png" },
				{ "bell_hover", "assets/bell_hover_animation.png" },
				{ "bell_press", "assets/bell_press_animation.png" },
				{ "bell_hover", "assets/hover.ogg" },
				{ "bell_press", "assets/bell.ogg" },
			}
		);

		V2_float size{ 200, 50 };
		V2_float offset{ 0, 70 };

		CreateButton(
			*this, offset * -5.0f, size,
			ButtonConfig{
				.content			= "It Takes Two 1",
				.text_color			= color::Gold,
				.text_color_hover	= color::Brown,
				.text_outline_width = 1,
				.texture			= "idle",
				.texture_tint_hover = color::Orange,
				.texture_tint_press = color::Blue,
				.sound_hover		= "hover",
				.sound_press		= "press",
			}
		);

		CreateButton(
			*this, offset * -4.0f, size,
			ButtonConfig{
				.content			= "It Takes Two 2",
				.text_color			= color::White,
				.text_color_hover	= color::Brown,
				.text_outline_width = 1,
				.texture_hover		= "hover",
				.sound_hover		= "hover",
				.sound_press		= "press",
				.move				= MoveButtonConfig{ .offset = { 20, 0 }, .duration = 100ms },
				.scale				= ScaleButtonConfig{ .scale = 1.1f, .duration = 100ms },
			}
		);

		CreateButton(
			*this, offset * -3.0f, size,
			ButtonConfig{
				.content	   = "Baba Is You",
				.text_color	   = color::White,
				.texture	   = "idle",
				.texture_hover = "hover",
				.texture_press = "press",
				.sound_hover   = "hover",
				.sound_press   = "press",
			}
		);

		CreateButton(
			*this, offset * -2.0f, size,
			ButtonConfig{
				.content			= "Dogs Organized Neatly",
				.text_color			= color::Black,
				.text_color_hover	= color::White,
				.texture			= "idle",
				.texture_tint_hover = color::Orange,
				.sound_hover		= "hover",
				.sound_press		= "press",
			}
		);

		CreateButton(
			*this, offset * -1.0f, size,
			ButtonConfig{
				.content	   = "Rogue Legacy 2",
				.text_color	   = color::Gray,
				.texture_hover = "hover",
				.texture_press = "hover",
				.sound_hover   = "hover",
				.sound_press   = "press",
			}
		);

		CreateButton(
			*this, offset * 0.0f, size,
			ButtonConfig{
				.content		  = "Enter the Gungeon",
				.text_color		  = color::Gray,
				.text_color_hover = color::White,
				.sound_hover	  = "hover",
				.sound_press	  = "press",
			}
		);

		CreateButton(
			*this, offset * 1.0f, size,
			ButtonConfig{
				.content				= "Payday 2",
				.text_color				= color::LightBlue,
				.text_color_hover		= color::Blue,
				.text_outline_width		= 1,
				.background_color_hover = color::Blue.WithAlpha(0.1f),
				.sound_hover			= "hover",
				.sound_press			= "press",
			}
		);

		CreateButton(
			*this, offset * 2.0f, size,
			ButtonConfig{
				.content			= "Terraria",
				.text_color			= color::Gray,
				.text_color_hover	= color::Gold,
				.text_outline_width = 1,
				.sound_hover		= "hover",
				.sound_press		= "press",
				.scale				= ScaleButtonConfig{ .scale = 1.25f, .duration = 100ms },
			}
		);

		CreateButton(
			*this, offset * 3.0f, size,
			ButtonConfig{
				.content			= "Celeste",
				.text_color_hover	= color::Green,
				.text_outline_width = 1,
				.sound_hover		= "hover",
				.sound_press		= "press",
				.move				= MoveButtonConfig{ .offset = { 20, 0 }, .duration = 100ms },
			}
		);

		CreateButton(
			*this, offset * 4.0f, size,
			ButtonConfig{
				.content				= "Golf with Friends",
				.text_color				= color::White,
				.text_color_hover		= color::Black,
				.text_outline_width		= 1,
				.background_color		= color::Gray.WithAlpha(0.5f),
				.background_color_hover = color::Gold.WithAlpha(0.5f),
				.sound_hover			= "hover",
				.sound_press			= "press",
			}
		);

		Button bell{ CreateAnimatedButton(
			*this, { 250, 0 },
			AnimatedButtonConfig{
				.texture		 = "bell_idle",
				.texture_hover	 = "bell_hover",
				.texture_press	 = "bell_press",
				.animation_hover = AnimationConfig{ .frame_count = 3,
													.duration	 = 400ms,
													.frame_size	 = { 253, 167 },
													.play_count	 = -1 },
				.animation_press = AnimationConfig{ .frame_count = 3,
													.duration	 = 200ms,
													.frame_size	 = { 253, 167 },
													.play_count	 = 1 },
				.sound_hover	 = "bell_hover",
				.sound_press	 = "bell_press",
			}
		) };

		bell.OnPress([]() { PTGN_LOG("Pressed bell!"); });
	}
};

int main(int, char**) {
	Application app{ "ButtonTemplatesScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<ButtonTemplatesScene>();
}