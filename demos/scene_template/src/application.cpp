#include <optional>

#include "common/assert.h"
#include "components/draw.h"
#include "core/game.h"
#include "renderer/api/color.h"
#include "renderer/text.h"
#include "scene/menu_template.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class GameScene : public Scene {
public:
	int level{ -1 };

	GameScene(int level) : level{ level } {}

	void Enter() override {
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
		LoadResource({ { "bg1", "resources/bg1.png" },
					   { "bg2", "resources/bg2.png" },
					   { "bg3", "resources/bg3.png" } });

		SceneAction::Register("load_level_1", []() {
			game.scene.Transition<GameScene>(std::nullopt, "game_scene", 1);
		});
		SceneAction::Register("load_level_2", []() {
			game.scene.Transition<GameScene>(std::nullopt, "game_scene", 2);
		});
		game.scene.EnterConfig("resources/scenes.json");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("SceneTemplateExample");
	game.scene.Enter<SceneTemplateExample>("");
	return 0;
}