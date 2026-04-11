#include "runtime/ui/dialogue.h"

#include "app/application.h"
#include "core/log.h"
#include "core/math/vector2.h"

#include "platform/key.h"
#include "platform/window.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

struct DialogueScene : public Scene {
	Entity npc;

	void OnEnter() override {
		ctx().input.SetSettings({ .debug_draw_enabled = true });

		PTGN_LOG("Entity count: ", GetEntityCount());

		ctx().asset.Load("retro_gaming", "examples/assets/Arial.ttf");
		ctx().asset.Load("dialogue_box", "examples/assets/dialogue_box.png");

		npc = CreateEntity();

		Refresh();
		PTGN_LOG("Entity count: ", GetEntityCount());

		npc.Add<DialogueComponent>(
			npc, "examples/assets/dialogue.json", GameObject{ CreateSprite(*this, "dialogue_box", {}) }
		);

		Refresh();
		PTGN_LOG("Entity count: ", GetEntityCount());
	}

	void OnUpdate() override {
		if (auto dialogue{ npc.TryGet<DialogueComponent>() }) {
			if (ctx().input.KeyPressed(Key::Space)) {
				dialogue->Open();
			}
			if (ctx().input.KeyPressed(Key::Escape)) {
				dialogue->Close();
			}
			if (ctx().input.KeyPressed(Key::N)) {
				dialogue->SetNextDialogue();
			}
			if (ctx().input.KeyPressed(Key::I)) {
				dialogue->SetDialogue("intro");
			}
			if (ctx().input.KeyPressed(Key::O)) {
				dialogue->SetDialogue("outro");
			}
			if (ctx().input.KeyPressed(Key::E)) {
				dialogue->SetDialogue("epilogue");
			}
			dialogue->DrawInfo(*this, -ctx().renderer.GetGameSize() * 0.5f);
		}
		if (ctx().input.KeyPressed(Key::A)) {
			npc.Add<DialogueComponent>(
				npc, "examples/assets/dialogue.json", GameObject{ CreateSprite(*this, "dialogue_box", {}) }
			);
			PTGN_LOG("Entity count: ", GetEntityCount());
		}
		if (ctx().input.KeyPressed(Key::D)) {
			npc.Remove<DialogueComponent>();
			PTGN_LOG("Entity count: ", GetEntityCount());
		}
	}
};

int main(int, char**) {
	Application app{ "DialogueScene: Space: Show, Enter: Continue, N: Next, "
					 "A/D: Add/Delete, I: Intro, O: "
					 "Outro, E: Epilogue" };
	app.StartWith<DialogueScene>();
}