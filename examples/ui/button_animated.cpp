
#include <chrono>
#include <optional>
#include <utility>

#include "app/application.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/ui/button.h"

using namespace ptgn;

class AnimatedButtonScene : public Scene {
public:
	Button b1;
	Button b2;

	void OnEnter() override {
		ctx().input.SetSettings({ .debug_draw_enabled = true });

		ctx().asset.Load("idle", "examples/assets/bell.png");
		ctx().asset.Load("animation_hover", "examples/assets/bell_hover_animation.png");
		ctx().asset.Load("animation_press", "examples/assets/bell_press_animation.png");
		ctx().asset.LoadAudio("hover", "examples/assets/hover.ogg");
		ctx().asset.LoadAudio("press", "examples/assets/bell.ogg");

		ctx().asset.Load("idle2", "examples/assets/button_idle.png");
		ctx().asset.Load("animation_hover2", "examples/assets/button_hover_animation.png");
		ctx().asset.Load("animation_press2", "examples/assets/button_press_animation.png");
		ctx().asset.LoadAudio("press2", "examples/assets/press.ogg");

		auto hover_animation{ CreateAnimation(
			*this, "animation_hover", {}, { 3, 400ms, V2_int{ 253, 167 }, std::nullopt }
		) };

		auto press_animation{
			CreateAnimation(*this, "animation_press", {}, { 3, 200ms, V2_int{ 253, 167 }, 1 })
		};

		b1 = CreateButton(*this, {}, *GetDisplaySize(press_animation));
		b1.SetTexture("idle")
			.SetAnimation(std::move(hover_animation), ButtonState::Hover)
			.SetAnimation(std::move(press_animation), ButtonState::Press)
			.SetSound("hover", ButtonState::Hover)
			.SetSound("press", ButtonState::Press);

		SetScale(b1, 1.0f);

		b1.OnPress([]() { PTGN_LOG("Pressed bell!"); });

		auto hover_animation2{ CreateAnimation(
			*this, "animation_hover2", {}, { 4, 400ms, V2_int{ 32, 16 }, std::nullopt }
		) };

		auto press_animation2{
			CreateAnimation(*this, "animation_press2", {}, { 4, 200ms, V2_int{ 32, 16 }, 1 })
		};

		b2 = CreateButton(*this, { 0, 200 }, *GetDisplaySize(press_animation2));
		b2.SetTexture("idle2")
			.SetAnimation(std::move(hover_animation2), ButtonState::Hover)
			.SetAnimation(std::move(press_animation2), ButtonState::Press)
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