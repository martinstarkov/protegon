
#include "app/application.h"
#include "core/log.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"


using namespace ptgn;

class LoadResourcesScene : public Scene {
public:
	void OnEnter() override {
		// ctx().asset.LoadMany("examples/assets/assets.json");
		ctx().asset.LoadDirectory("assets");
		PTGN_LOG("Loaded all assets!");
	}
};

int main(int, char**) {
	Application game{ "LoadResourcesScene" };
	game.StartWith<LoadResourcesScene>();
}