#include "runtime/ui/dialogue.h"

#include <fstream>

#include "app/application.h"
#include "core/assert.h"
#include "core/editor.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/util/file.h"
#include "nlohmann/json.hpp"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"
#include "serialization/json/json.h"
#include "tools/debug/debug_system.h"

using namespace ptgn;

struct DialogueScene : public Scene {
	Entity npc;
	DialogueBox dialogue;

	[[nodiscard]] json LoadDialogueJson() const {
		std::ifstream file{ GetAbsolutePath("assets/dialogue.json") };
		PTGN_ASSERT(file.is_open(), "Failed to open dialogue json file");

		json data;
		file >> data;
		return data;
	}

	DialogueBox CreateNpcDialogue() {
		auto dialogue_box{ CreateDialogueBox(
			*this, { 0, 220 },
			DialogueDesc{
				.origin				= Origin::Center,
				.data				= LoadDialogueJson(),
				.box_size			= { 600.0f, 160.0f },
				.background_texture = "dialogue_box",
				.ui_layer			= true,
			}
		) };

		SetParent(dialogue_box, npc);

		return dialogue_box;
	}

	void OnEnter() override {
		ctx().debug.text.draw_enabled = true;
		ctx().interaction.SetDebugSettings({ .draw_enabled = true });

		PTGN_LOG("Entity count: ", GetEntityCount());

		ctx().asset.Load("retro_gaming", "assets/Arial.ttf");
		ctx().asset.Load("dialogue_box", "assets/dialogue_box.png");

		npc = CreateEntity();

		dialogue = CreateNpcDialogue();

		Refresh();

		PTGN_LOG("Entity count: ", GetEntityCount());
	}

	void OnUpdate() override {
		if (dialogue) {
			if (ctx().input.KeyPressed(Key::Space)) {
				dialogue.Open();
			}

			if (ctx().input.KeyPressed(Key::Escape)) {
				dialogue.Close();
			}

			if (ctx().input.KeyPressed(Key::N)) {
				dialogue.SetNextDialogue();
			}

			if (ctx().input.KeyPressed(Key::I)) {
				dialogue.SetDialogue("intro");
			}

			if (ctx().input.KeyPressed(Key::O)) {
				dialogue.SetDialogue("outro");
			}

			if (ctx().input.KeyPressed(Key::E)) {
				dialogue.SetDialogue("epilogue");
			}
		}

		if (ctx().input.KeyPressed(Key::A)) {
			if (!dialogue) {
				dialogue = CreateNpcDialogue();
				Refresh();
				PTGN_LOG("Entity count: ", GetEntityCount());
			}
		}

		if (ctx().input.KeyPressed(Key::D)) {
			if (dialogue) {
				dialogue.Close();
				dialogue.Destroy();
				Refresh();
				PTGN_LOG("Entity count: ", GetEntityCount());
			}
		}
	}
};

int main(int, char**) {
	Application app{ "DialogueScene: Space: Show, Enter: Continue, N: Next, "
					 "A/D: Add/Delete, I: Intro, O: Outro, E: Epilogue" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<DialogueScene>();
}