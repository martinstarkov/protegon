
#include <utility>

#include "app/application.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/window/window.h"
#include "renderer/primitives/color.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/ui/button.h"

using namespace ptgn;

class AnimatedButtonScene : public Scene {
public:
	Button b1;
	Button b2;

	void OnEnter() override {
		ctx().input.SetSettings({ .debug_draw_enabled = true });

		ctx().asset.Load("idle", "assets/bell.png");
		ctx().asset.Load("animation_hover", "assets/bell_hover_animation.png");
		ctx().asset.Load("animation_activate", "assets/bell_click_animation.png");
		ctx().asset.LoadAudio("hover", "assets/hover.ogg");
		ctx().asset.LoadAudio("click", "assets/bell.ogg");

		ctx().asset.Load("idle2", "assets/button_idle.png");
		ctx().asset.Load("animation_hover2", "assets/button_animation_hover.png");
		ctx().asset.Load("animation_activate2", "assets/button_animation_activate.png");
		ctx().asset.LoadAudio("click2", "assets/click.ogg");

		auto hover_animation{ CreateAnimation(
			*this, "animation_hover", V2_int{}, { 3, milliseconds{ 400 }, V2_int{ 253, 167 }, -1 }
		) };

		auto activate_animation{ CreateAnimation(
			*this, "animation_activate", V2_int{}, { 3, milliseconds{ 200 }, V2_int{ 253, 167 }, 1 }
		) };

		b1 = CreateButton(*this, *GetDisplaySize(activate_animation));
		b1.SetTexture("idle")
			.SetAnimation(std::move(hover_animation), ButtonState::Hover)
			.SetAnimation(std::move(activate_animation), ButtonState::Press)
			.SetSound("hover", ButtonState::Hover)
			.SetSound("click", ButtonState::Press);

		SetScale(b1, 1.0f);

		b1.OnActivate([]() { PTGN_LOG("Clicked bell!"); });

		auto hover_animation2{ CreateAnimation(
			*this, "animation_hover2", V2_int{}, { 4, milliseconds{ 400 }, V2_int{ 32, 16 }, -1 }
		) };

		auto activate_animation2{ CreateAnimation(
			*this, "animation_activate2", V2_int{}, { 4, milliseconds{ 200 }, V2_int{ 32, 16 }, 1 }
		) };

		b2 = CreateButton(*this, *GetDisplaySize(activate_animation2));
		b2.SetTexture("idle2")
			.SetAnimation(std::move(hover_animation2), ButtonState::Hover)
			.SetAnimation(std::move(activate_animation2), ButtonState::Press)
			.SetSound("hover", ButtonState::Hover)
			.SetSound("click2", ButtonState::Press);

		SetPosition(b2, { 0, 200 });
		SetScale(b2, 4.0f);

		b2.OnActivate([]() { PTGN_LOG("Clicked button!"); });
	}
};

int main(int, char**) {
	Application app{ "AnimatedButtonScene" };
	app.StartWith<AnimatedButtonScene>();
}