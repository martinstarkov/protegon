
#include "app/application.h"
#include "app/context.h"
#include "core/log.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class LoadResourcesScene : public Scene {
public:
	void OnEnter() override {
		app().assets.LoadMany("assets/assets.json");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application game{ "LoadResourcesScene" };
	game.StartWith<LoadResourcesScene>();
}