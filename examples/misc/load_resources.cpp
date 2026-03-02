
#include "app/application.h"
#include "app/context.h"
#include "core/log.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class LoadResourcesScene : public Scene {
public:
	void OnEnter() override {
		// app().asset.LoadMany("assets/assets.json");
		app().asset.LoadDirectory("assets");
		PTGN_LOG("Loaded all assets!");
	}
};

int main(int, char**) {
	Application game{ "LoadResourcesScene" };
	game.StartWith<LoadResourcesScene>();
}