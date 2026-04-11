

#include <chrono>
#include <optional>

#include "app/application.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/ui/button.h"

using namespace ptgn;

class ButtonTemplatesScene : public Scene {
public:
	void OnEnter() override {
		ctx().input.SetSettings({ .debug_draw_enabled = true });
		SetBackgroundColor(color::LightGray);

		ctx().asset.LoadMany({ { "hover", "examples/assets/hover.ogg" },
							   { "press", "examples/assets/press.ogg" },
							   { "idle", "examples/assets/big_button_idle.png" },
							   { "hover", "examples/assets/big_button_hover.png" },
							   { "press", "examples/assets/big_button_press.png" },
							   { "bell_idle", "examples/assets/bell.png" },
							   { "bell_hover", "examples/assets/bell_hover_animation.png" },
							   { "bell_press", "examples/assets/bell_press_animation.png" },
							   { "bell_hover", "examples/assets/hover.ogg" },
							   { "bell_press", "examples/assets/bell.ogg" } });

		V2_float size{ 200, 50 };
		V2_float offset{ 0, 70 };

		CreateButton(
			*this, offset * -5, size,
			{
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
			*this, offset * -4, size,
			{
				.content			= "It Takes Two 2",
				.text_color			= color::White,
				.text_color_hover	= color::Brown,
				.text_outline_width = 1,
				.texture_hover		= "hover",
				.sound_hover		= "hover",
				.sound_press		= "press",
				.move				= MoveButtonConfig{},
				.scale				= ScaleButtonConfig{ .scale = 1.1f },
			}
		);

		CreateButton(
			*this, offset * -3, size,
			{ .content		 = "Baba Is You",
			  .text_color	 = color::White,
			  .texture		 = "idle",
			  .texture_hover = "hover",
			  .texture_press = "press",
			  .sound_hover	 = "hover",
			  .sound_press	 = "press" }
		);

		CreateButton(
			*this, offset * -2, size,
			{ .content			  = "Dogs Organized Neatly",
			  .text_color		  = color::Black,
			  .text_color_hover	  = color::White,
			  .texture			  = "idle",
			  .texture_tint_hover = color::Orange,
			  .sound_hover		  = "hover",
			  .sound_press		  = "press" }
		);

		CreateButton(
			*this, offset * -1, size,
			{ .content		 = "Rogue Legacy 2",
			  .text_color	 = color::Gray,
			  .texture_hover = "hover",
			  .texture_press = "hover",
			  .sound_hover	 = "hover",
			  .sound_press	 = "press" }
		);

		CreateButton(
			*this, offset * 0, size,
			{ .content			= "Enter the Gungeon",
			  .text_color		= color::Gray,
			  .text_color_hover = color::White,
			  .sound_hover		= "hover",
			  .sound_press		= "press" }
		);

		CreateButton(
			*this, offset * 1, size,
			{ .content				  = "Payday 2",
			  .text_color			  = color::LightBlue,
			  .text_color_hover		  = color::Blue,
			  .text_outline_width	  = 1,
			  .background_color_hover = color::Blue.WithAlpha(0.1f),
			  .sound_hover			  = "hover",
			  .sound_press			  = "press" }
		);

		CreateButton(
			*this, offset * 2, size,
			{ .content			  = "Terraria",
			  .text_color		  = color::Gray,
			  .text_color_hover	  = color::Gold,
			  .text_outline_width = 1,
			  .sound_hover		  = "hover",
			  .sound_press		  = "press",
			  .scale			  = ScaleButtonConfig{} }
		);

		CreateButton(
			*this, offset * 3, size,
			{ .content			  = "Celeste",
			  .text_color_hover	  = color::Green,
			  .text_outline_width = 1,
			  .sound_hover		  = "hover",
			  .sound_press		  = "press",
			  .move				  = MoveButtonConfig{} }
		);

		CreateButton(
			*this, offset * 4, size,
			{ .content				  = "Golf with Friends",
			  .text_color			  = color::White,
			  .text_color_hover		  = color::Black,
			  .text_outline_width	  = 1,
			  .background_color		  = color::Gray.WithAlpha(0.5f),
			  .background_color_hover = color::Gold.WithAlpha(0.5f),
			  .sound_hover			  = "hover",
			  .sound_press			  = "press" }
		);

		CreateAnimatedButton(
			*this, { 250, 0 }, std::nullopt,
			{
				.texture		 = "bell_idle",
				.texture_hover	 = "bell_hover",
				.texture_press	 = "bell_press",
				.animation_hover = { 3, 400ms, V2_int{ 253, 167 }, std::nullopt },
				.animation_press = AnimationConfig{ 3, 200ms, V2_int{ 253, 167 }, 1 },
				.sound_hover	 = "bell_hover",
				.sound_press	 = "bell_press",
			}
		);
	}
};

int main(int, char**) {
	Application app{ "ButtonTemplatesScene" };
	app.StartWith<ButtonTemplatesScene>();
}