#include <optional>

#include "app/application.h"
#include "core/assert.h"
#include "core/graphics/color.h"
#include "renderer/primitives/text.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/ui/menu_template.h"

using namespace ptgn;

class GameScene : public Scene {
public:
	int level{ -1 };

	GameScene(int level) : level{ level } {}

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
	SceneTemplateExample() {
		app().asset.LoadMany({ { "bg1", "assets/bg1.png" },
							   { "bg2", "assets/bg2.png" },
							   { "bg3", "assets/bg3.png" } });

		SceneAction::Register("load_level_1", []() {
			app().scene.Transition<GameScene>(std::nullopt, "game_scene", 1);
		});
		SceneAction::Register("load_level_2", []() {
			app().scene.Transition<GameScene>(std::nullopt, "game_scene", 2);
		});
		app().scene.EnterConfig("assets/scenes.json");
	}
};

int main(int, char**) {
	Application app{ "SceneTemplateExample" };
	app.StartWith<SceneTemplateExample>();
}