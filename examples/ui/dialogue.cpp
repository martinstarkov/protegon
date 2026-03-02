#include "runtime/ui/dialogue.h"

#include "app/application.h"
#include "core/math/vector2.h"
#include "platform/input/input_handler.h"
#include "platform/input/key.h"
#include "platform/window/window.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 };

struct DialogueScene : public Scene {
	Entity npc;

	void OnEnter() override {
		PTGN_LOG("Entity count: ", Size());

		app().asset.Load("dialogue_box", "assets/box.png");

		npc = CreateEntity();
		SetPosition(npc, {});

		Refresh();
		PTGN_LOG("Entity count: ", Size());

		npc.Add<DialogueComponent>(
			npc, "assets/dialogue.json", CreateSprite(*this, "dialogue_box", {})
		);

		Refresh();
		PTGN_LOG("Entity count: ", Size());
	}

	void OnUpdate() override {
		if (auto dialogue{ npc.TryGet<DialogueComponent>() }) {
			if (input.KeyPressed(Key::Space)) {
				dialogue->Open();
			}
			if (input.KeyPressed(Key::Escape)) {
				dialogue->Close();
			}
			if (input.KeyPressed(Key::N)) {
				dialogue->SetNextDialogue();
			}
			if (input.KeyPressed(Key::I)) {
				dialogue->SetDialogue("intro");
			}
			if (input.KeyPressed(Key::O)) {
				dialogue->SetDialogue("outro");
			}
			if (input.KeyPressed(Key::E)) {
				dialogue->SetDialogue("epilogue");
			}
			dialogue->DrawInfo(-game_size * 0.5f);
		}
		if (input.KeyPressed(Key::A)) {
			npc.Add<DialogueComponent>(
				npc, "assets/dialogue.json", CreateSprite(*this, "dialogue_box", {})
			);
			PTGN_LOG("Entity count: ", Size());
		}
		if (input.KeyPressed(Key::D)) {
			npc.Remove<DialogueComponent>();
			PTGN_LOG("Entity count: ", Size());
		}
	}
};

int main(int, char**) {
	Application app{ "DialogueScene: Space: Show, Enter: Continue, N: Next, "
					 "A/D: Add/Delete, I: Intro, O: "
					 "Outro, E: Epilogue",
					 game_size };
	app.StartWith<DialogueScene>();
}