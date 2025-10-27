#include "core/app/application.h"

#include "world/scene/scene_transition.h"

using namespace ptgn;

class TestScene : public Scene {
public:
	void Enter() override {
		PTGN_INFO("Entered test scene");
	}

	void Update() override {
		PTGN_INFO("Updating test scene");
	}

	void Exit() override {
		PTGN_INFO("Exiting test scene");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application app{};
	app.StartWith<TestScene>("test", SlideLeft{ 2.0 });

	return 0;
}