
#include "app/application.h"
#include "app/context.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/window/window.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/ui/button.h"

using namespace ptgn;

class AnimatedButtonScene : public Scene {
public:
	Button b1;

	void OnEnter() override {
		// TODO: Fix draw interactables.
		// input.SetDrawInteractives();

		app().asset.Load("animation", "assets/button_animation.png");

		auto activate_animation{ CreateAnimation(
			*this, "animation", V2_int{}, 4, milliseconds{ 1000 }, V2_int{ 16, 32 }, 1
		) };

		b1 = CreateAnimatedButton(*this, GetDisplaySize(activate_animation), activate_animation);

		SetScale(b1, 4.0f);

		b1.OnActivate([]() { PTGN_LOG("Clicked button!"); });
	}
};

int main(int, char**) {
	Application app{ "AnimatedButtonScene" };
	app.StartWith<AnimatedButtonScene>();
}