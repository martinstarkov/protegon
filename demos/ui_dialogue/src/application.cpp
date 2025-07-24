
#include "core/entity.h"
#include "core/game.h"
#include "events/input_handler.h"
#include "math/vector2.h"
#include "rendering/resources/text.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "utility/span.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

namespace ptgn {

class DialogueComponent;

struct DialoguePageProperties {
	DialoguePageProperties() = default;

	friend bool operator==(const DialoguePageProperties& a, const DialoguePageProperties& b) {
		return a.scroll_duration == b.scroll_duration && a.color == b.color &&
			   a.max_length == b.max_length;
	}

	friend bool operator!=(const DialoguePageProperties& a, const DialoguePageProperties& b) {
		return !operator==(a, b);
	}

	Color color{ color::White };
	milliseconds scroll_duration{ 1000 };
	std::size_t max_length{ 80 };
};

struct DialoguePage {
	DialoguePage() = default;

	DialoguePage(const std::string& text_content, const DialoguePageProperties& properties) :
		content{ text_content }, properties{ properties } {}

	std::string content;
	DialoguePageProperties properties;
};

struct DialogueLine {
	std::vector<DialoguePage> pages;
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

	std::string next_dialogue;

	// Picks a random index in range [0, max_exclusive) that is not in `excluded_indices`.
	// Internally creates a random number generator.
	[[nodiscard]] std::size_t PickRandomIndex() {
		PTGN_ASSERT(lines.size() > used_line_indices.size() && "Too many excluded indices!");

		if (lines.size() == 1) {
			return 0;
		}

		PTGN_ASSERT(lines.size() > 1);

		RNG<std::size_t> index_rng(0, lines.size() - 1);

		std::size_t chosen_index{ index };

		do {
			chosen_index = std::invoke(index_rng);
		} while (VectorContains(used_line_indices, chosen_index));

		return chosen_index;
	}

	[[nodiscard]] const DialogueLine* GetCurrentDialogueLine() const {
		PTGN_ASSERT(!lines.empty());
		PTGN_ASSERT(!used_line_indices.empty());

		std::size_t current_index{ Mod(index, lines.size()) };

		PTGN_ASSERT(current_index < lines.size());
		PTGN_ASSERT(VectorContains(used_line_indices, current_index));

		return &lines[current_index];
	}

	// @return Index of a new dialogue line based on the dialogue behavior and used lines. -1 if
	// there is no available dialogue line.
	[[nodiscard]] int GetNewDialogueLine() {
		if (lines.empty()) {
			return -1;
		}

		if (lines.size() == used_line_indices.size()) {
			if (!repeatable) {
				return -1;
			} else {
				used_line_indices.clear();
				if (lines.size() > 1 && behavior == DialogueBehavior::Random) {
					used_line_indices.emplace_back(index);
				}
			}
		}

		int chosen_index{ static_cast<int>(index) };

		switch (behavior) {
			case DialogueBehavior::Sequential:
				chosen_index = Mod(chosen_index, static_cast<int>(lines.size()));
				++index;
				break;
			case DialogueBehavior::Random:
				chosen_index = static_cast<int>(PickRandomIndex());
				index		 = chosen_index;
				break;
			default: PTGN_ERROR("Unrecognized dialogue behavior");
		}

		PTGN_ASSERT(chosen_index >= 0);
		PTGN_ASSERT(chosen_index < lines.size());
		PTGN_ASSERT(!VectorContains(used_line_indices, static_cast<std::size_t>(chosen_index)));

		used_line_indices.emplace_back(static_cast<std::size_t>(chosen_index));

		if (lines[static_cast<std::size_t>(chosen_index)].pages.empty()) {
			return -1;
		}

		return chosen_index;
	}

	std::vector<DialogueLine> lines;
	std::vector<std::size_t>
		used_line_indices; // List of indices of dialogue lines that have already been used. Cleared
						   // if repeatable is true and used_line_indices.size() == lines.size()
};

namespace impl {

struct DialogueWaitScript : public ptgn::Script<DialogueWaitScript> {
	DialogueWaitScript() {}

	[[nodiscard]] DialogueComponent& GetDialogueComponent();

	void OnUpdate(float dt);
};

struct DialogueScrollScript : public ptgn::Script<DialogueScrollScript> {
	DialogueScrollScript() {}

	[[nodiscard]] DialogueComponent& GetDialogueComponent();

	void OnTimerStart();
	void OnTimerUpdate(float elapsed_fraction);
};

} // namespace impl

class DialogueComponent {
public:
	DialogueComponent()										   = default;
	DialogueComponent(DialogueComponent&&) noexcept			   = default;
	DialogueComponent& operator=(DialogueComponent&&) noexcept = default;
	DialogueComponent(const DialogueComponent&)				   = delete;
	DialogueComponent& operator=(const DialogueComponent&)	   = delete;

	DialogueComponent(Entity parent, const json& j) {
		text_ = CreateText(parent.GetScene(), "Hello!", color::Pink);
		text_.SetParent(parent);
		LoadFromJson(j);
	}

	~DialogueComponent() {
		text_.Destroy();
	}

	void Open(std::string_view dialogue_name = "") {
		PTGN_ASSERT(!dialogues_.empty(), "Cannot open any dialogue when none have been loaded");
		current_dialogue_ = dialogue_name.empty() ? current_dialogue_ : dialogue_name;
		if (current_dialogue_.empty()) {
			current_dialogue_ = dialogues_.begin()->first;
		}

		auto dialogue{ GetCurrentDialogue() };

		if (!dialogue) {
			PTGN_LOG("No available dialogues");
			return;
		}

		auto dialogue_line_index{ dialogue->GetNewDialogueLine() };

		if (dialogue_line_index == -1) {
			PTGN_LOG("No available dialogue lines");
			return;
		}

		StartDialogueLine(dialogue_line_index);
	}

	void Close() {
		text_.RemoveScript<impl::DialogueWaitScript>();
		text_.RemoveScript<impl::DialogueScrollScript>();
		current_line = -1;
		current_page = -1;
		text_.Hide();
	}

	void NextParagraph() {}

	void UpdateDialogue() {}

	void SetDialogue(std::string_view name) {}

	[[nodiscard]] const std::unordered_map<std::string, Dialogue>& GetDialogues() const {
		return dialogues_;
	}

	[[nodiscard]] Dialogue* GetCurrentDialogue() {
		if (current_dialogue_.empty()) {
			return nullptr;
		}

		auto it{ dialogues_.find(std::string(current_dialogue_)) };

		if (it == dialogues_.end()) {
			return nullptr;
		}

		return &it->second;
	}

	[[nodiscard]] DialogueLine* GetCurrentDialogueLine() {
		if (current_line < 0) {
			return nullptr;
		}
		auto dialogue{ GetCurrentDialogue() };
		if (!dialogue) {
			return nullptr;
		}
		if (current_line >= dialogue->lines.size()) {
			return nullptr;
		}
		return &dialogue->lines[current_line];
	}

	[[nodiscard]] DialoguePage* GetCurrentDialoguePage() {
		if (current_page < 0) {
			return nullptr;
		}
		auto current_line{ GetCurrentDialogueLine() };
		if (!current_line) {
			return nullptr;
		}
		if (current_page >= current_line->pages.size()) {
			return nullptr;
		}
		return &current_line->pages[current_page];
	}

	void IncrementPage() {
		current_page++;
	}

private:
	void StartDialogueLine(int dialogue_line_index) {
		if (text_.HasScript<impl::DialogueScrollScript>() ||
			text_.HasScript<impl::DialogueWaitScript>()) {
			return;
		}
		PTGN_ASSERT(dialogue_line_index >= 0);
		current_line = dialogue_line_index;
		current_page = 0;
		auto page{ GetCurrentDialoguePage() };
		PTGN_ASSERT(page);
		auto duration{ page->properties.scroll_duration };
		text_.AddTimerScript<impl::DialogueScrollScript>(duration);
		text_.AddScript<impl::DialogueWaitScript>();
	}

	void LoadFromJson(const json& root);

	[[nodiscard]] static DialoguePageProperties InheritProperties(
		const json& j, const DialoguePageProperties& parent_properties
	) {
		DialoguePageProperties properties;
		properties.color		   = j.value("color", parent_properties.color);
		properties.scroll_duration = j.value("scroll_duration", parent_properties.scroll_duration);
		return properties;
	}

	// Helper function to split text based on max length and adjust duration proportionally
	[[nodiscard]] static std::vector<DialoguePage> SplitTextWithDuration(
		const std::string& full_text, const DialoguePageProperties& properties,
		const std::string& split_end, const std::string& split_begin
	) {
		std::vector<DialoguePage> pages;

		// First, split by newline
		std::vector<std::string> newline_segments;
		std::size_t start		= 0;
		std::size_t newline_pos = 0;

		while ((newline_pos = full_text.find('\n', start)) != std::string::npos) {
			newline_segments.push_back(full_text.substr(start, newline_pos - start));
			start = newline_pos + 1;
		}
		newline_segments.push_back(full_text.substr(start));

		for (const auto& segment : newline_segments) {
			if (segment.empty()) {
				pages.emplace_back(DialoguePage{ "", properties });
				continue;
			}

			const std::size_t original_len = segment.size();
			std::size_t sub_start		   = 0;
			bool first_split			   = true;

			std::size_t effective_total_length = original_len;

			// Estimate total added split_end and split_begin sizes
			// We'll add the actual sizes dynamically as we go
			std::vector<std::size_t> part_lengths;

			std::vector<std::string> part_texts;

			while (sub_start < original_len) {
				std::size_t remaining = original_len - sub_start;
				std::size_t ideal_len = std::min(properties.max_length, remaining);
				std::size_t sub_end	  = sub_start + ideal_len;

				if (sub_end >= original_len) {
					std::string part = segment.substr(sub_start);
					if (!first_split) {
						part = split_begin + part;
					}

					std::size_t effective_len = part.size();
					part_texts.push_back(part);
					part_lengths.push_back(effective_len);
					break;
				}

				std::size_t cutoff	   = sub_end;
				std::size_t last_space = segment.rfind(' ', cutoff);
				if (last_space != std::string::npos && last_space > sub_start) {
					cutoff = last_space;
				}

				std::string part = segment.substr(sub_start, cutoff - sub_start);
				if (!first_split) {
					part = split_begin + part;
				}

				part					  += split_end;
				std::size_t effective_len  = part.size();
				part_texts.push_back(part);
				part_lengths.push_back(effective_len);

				sub_start	= cutoff + 1;
				first_split = false;
			}

			// Now compute final effective total length (including all added prefixes/suffixes)
			std::size_t total_effective_length = 0;
			for (const auto len : part_lengths) {
				total_effective_length += len;
			}

			for (std::size_t i = 0; i < part_texts.size(); ++i) {
				double fraction = static_cast<double>(part_lengths[i]) /
								  static_cast<double>(total_effective_length);
				milliseconds part_duration = std::chrono::duration_cast<milliseconds>(
					ptgn::duration<double, milliseconds::period>(
						fraction * static_cast<double>(properties.scroll_duration.count())
					)
				);
				auto new_properties{ properties };
				new_properties.scroll_duration = part_duration;
				pages.emplace_back(DialoguePage{ part_texts[i], new_properties });
			}
		}

		return pages;
	}

	Text text_;

	int current_line{ -1 };
	int current_page{ -1 };
	std::string_view current_dialogue_;

	std::unordered_map<std::string, Dialogue> dialogues_;
};

inline void DialogueComponent::LoadFromJson(const json& root) {
	const auto root_properties{ InheritProperties(root, {}) };
	std::string split_end{ root.value("split_end", "...") };
	std::string split_begin{ root.value("split_begin", ",,,") };
	int default_index{ root.value("index", 0) };
	PTGN_ASSERT(default_index >= 0, "Index must be greater than or equal to zero");
	DialogueBehavior behavior{ root.value("behavior", DialogueBehavior::Sequential) };
	bool repeatable{ root.value("repeatable", true) };
	bool scroll{ root.value("scroll", true) };
	std::string next{ root.value("next", "") };

	PTGN_ASSERT(root.contains("dialogues"));

	const auto& dialogues_json = root.at("dialogues");

	for (const auto& [dialogue_name, dialogue_json] : dialogues_json.items()) {
		Dialogue dialogue;

		const auto dialogue_properties = InheritProperties(dialogue_json, root_properties);

		int index{ dialogue_json.value("index", default_index) };
		dialogue.repeatable	   = dialogue_json.value("repeatable", repeatable);
		dialogue.scroll		   = dialogue_json.value("scroll", scroll);
		dialogue.next_dialogue = dialogue_json.value("next", next);
		dialogue.behavior	   = dialogue_json.value("behavior", behavior);

		PTGN_ASSERT(dialogue_json.contains("lines"));

		const auto& lines_json = dialogue_json.at("lines");

		if (lines_json.is_string()) {
			DialogueLine line;
			auto pages = SplitTextWithDuration(
				lines_json.get<std::string>(), dialogue_properties, split_end, split_begin
			);
			line.pages.insert(line.pages.end(), pages.begin(), pages.end());
			dialogue.lines.push_back(std::move(line));
		} else if (lines_json.is_array()) {
			for (const auto& line_json : lines_json) {
				DialogueLine line;

				if (line_json.is_string()) {
					auto pages = SplitTextWithDuration(
						line_json.get<std::string>(), dialogue_properties, split_end, split_begin
					);
					line.pages.insert(line.pages.end(), pages.begin(), pages.end());
				} else if (line_json.is_object()) {
					const auto line_properties = InheritProperties(line_json, dialogue_properties);

					PTGN_ASSERT(line_json.contains("pages"));

					const auto& pages_json = line_json.at("pages");

					if (pages_json.is_string()) {
						auto pages = SplitTextWithDuration(
							pages_json.get<std::string>(), line_properties, split_end, split_begin
						);
						line.pages.insert(line.pages.end(), pages.begin(), pages.end());
					} else if (pages_json.is_array()) {
						for (const auto& page_json : pages_json) {
							if (page_json.is_string()) {
								auto pages = SplitTextWithDuration(
									page_json.get<std::string>(), line_properties, split_end,
									split_begin
								);
								line.pages.insert(line.pages.end(), pages.begin(), pages.end());
							} else if (page_json.is_object()) {
								PTGN_ASSERT(page_json.contains("content"));
								const std::string content = page_json.at("content");

								const auto page_properties{
									InheritProperties(page_json, line_properties)
								};

								auto pages = SplitTextWithDuration(
									content, page_properties, split_end, split_begin
								);
								line.pages.insert(line.pages.end(), pages.begin(), pages.end());
							}
						}
					}
				}

				dialogue.lines.push_back(std::move(line));
			}
		}

		PTGN_ASSERT(index >= 0, "Index must be greater than or equal to zero");

		if (!dialogue.lines.empty()) {
			if (dialogue.behavior == DialogueBehavior::Sequential &&
				index >= static_cast<int>(dialogue.lines.size())) {
				PTGN_WARN(
					"Index ", index, " out of range of '", dialogue_name,
					"' dialogue lines; clamping to ", dialogue.lines.size() - 1
				);
			}
			index = std::clamp(index, 0, static_cast<int>(dialogue.lines.size() - 1));
		} else {
			index = 0;
		}

		dialogue.index = static_cast<std::size_t>(index);

		dialogues_.emplace(dialogue_name, std::move(dialogue));
	}
}

namespace impl {

void DialogueWaitScript::OnUpdate([[maybe_unused]] float dt) {
	if (game.input.KeyDown(Key::E)) {
		auto& dialogue_component{ GetDialogueComponent() };
		dialogue_component.IncrementPage();
		auto page{ dialogue_component.GetCurrentDialoguePage() };
		entity.RemoveScript<DialogueScrollScript>();
		if (!page) {
			entity.Hide();
			auto e = entity;
			e.RemoveScript<DialogueWaitScript>();
		} else {
			auto duration{ page->properties.scroll_duration };
			entity.AddTimerScript<impl::DialogueScrollScript>(duration);
		}
	}
}

DialogueComponent& DialogueWaitScript::GetDialogueComponent() {
	auto dialogue_entity = entity.GetParent();

	PTGN_ASSERT(dialogue_entity);

	PTGN_ASSERT(dialogue_entity.Has<DialogueComponent>());

	return dialogue_entity.Get<DialogueComponent>();
}

DialogueComponent& DialogueScrollScript::GetDialogueComponent() {
	auto dialogue_entity = entity.GetParent();

	PTGN_ASSERT(dialogue_entity);

	PTGN_ASSERT(dialogue_entity.Has<DialogueComponent>());

	return dialogue_entity.Get<DialogueComponent>();
}

void DialogueScrollScript::OnTimerStart() {
	auto& dialogue_component{ GetDialogueComponent() };
	auto page{ dialogue_component.GetCurrentDialoguePage() };
	PTGN_ASSERT(page);
	entity.Show();
	Text{ entity }.SetColor(page->properties.color);
}

void DialogueScrollScript::OnTimerUpdate(float elapsed_fraction) {
	auto& dialogue_component{ GetDialogueComponent() };
	auto page{ dialogue_component.GetCurrentDialoguePage() };
	PTGN_ASSERT(page);
	const auto& text{ page->content };
	std::size_t char_count{ static_cast<std::size_t>(std::round(elapsed_fraction * text.size())) };
	auto revealed_text{ text.substr(0, char_count) };
	// For debugging purposes:
	/*
	std::stringstream s;
	s << entity.GetTimerScriptInfo<DialogueScrollScript>().duration << " ";
	auto duration_str{ s.str() };
	*/
	Text{ entity }.SetContent(revealed_text);
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
		TestDialogues(dialogue.GetDialogues());
	}

	void Update() override {
		auto& dialogue{ npc.Get<DialogueComponent>() };
		if (game.input.KeyDown(Key::Space)) {
			dialogue.Open("intro");
		}
		if (game.input.KeyDown(Key::Escape)) {
			dialogue.Close();
		}
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