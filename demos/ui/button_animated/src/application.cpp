
#include "core/ecs/components/animation.h"
#include "core/ecs/components/transform.h"
#include "core/app/application.h"
#include "core/app/window.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"
#include "ui/button.h"

using namespace ptgn;

class AnimatedButtonScene : public Scene {
public:
	Button b1;

	void Enter() override {
		game.window.SetResizable();
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
	game.Init("AnimatedButtonScene");
	game.scene.Enter<AnimatedButtonScene>("");
	return 0;
}