
#include "core/entity.h"
#include "core/game.h"
#include "events/input_handler.h"
#include "math/vector2.h"
#include "rendering/resources/text.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

namespace ptgn {

struct DialogueLine {
	DialogueLine() = default;

	DialogueLine(std::string_view text_content) : content{ text_content } {}

	std::string content{ "Placeholder" };
	Color color{ color::Pink };
	seconds scroll_duration{ 1 };

	PTGN_SERIALIZER_REGISTER_NAMED_IGNORE_DEFAULTS(
		DialogueLine, KeyValue("content", content), KeyValue("color", color),
		KeyValue("scroll_duration", scroll_duration)
	)
};

enum class DialogueBehavior {
	Sequential, // Pick next dialogue line sequentially until none left. If repeatable, restart,
				// otherwise stop.
	Random // Pick next dialogue line randomly until none left (no repeats). If repeatable, restart,
		   // otherwise stop.
};

PTGN_SERIALIZER_REGISTER_ENUM(
	DialogueBehavior, {
						  { DialogueBehavior::Sequential, "sequential" },
						  { DialogueBehavior::Random, "random" },
					  }
)

struct Dialogue {
	std::size_t index{ 0 };	 // Current position in the lines vector (for cycling).

	bool repeatable{ true }; // Whether or not the entire content can be repeated or not.

	DialogueBehavior behavior{
		DialogueBehavior::Sequential
	}; // How the next dialogue line is chosen.

	bool scroll{ true };

	std::string next_dialogue{ "" };

	std::vector<DialogueLine> lines;
	std::vector<std::size_t>
		used_line_indices; // List of indices of dialogue lines that have already been used. Cleared
						   // if repeatable is true and used_line_indices.size() == lines.size()

	PTGN_SERIALIZER_REGISTER_NAMED_IGNORE_DEFAULTS(
		Dialogue, KeyValue("index", index), KeyValue("repeatable", repeatable),
		KeyValue("behavior", behavior), KeyValue("scroll", scroll), KeyValue("next", next_dialogue),
		KeyValue("lines", lines), KeyValue("used_lines", used_line_indices)
	)
};

namespace impl {

struct DialogueScrollScript : public ptgn::Script<DialogueScrollScript> {
	DialogueScrollScript() {}

	void OnRepeatUpdate(int repeat);
};

} // namespace impl

class DialogueComponent {
public:
	DialogueComponent() = default;

	DialogueComponent(Entity parent, const json& j) {
		text_ = CreateText(parent.GetScene(), "Hello!", color::Pink);
		text_.SetParent(parent);
		j.get_to(*this);
		dialogues_.find("0");
	}

	void Open() {
		// TODO: Pull from JSON and possibly use default, which can also be from JSON.
		milliseconds scroll_duration{ 1000 };
		int characters{ 1000 };
		milliseconds execution_delay{ scroll_duration / characters };
		text_.AddRepeatScript<impl::DialogueScrollScript>(execution_delay, characters, false);
	}

	void NextParagraph() {}

	void UpdateDialogue() {}

	void SetDialogue(std::string_view name) {}

	DialogueComponent(DialogueComponent&&) noexcept			   = default;
	DialogueComponent& operator=(DialogueComponent&&) noexcept = default;
	DialogueComponent(const DialogueComponent&)				   = delete;
	DialogueComponent& operator=(const DialogueComponent&)	   = delete;

	~DialogueComponent() {
		text_.Destroy();
	}

	PTGN_SERIALIZER_REGISTER_NAMED_IGNORE_DEFAULTS(
		DialogueComponent, KeyValue("dialogues", dialogues_)
	)
private:
	Text text_;
	std::unordered_map<std::string, Dialogue> dialogues_;
};

namespace impl {

void DialogueScrollScript::OnRepeatUpdate(int repeat) {
	auto dialogue_entity = entity.GetParent();

	PTGN_ASSERT(dialogue_entity);

	PTGN_ASSERT(dialogue_entity.Has<DialogueComponent>());

	auto& dialogue{ dialogue_entity.Get<DialogueComponent>() };

	const auto& repeat_info{ entity.GetRepeatScriptInfo<DialogueScrollScript>() };

	// PTGN_LOG("Scroll repeat: ", repeat);
}

} // namespace impl

} // namespace ptgn

struct DialogueScene : public Scene {
	Entity npc;

	void Enter() override {
		npc = CreateEntity();
		npc.SetPosition(window_size / 2);
		json j		   = LoadJson("resources/dialogue.json");
		auto& dialogue = npc.Add<DialogueComponent>(npc, j);
		dialogue.Open();

		json save = dialogue;
		PTGN_LOG(save.dump(4));
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("DialogueScene", window_size);
	game.scene.Enter<DialogueScene>("");
	return 0;
}