
#include "components/draw.h"
#include "core/entity.h"
#include "core/game.h"
#include "events/input_handler.h"
#include "math/vector2.h"
#include "rendering/graphics/rect.h"
#include "rendering/renderer.h"
#include "rendering/resources/text.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "ui/dialogue.h"
#include "utility/span.h"

using namespace ptgn;

constexpr V2_int window_size{ 1200, 1200 };

struct DialogueScene : public Scene {
	Entity npc;

	void Enter() override {
		npc = CreateEntity();
		npc.SetPosition(window_size / 2);
		LoadResource("dialogue_box", "resources/box.png");
		[[maybe_unused]] auto& dialogue = npc.Add<DialogueComponent>(
			npc, "resources/dialogue.json", CreateSprite(*this, "dialogue_box")
		);
		//	TestDialogues(dialogue.GetDialogues());
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

	void TestDialogues(const std::unordered_map<std::string, Dialogue>& dialogues) {
		PTGN_ASSERT(MapContains(dialogues, "intro"));
		PTGN_ASSERT(MapContains(dialogues, "outro"));
		PTGN_ASSERT(MapContains(dialogues, "epilogue"));

		const auto& intro	 = dialogues.at("intro");
		const auto& outro	 = dialogues.at("outro");
		const auto& epilogue = dialogues.at("epilogue");

		PTGN_ASSERT(intro.behavior == DialogueBehavior::Sequential);
		PTGN_ASSERT(intro.index == 0);
		PTGN_ASSERT(intro.repeatable == true);
		PTGN_ASSERT(intro.scroll == true);
		PTGN_ASSERT(intro.used_line_indices == std::vector<std::size_t>{});
		PTGN_ASSERT(intro.next_dialogue == "outro");

		PTGN_ASSERT(outro.behavior == DialogueBehavior::Random);
		PTGN_ASSERT(outro.index == 0);
		PTGN_ASSERT(outro.repeatable == true);
		PTGN_ASSERT(outro.scroll == true);
		PTGN_ASSERT(outro.used_line_indices == std::vector<std::size_t>{});
		PTGN_ASSERT(outro.next_dialogue == "epilogue");

		PTGN_ASSERT(epilogue.behavior == DialogueBehavior::Sequential);
		PTGN_ASSERT(epilogue.index == 0);
		PTGN_ASSERT(epilogue.repeatable == true);
		PTGN_ASSERT(epilogue.scroll == true);
		PTGN_ASSERT(epilogue.used_line_indices == std::vector<std::size_t>{});
		PTGN_ASSERT(epilogue.next_dialogue == "");

		PTGN_ASSERT((intro.lines.size() == 3));
		PTGN_ASSERT((intro.lines.at(0).pages.size() == 4));
		PTGN_ASSERT((intro.lines.at(1).pages.size() == 2));
		PTGN_ASSERT((intro.lines.at(2).pages.size() == 2));

		PTGN_ASSERT((intro.lines.at(0).pages.at(0).properties.color == Color{ 0, 255, 0, 255 }));
		PTGN_ASSERT((intro.lines.at(0).pages.at(0).properties.scroll_duration == seconds{ 3 }));
		PTGN_ASSERT((intro.lines.at(0).pages.at(0).content == "Hello traveler!"));
		PTGN_ASSERT((intro.lines.at(0).pages.at(1).properties.color == Color{ 0, 255, 0, 255 }));
		PTGN_ASSERT((intro.lines.at(0).pages.at(1).properties.scroll_duration == seconds{ 3 }));
		PTGN_ASSERT((intro.lines.at(0).pages.at(1).content == "My name is Martin"));
		PTGN_ASSERT((intro.lines.at(0).pages.at(2).properties.color == Color{ 0, 0, 255, 255 }));
		PTGN_ASSERT((intro.lines.at(0).pages.at(3).properties.color == Color{ 0, 0, 255, 255 }));
		const std::string intro_string_a =
			"Nice to meet you! This is an extended piece of dialogue which should be split...";
		const std::string intro_string_b = ",,,among multiple pages!";
		const milliseconds total_duration{ 2000 };
		const std::size_t total_length{ intro_string_a.size() + intro_string_b.size() };
		const auto get_duration = [](std::size_t length, std::size_t total, milliseconds duration) {
			return std::chrono::duration_cast<milliseconds>(
				ptgn::duration<double, milliseconds::period>(
					static_cast<double>(length) / static_cast<double>(total) *
					static_cast<double>(duration.count())
				)
			);
		};
		milliseconds duration_a{
			std::invoke(get_duration, intro_string_a.size(), total_length, total_duration)
		};
		milliseconds duration_b{
			std::invoke(get_duration, intro_string_b.size(), total_length, total_duration)
		};
		PTGN_ASSERT((intro.lines.at(0).pages.at(2).content == intro_string_a));
		PTGN_ASSERT((intro.lines.at(0).pages.at(3).content == intro_string_b));
		PTGN_ASSERT((intro.lines.at(0).pages.at(2).properties.scroll_duration == duration_a));
		PTGN_ASSERT((intro.lines.at(0).pages.at(3).properties.scroll_duration == duration_b));
		PTGN_ASSERT((intro.lines.at(1).pages.at(0).properties.color == Color{ 0, 255, 255, 255 }));
		PTGN_ASSERT(
			(intro.lines.at(1).pages.at(0).properties.scroll_duration == milliseconds{ 300 })
		);
		PTGN_ASSERT((intro.lines.at(1).pages.at(0).content == "Welcome to our city!"));
		PTGN_ASSERT((intro.lines.at(1).pages.at(1).properties.color == Color{ 0, 255, 255, 255 }));
		PTGN_ASSERT(
			(intro.lines.at(1).pages.at(1).properties.scroll_duration == milliseconds{ 300 })
		);
		PTGN_ASSERT((intro.lines.at(1).pages.at(1).content == "My name is Martin"));
		PTGN_ASSERT((intro.lines.at(2).pages.at(0).properties.color == Color{ 255, 0, 0, 255 }));
		PTGN_ASSERT((intro.lines.at(2).pages.at(0).properties.scroll_duration == seconds{ 1 }));
		PTGN_ASSERT((intro.lines.at(2).pages.at(0).content == "You really should get going!"));
		PTGN_ASSERT((intro.lines.at(2).pages.at(1).properties.color == Color{ 255, 0, 0, 255 }));
		PTGN_ASSERT((intro.lines.at(2).pages.at(1).properties.scroll_duration == seconds{ 1 }));
		PTGN_ASSERT((intro.lines.at(2).pages.at(1).content == "Bye!"));

		PTGN_ASSERT((outro.lines.size() == 2));
		PTGN_ASSERT((outro.lines.at(0).pages.size() == 2));
		PTGN_ASSERT((outro.lines.at(1).pages.size() == 2));

		PTGN_ASSERT(
			(outro.lines.at(0).pages.at(0).properties.color == Color{ 255, 255, 255, 255 })
		);
		PTGN_ASSERT((outro.lines.at(0).pages.at(0).properties.scroll_duration == seconds{ 4 }));
		PTGN_ASSERT((outro.lines.at(0).pages.at(0).content == "Great job on the boss fight!"));
		PTGN_ASSERT(
			(outro.lines.at(0).pages.at(1).properties.color == Color{ 255, 255, 255, 255 })
		);
		PTGN_ASSERT((outro.lines.at(0).pages.at(1).properties.scroll_duration == seconds{ 4 }));
		PTGN_ASSERT((outro.lines.at(0).pages.at(1).content == "You have won!"));
		PTGN_ASSERT(
			(outro.lines.at(1).pages.at(0).properties.color == Color{ 255, 255, 255, 255 })
		);
		PTGN_ASSERT((outro.lines.at(1).pages.at(0).properties.scroll_duration == seconds{ 4 }));
		PTGN_ASSERT((outro.lines.at(1).pages.at(0).content == "You are the savior of our city!"));
		PTGN_ASSERT(
			(outro.lines.at(1).pages.at(1).properties.color == Color{ 255, 255, 255, 255 })
		);
		PTGN_ASSERT((outro.lines.at(1).pages.at(1).properties.scroll_duration == seconds{ 4 }));
		PTGN_ASSERT((outro.lines.at(1).pages.at(1).content == "Thank you!"));

		PTGN_ASSERT((epilogue.lines.size() == 1));
		PTGN_ASSERT((epilogue.lines.at(0).pages.size() == 1));

		PTGN_ASSERT(
			(epilogue.lines.at(0).pages.at(0).properties.color == Color{ 255, 255, 255, 255 })
		);
		PTGN_ASSERT((epilogue.lines.at(0).pages.at(0).properties.scroll_duration == seconds{ 4 }));
		PTGN_ASSERT((epilogue.lines.at(0).pages.at(0).content == "Woo!"));
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("DialogueScene", window_size);
	game.scene.Enter<DialogueScene>("");
	return 0;
}