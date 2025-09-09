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

constexpr V2_int resolution{ 800, 800 };

struct DialogueScene : public Scene {
	Entity npc;

	void Enter() override {
		PTGN_LOG("Entity count: ", Size());
		game.window.SetResizable();
		LoadResource("dialogue_box", "resources/box.png");

		npc = CreateEntity();
		SetPosition(npc, {});

		Refresh();
		PTGN_LOG("Entity count: ", Size());

		npc.Add<DialogueComponent>(
			npc, "resources/dialogue.json", CreateSprite(*this, "dialogue_box", {})
		);

		Refresh();
		PTGN_LOG("Entity count: ", Size());
	}

	void Update() override {
		if (auto dialogue{ npc.TryGet<DialogueComponent>() }) {
			if (input.KeyDown(Key::Space)) {
				dialogue->Open();
			}
			if (input.KeyDown(Key::Escape)) {
				dialogue->Close();
			}
			if (input.KeyDown(Key::N)) {
				dialogue->SetNextDialogue();
			}
			if (input.KeyDown(Key::I)) {
				dialogue->SetDialogue("intro");
			}
			if (input.KeyDown(Key::O)) {
				dialogue->SetDialogue("outro");
			}
			if (input.KeyDown(Key::E)) {
				dialogue->SetDialogue("epilogue");
			}
			dialogue->DrawInfo(-resolution * 0.5f);
		}
		if (input.KeyDown(Key::A)) {
			npc.Add<DialogueComponent>(
				npc, "resources/dialogue.json", CreateSprite(*this, "dialogue_box", {})
			);
			PTGN_LOG("Entity count: ", Size());
		}
		if (input.KeyDown(Key::D)) {
			npc.Remove<DialogueComponent>();
			PTGN_LOG("Entity count: ", Size());
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init(
		"DialogueScene: Space: Show, Enter: Continue, N: Next, A/D: Add/Delete, I: Intro, O: "
		"Outro, E: Epilogue",
		resolution
	);
	game.scene.Enter<DialogueScene>("");
	return 0;
}