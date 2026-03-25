#include <format>
#include <memory>
#include <string>

#include "app/application.h"
#include "core/assert.h"
#include "renderer/primitives/color.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"

#include "runtime/scene/scene_manager.h"
#include "runtime/ui/menu_template.h"

using namespace ptgn;

class GameScene : public Scene {
public:
	int level{ -1 };

	explicit GameScene(int level) : level{ level } {}

	void OnEnter() override {
		PTGN_ASSERT(level != -1);

		std::string label{ std::format("Level {}", level) };
		Color color;

		switch (level) {
			case 1:	 color = color::Blue; break;
			case 2:	 color = color::Red; break;
			default: color = Color::RandomOpaque(); break;
		}

		CreateRect(*this, {}, { 100, 100 }, color);
		CreateText(*this, label, color::White);
	}
};

class SceneTemplateExample : public Scene {
public:
	void OnEnter() {
		ctx().asset.LoadMany({ { "bg1", "assets/scene1.png" },
							   { "bg2", "assets/scene2.png" },
							   { "bg3", "assets/scene3.png" } });

		SceneAction::Register(*this, "load_level_1", [ctx = ctx()]() mutable {
			ctx->scene.SwitchTo<GameScene>("game_scene", {}, 1);
		});
		SceneAction::Register(*this, "load_level_2", [ctx = ctx()]() mutable {
			ctx->scene.SwitchTo<GameScene>("game_scene", {}, 2);
		});

		EnterSceneConfig(*this, "assets/scenes.json");
	}
};

int main(int, char**) {
	Application app{ "SceneTemplateExample" };
	app.StartWith<SceneTemplateExample>();
}