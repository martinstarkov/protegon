
#include "app/application.h"
#include "protegon_editor/layer.h"
#include "runtime/asset/asset_manager.h"

#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class EditorScene : public Scene {
public:
	void OnEnter() override {
		ctx().asset.Load("tree", "assets/jpg.jpg");

		CreateSprite(*this, "tree", {});
	}

	void OnUpdate() override {}

	void OnExit() override {}

	void OnEvent(EventDispatcher d) override {}
};

int main(int, char**) {
	Application app{ "EditorScene" };
	app.PushLayer<editor::EditorLayer>();
	app.StartWith<EditorScene>();
}