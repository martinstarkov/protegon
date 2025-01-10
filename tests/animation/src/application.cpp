#include "protegon/protegon.h"

using namespace ptgn;

class AnimationExample : public Scene {
public:
	Texture texture{ "resources/animation.png" };

	V2_float scale{ 5.0f };

	ecs::Manager manager;
	ecs::Entity entity1;
	ecs::Entity entity2;

	void Init() override {
		entity1 = manager.CreateEntity();
		entity2 = manager.CreateEntity();

		entity1.Add<Transform>(game.window.GetCenter(), 0.0f, scale);
		entity1.Add<Sprite>(
			texture, V2_float{}, Origin::CenterBottom, V2_float{ 16, 32 }
		);

		entity2.Add<Transform>(game.window.GetCenter() + V2_float{ 100, 0 }, 0.0f, scale);

		auto& a = entity2.Add<Animation>(
			texture, 4, V2_float{ 16, 32 }, milliseconds{ 500 }, V2_float{},
			V2_float{}, Origin::CenterBottom
		);
		a.Start();

		manager.Refresh();
	}

	void Update() override {
		entity1.Get<Sprite>().Draw(entity1);
		entity2.Get<Animation>().Draw(entity2);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("AnimationExample");
	game.scene.LoadActive<AnimationExample>("animation_example");
	return 0;
}