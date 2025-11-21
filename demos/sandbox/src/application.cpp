#include "core/app/application.h"

#include "scene/scene_manager.h"

using namespace ptgn;

class SecondScene : public Scene {
public:
	void Enter() override {
		PTGN_INFO("Entered second scene");
	}

	void Update() override {
		// PTGN_INFO("Updating test scene");
	}

	void Exit() override {
		// PTGN_INFO("Exiting test scene");
	}
};

class FirstScene : public Scene {
public:
	void Enter() override {
		PTGN_INFO("Entered first scene");
		app().scenes.SwitchTo<SecondScene>("second", std::make_unique<SlideLeft>(secondsf{ 3.0f }));
		// PTGN_INFO("Entered test scene");
	}

	void Update() override {
		// PTGN_INFO("Updating test scene");
	}

	void Exit() override {
		// PTGN_INFO("Exiting test scene");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application app{};
	app.StartWith<FirstScene>("first");

	return 0;
}