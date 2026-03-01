
#include "core/app/game.h"
#include "core/app/window.h"
#include "core/ecs/components/animation.h"
#include "core/ecs/components/transform.h"
#include "ui/button.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"

using namespace ptgn;

class AnimatedButtonScene : public Scene {
public:
	Button b1;

	void OnEnter() override {
		input.SetDrawInteractives();

		LoadResource("animation", "resources/animation.png");

		auto activate_animation{
			CreateAnimation(*this, "animation", {}, 4, milliseconds{ 1000 }, { 16, 32 }, 1)
		};

		b1 = CreateAnimatedButton(*this, activate_animation.GetDisplaySize(), activate_animation);

		SetScale(b1, 4.0f);

		b1.OnActivate([]() { PTGN_LOG("Clicked button!"); });
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application app{ "AnimatedButtonScene" };
	app.StartWith<AnimatedButtonScene>();
}