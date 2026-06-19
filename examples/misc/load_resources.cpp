
#include "app/application.h"
#include "core/editor.h"
#include "core/log.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class LoadResourcesScene : public Scene {
public:
	void OnEnter() override {
		// ctx().asset.LoadMany("assets/assets.json");
		PTGN_LOG("Loading all assets...");
		ctx().asset.LoadDirectory("assets");
		PTGN_LOG("Loaded all assets!");
	}
};

int main(int, char**) {
	Application app{ "LoadResourcesScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<LoadResourcesScene>();
}