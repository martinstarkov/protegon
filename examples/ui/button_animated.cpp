
#include "app/application.h"
#include "app/context.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/window/window.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/ui/button.h"

using namespace ptgn;

class AnimatedButtonScene : public Scene {
public:
	Button b1;

	void OnEnter() override {
		input.SetSettings({ .debug_draw_enabled = true });

		app().asset.Load("animation_hover", "assets/button_animation_hover.png");
		app().asset.Load("animation_activate", "assets/button_animation_activate.png");

		auto hover_animation{ CreateAnimation(
			*this, "animation_activate", V2_int{}, 4, milliseconds{ 200 }, V2_int{ 32, 16 }, 1
		) };

		auto activate_animation{ CreateAnimation(
			*this, "animation_activate", V2_int{}, 4, milliseconds{ 200 }, V2_int{ 32, 16 }, 1
		) };

		b1 = CreateAnimatedButton(
			*this, *GetDisplaySize(activate_animation), activate_animation, hover_animation
		);
		// TODO: Fix tints.
		/*.SetTint(color::Red, ButtonState::Default)
		.SetTint(color::Green, ButtonState::Hover)
		.SetTint(color::Blue, ButtonState::Pressed);*/

		SetScale(b1, 4.0f);

		b1.OnActivate([]() { PTGN_LOG("Clicked button!"); });
	}
};

int main(int, char**) {
	Application app{ "AnimatedButtonScene" };
	app.StartWith<AnimatedButtonScene>();
}