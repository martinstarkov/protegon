
#include "app/application.h"
#include "platform/window/window.h"
#include "runtime/ecs/components/animation.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/ui/button.h"

using namespace ptgn;

class AnimatedButtonScene : public Scene {
public:
	Button b1;

	void OnEnter() override {
		input.SetDrawInteractives();

		app().asset.Load("animation", "assets/animation.png");

		auto activate_animation{
			CreateAnimation(*this, "animation", {}, 4, milliseconds{ 1000 }, { 16, 32 }, 1)
		};

		b1 = CreateAnimatedButton(*this, activate_animation.GetDisplaySize(), activate_animation);

		SetScale(b1, 4.0f);

		b1.OnActivate([]() { PTGN_LOG("Clicked button!"); });
	}
};

int main(int, char**) {
	Application app{ "AnimatedButtonScene" };
	app.StartWith<AnimatedButtonScene>();
}