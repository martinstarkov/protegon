#include <chrono>
#include <optional>
#include <utility>

#include "app/application.h"
#include "core/editor.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/sprite.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"

using namespace ptgn;

class AnimatedButtonScene : public Scene {
public:
	Button b1;
	Button b2;

	void OnEnter() override {
		ctx().interaction.SetDebugSettings({ .draw_enabled = true });

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

		auto hover_animation{ CreateAnimation(
			*this, {}, "animation_hover",
			AnimationConfig{
				.frame_count		= 3,
				.animation_duration = 400ms,
				.frame_size			= { 253, 167 },
				.play_count			= std::nullopt,
			}
		) };

		auto press_animation{ CreateAnimation(
			*this, {}, "animation_press",
			AnimationConfig{
				.frame_count		= 3,
				.animation_duration = 200ms,
				.frame_size			= { 253, 167 },
				.play_count			= 1,
			}
		) };

		V2_float b1_size{ GetDisplaySize(press_animation).value() };

		b1 = CreateButton(*this, {}, b1_size, Origin::TopLeft);

		b1.Sprite("idle", {}, ButtonVisualState::Idle);

		b1.Animation(hover_animation, ButtonVisualState::Hover)
			.Animation(press_animation, ButtonVisualState::Press)
			.Sounds("press", "hover");

		SetScale(b1, 1);

		b1.OnPress([]() { PTGN_LOG("Pressed bell!"); });

		auto hover_animation2{ CreateAnimation(
			*this, {}, "animation_hover2",
			AnimationConfig{
				.frame_count		= 4,
				.animation_duration = 400ms,
				.frame_size			= { 32, 16 },
				.play_count			= std::nullopt,
			}
		) };

		auto press_animation2{ CreateAnimation(
			*this, {}, "animation_press2",
			AnimationConfig{
				.frame_count		= 4,
				.animation_duration = 200ms,
				.frame_size			= { 32, 16 },
				.play_count			= 1,
			}
		) };

		V2_float b2_size{ GetDisplaySize(press_animation2).value() };

		b2 = CreateButton(*this, { 0, 200 }, b2_size, Origin::TopLeft);

		b2.Sprite("idle2", {}, ButtonVisualState::Idle);

		b2.Animation(hover_animation2, ButtonVisualState::Hover)
			.Animation(press_animation2, ButtonVisualState::Press)
			.Sounds("press2", "hover");

		SetScale(b2, 4);

		b2.OnPress([]() { PTGN_LOG("Pressed button!"); });
	}
};

int main(int, char**) {
	Application app{ "AnimatedButtonScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<AnimatedButtonScene>();
}