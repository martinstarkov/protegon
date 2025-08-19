#include "components/sprite.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/window.h"
#include "input/input_handler.h"
#include "input/key.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "ui/dialogue.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct DialogueScene : public Scene {
	Entity npc;

	void Enter() override {
		game.window.SetSetting(WindowSetting::Resizable);
		LoadResource("dialogue_box", "resources/box.png");

		npc = CreateEntity();
		SetPosition(npc, window_size / 2);

		npc.Add<DialogueComponent>(
			npc, "resources/dialogue.json", CreateSprite(*this, "dialogue_box", {})
		);
	}

	void Update() override {
		auto& dialogue{ npc.Get<DialogueComponent>() };
		if (game.input.KeyDown(Key::Space)) {
			dialogue.Open();
		}
		if (game.input.KeyDown(Key::Escape)) {
			dialogue.Close();
		}
		if (game.input.KeyDown(Key::N)) {
			dialogue.SetNextDialogue();
		}
		if (game.input.KeyDown(Key::I)) {
			dialogue.SetDialogue("intro");
		}
		if (game.input.KeyDown(Key::O)) {
			dialogue.SetDialogue("outro");
		}
		if (game.input.KeyDown(Key::E)) {
			dialogue.SetDialogue("epilogue");
		}
		dialogue.DrawInfo();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("DialogueScene", window_size);
	game.scene.Enter<DialogueScene>("");
	return 0;
}