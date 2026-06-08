#include <chrono>
#include <optional>
#include <utility>

#include "app/application.h"
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

using namespace ptgn;

class AnimatedButtonScene : public Scene {
public:
	Button b1;
	Button b2;

	void OnEnter() override {
		ctx().interaction.SetDebugSettings({ .draw_enabled = true });

		ctx().asset.Load("idle", "assets/bell.png");
		ctx().asset.Load("animation_hover", "assets/bell_hover_animation.png");
		ctx().asset.Load("animation_press", "assets/bell_press_animation.png");
		ctx().asset.LoadAudio("hover", "assets/hover.ogg");
		ctx().asset.LoadAudio("press", "assets/bell.ogg");

		ctx().asset.Load("idle2", "assets/button_idle.png");
		ctx().asset.Load("animation_hover2", "assets/button_hover_animation.png");
		ctx().asset.Load("animation_press2", "assets/button_press_animation.png");
		ctx().asset.LoadAudio("press2", "assets/press.ogg");

		auto hover_animation{ CreateAnimation(
			*this, "animation_hover", {},
			AnimationConfig{
				.frame_count		= 3,
				.animation_duration = 400ms,
				.frame_size			= V2_int{ 253, 167 },
				.play_count			= std::nullopt,
			}
		) };

		auto press_animation{ CreateAnimation(
			*this, "animation_press", {},
			AnimationConfig{
				.frame_count		= 3,
				.animation_duration = 200ms,
				.frame_size			= V2_int{ 253, 167 },
				.play_count			= 1,
			}
		) };

		auto b1_size{ V2_float{ *GetDisplaySize(press_animation) } };

		b1 = CreateButton(*this, {}, Rect{ b1_size }, Origin::Center);

		b1.Icon(ButtonVisualState::Idle).SetTexture("idle");

		b1.SetAnimation(std::move(hover_animation), ButtonVisualState::Hover)
			.SetAnimation(std::move(press_animation), ButtonVisualState::Press)
			.SetSound("hover", ButtonState::Hover)
			.SetSound("press", ButtonState::Press);

		SetScale(b1, 1.0f);

		b1.OnPress([]() { PTGN_LOG("Pressed bell!"); });

		auto hover_animation2{ CreateAnimation(
			*this, "animation_hover2", {},
			AnimationConfig{
				.frame_count		= 4,
				.animation_duration = 400ms,
				.frame_size			= V2_int{ 32, 16 },
				.play_count			= std::nullopt,
			}
		) };

		auto press_animation2{ CreateAnimation(
			*this, "animation_press2", {},
			AnimationConfig{
				.frame_count		= 4,
				.animation_duration = 200ms,
				.frame_size			= V2_int{ 32, 16 },
				.play_count			= 1,
			}
		) };

		auto b2_size{ V2_float{ *GetDisplaySize(press_animation2) } };

		b2 = CreateButton(*this, { 0.0f, 200.0f }, Rect{ b2_size }, Origin::Center);

		b2.Icon(ButtonVisualState::Idle).SetTexture("idle2");

		b2.SetAnimation(std::move(hover_animation2), ButtonVisualState::Hover)
			.SetAnimation(std::move(press_animation2), ButtonVisualState::Press)
			.SetSound("hover", ButtonState::Hover)
			.SetSound("press2", ButtonState::Press);

		SetScale(b2, 4.0f);

		b2.OnPress([]() { PTGN_LOG("Pressed button!"); });
	}
};

int main(int, char**) {
	Application app{ "AnimatedButtonScene" };
	app.StartWith<AnimatedButtonScene>();
}