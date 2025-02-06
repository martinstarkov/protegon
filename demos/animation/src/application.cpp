#include "components/draw.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "renderer/texture.h"
#include "scene/scene.h"
#include "utility/time.h"

using namespace ptgn;

class AnimationExample : public Scene {
public:
	V2_float scale{ 5.0f };

	void Enter() override {
		game.texture.Load("test", "resources/animation.png");

		ecs::Entity s1 = manager.CreateEntity();
		s1.Add<Transform>(
			game.window.GetCenter() - V2_float{ 0, 50 }, 0.0f, scale
			/*, half_pi<float> / 2.0f, V2_float{ 1.0f }*/
		);
		auto& a = s1.Add<Animation>(s1, "test", 4, V2_float{ 16, 32 }, milliseconds{ 500 });
		a.Start();
		// s1.Add<Size>(V2_float{ 800, 800 });
		// s1.Add<Offset>(V2_float{ 0, 0 });
		// s1.Add<Tint>(color::White);
		s1.Add<Visible>();

		manager.Refresh();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("AnimationExample");
	game.Start<AnimationExample>("animation_example");
	return 0;
}