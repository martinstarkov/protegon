#include <chrono>
#include <optional>

#include "app/application.h"
#include "app/editor.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"

using namespace ptgn;

class AnimatedButtonScene : public Scene {
public:
	void OnEnter() override {
		ctx().debug.settings.interaction.draw_enabled = true;

		ctx().asset.Load(
			{ { "idle", "assets/bell.png" },
			  { "animation_hover", "assets/bell_hover_animation.png" },
			  { "animation_press", "assets/bell_press_animation.png" },
			  { "hover", "assets/hover.ogg" },
			  { "press", "assets/bell.ogg" },
			  { "idle2", "assets/button_idle.png" },
			  { "animation_hover2", "assets/button_hover_animation.png" },
			  { "animation_press2", "assets/button_press_animation.png" },
			  { "press2", "assets/press.ogg" } }
		);

		{
			V2_int frame_size{ 253, 167 };

			auto button{ CreateButton(*this, {}, frame_size, Origin::TopLeft) };

			button.Sprites("idle", "animation_hover", "animation_press")
				.Sounds("press", "hover")
				.OnPress([]() { PTGN_LOG("Pressed bell!"); })
				.Animations(
					std::nullopt,
					AnimationConfig{
						.frame_count = 3,
						.duration	 = 400ms,
						.frame_size	 = frame_size,
						.play_count	 = std::nullopt,
					},
					AnimationConfig{
						.frame_count = 3,
						.duration	 = 200ms,
						.frame_size	 = frame_size,
						.play_count	 = 1,
					}
				);

			SetScale(button, 1);
		}

		{
			V2_int frame_size{ 32, 16 };

			auto button{ CreateButton(*this, { 0, 200 }, frame_size, Origin::TopLeft) };

			button.Sprites("idle2", "animation_hover2", "animation_press2")
				.Sounds("press2", "hover")
				.OnPress([]() { PTGN_LOG("Pressed button!"); })
				.Animations(
					std::nullopt,
					AnimationConfig{
						.frame_count = 4,
						.duration	 = 400ms,
						.frame_size	 = frame_size,
						.play_count	 = std::nullopt,
					},
					AnimationConfig{
						.frame_count = 4,
						.duration	 = 200ms,
						.frame_size	 = frame_size,
						.play_count	 = 1,
					}
				);

			SetScale(button, 4);
		}
	}
};

int main(int, char**) {
	Application app{ "AnimatedButtonScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<AnimatedButtonScene>();
}