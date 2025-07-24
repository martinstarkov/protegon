
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

struct DialoguePage {
	DialoguePage() = default;

	DialoguePage(
		const std::string& text_content, const Color& color, milliseconds scroll_duration
	) :
		content{ text_content }, color{ color }, scroll_duration{ scroll_duration } {}

	std::string content{ "Placeholder" };
	Color color{ color::Pink };
	milliseconds scroll_duration{ 1000 };
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

	std::vector<DialogueLine> lines;
	std::vector<std::size_t>
		used_line_indices; // List of indices of dialogue lines that have already been used. Cleared
						   // if repeatable is true and used_line_indices.size() == lines.size()
};

namespace impl {

struct DialogueScrollScript : public ptgn::Script<DialogueScrollScript> {
	DialogueScrollScript() {}

	void OnRepeatUpdate(int repeat);
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

	[[nodiscard]] const std::unordered_map<std::string, Dialogue>& GetDialogues() const {
		return dialogues_;
	}

private:
	void LoadFromJson(const json& root);

	[[nodiscard]] static Color InheritColor(const json& j, const Color& parent) {
		return j.value("color", parent);
	}

	[[nodiscard]] static milliseconds InheritDuration(const json& j, const milliseconds& parent) {
		return j.value("scroll_duration", parent);
	}

	// Helper function to split text based on max length and adjust duration proportionally
	[[nodiscard]] static std::vector<DialoguePage> SplitTextWithDuration(
		const std::string& full_text, const Color& color, const milliseconds& total_duration,
		std::size_t max_length, const std::string& split_end, const std::string& split_begin
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
				pages.emplace_back(DialoguePage{ "", color, total_duration });
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
				std::size_t ideal_len = std::min(max_length, remaining);
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
						fraction * static_cast<double>(total_duration.count())
					)
				);
				pages.emplace_back(DialoguePage{ part_texts[i], color, part_duration });
			}
		}

		return pages;
	}

	Text text_;

	std::unordered_map<std::string, Dialogue> dialogues_;
};

inline void DialogueComponent::LoadFromJson(const json& root) {
	const Color root_color			 = root.value("color", color::White);
	const milliseconds root_duration = root.value("scroll_duration", milliseconds{ 1000 });
	std::size_t character_split_count{ root.value("character_split_count", std::size_t{ 80 }) };
	std::string split_end{ root.value("split_end", "...") };
	std::string split_begin{ root.value("split_begin", ",,,") };

	PTGN_ASSERT(root.contains("dialogues"));

	const auto& dialogues_json = root.at("dialogues");

	for (const auto& [dialogue_name, dialogue_json] : dialogues_json.items()) {
		Dialogue dialogue;

		const Color dialogue_color			 = InheritColor(dialogue_json, root_color);
		const milliseconds dialogue_duration = InheritDuration(dialogue_json, root_duration);

		dialogue.index		   = dialogue_json.value("index", std::size_t{ 0 });
		dialogue.repeatable	   = dialogue_json.value("repeatable", true);
		dialogue.scroll		   = dialogue_json.value("scroll", true);
		dialogue.next_dialogue = dialogue_json.value("next", "");
		dialogue.behavior	   = dialogue_json.value("behavior", DialogueBehavior::Sequential);

		PTGN_ASSERT(dialogue_json.contains("lines"));

		const auto& lines_json = dialogue_json.at("lines");

		if (lines_json.is_string()) {
			DialogueLine line;
			auto pages = SplitTextWithDuration(
				lines_json.get<std::string>(), dialogue_color, dialogue_duration,
				character_split_count, split_end, split_begin
			);
			line.pages.insert(line.pages.end(), pages.begin(), pages.end());
			dialogue.lines.push_back(std::move(line));
		} else if (lines_json.is_array()) {
			for (const auto& line_json : lines_json) {
				DialogueLine line;

				if (line_json.is_string()) {
					auto pages = SplitTextWithDuration(
						line_json.get<std::string>(), dialogue_color, dialogue_duration,
						character_split_count, split_end, split_begin
					);
					line.pages.insert(line.pages.end(), pages.begin(), pages.end());
				} else if (line_json.is_object()) {
					const Color line_color = InheritColor(line_json, dialogue_color);
					const milliseconds line_duration =
						InheritDuration(line_json, dialogue_duration);

					PTGN_ASSERT(line_json.contains("pages"));

					const auto& pages_json = line_json.at("pages");

					if (pages_json.is_string()) {
						auto pages = SplitTextWithDuration(
							pages_json.get<std::string>(), line_color, line_duration,
							character_split_count, split_end, split_begin
						);
						line.pages.insert(line.pages.end(), pages.begin(), pages.end());
					} else if (pages_json.is_array()) {
						for (const auto& page_json : pages_json) {
							if (page_json.is_string()) {
								auto pages = SplitTextWithDuration(
									page_json.get<std::string>(), line_color, line_duration,
									character_split_count, split_end, split_begin
								);
								line.pages.insert(line.pages.end(), pages.begin(), pages.end());
							} else if (page_json.is_object()) {
								PTGN_ASSERT(page_json.contains("content"));
								const std::string content = page_json.at("content");

								const Color page_color = InheritColor(page_json, line_color);
								const milliseconds page_duration =
									InheritDuration(page_json, line_duration);

								auto pages = SplitTextWithDuration(
									content, page_color, page_duration, character_split_count,
									split_end, split_begin
								);
								line.pages.insert(line.pages.end(), pages.begin(), pages.end());
							}
						}
					}
				}

				dialogue.lines.push_back(std::move(line));
			}
		}

		dialogues_.emplace(dialogue_name, std::move(dialogue));
	}
}

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
		TestDialogues(dialogue.GetDialogues());
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

		PTGN_ASSERT((intro.lines.at(0).pages.at(0).color == Color{ 0, 255, 0, 255 }));
		PTGN_ASSERT((intro.lines.at(0).pages.at(0).scroll_duration == seconds{ 5 }));
		PTGN_ASSERT((intro.lines.at(0).pages.at(0).content == "Hello traveler!"));
		PTGN_ASSERT((intro.lines.at(0).pages.at(1).color == Color{ 0, 255, 0, 255 }));
		PTGN_ASSERT((intro.lines.at(0).pages.at(1).scroll_duration == seconds{ 5 }));
		PTGN_ASSERT((intro.lines.at(0).pages.at(1).content == "My name is Martin"));
		PTGN_ASSERT((intro.lines.at(0).pages.at(2).color == Color{ 0, 0, 255, 255 }));
		PTGN_ASSERT((intro.lines.at(0).pages.at(3).color == Color{ 0, 0, 255, 255 }));
		const std::string intro_string_a =
			"Nice to meet you! This is an extended piece of dialogue which should be split...";
		const std::string intro_string_b = ",,,among multiple pages!";
		const milliseconds total_duration{ 3000 };
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
		PTGN_ASSERT((intro.lines.at(0).pages.at(2).scroll_duration == duration_a));
		PTGN_ASSERT((intro.lines.at(0).pages.at(3).scroll_duration == duration_b));
		PTGN_ASSERT((intro.lines.at(1).pages.at(0).color == Color{ 0, 255, 255, 255 }));
		PTGN_ASSERT((intro.lines.at(1).pages.at(0).scroll_duration == milliseconds{ 300 }));
		PTGN_ASSERT((intro.lines.at(1).pages.at(0).content == "Welcome to our city!"));
		PTGN_ASSERT((intro.lines.at(1).pages.at(1).color == Color{ 0, 255, 255, 255 }));
		PTGN_ASSERT((intro.lines.at(1).pages.at(1).scroll_duration == milliseconds{ 300 }));
		PTGN_ASSERT((intro.lines.at(1).pages.at(1).content == "My name is Martin"));
		PTGN_ASSERT((intro.lines.at(2).pages.at(0).color == Color{ 255, 0, 0, 255 }));
		PTGN_ASSERT((intro.lines.at(2).pages.at(0).scroll_duration == seconds{ 1 }));
		PTGN_ASSERT((intro.lines.at(2).pages.at(0).content == "You really should get going!"));
		PTGN_ASSERT((intro.lines.at(2).pages.at(1).color == Color{ 255, 0, 0, 255 }));
		PTGN_ASSERT((intro.lines.at(2).pages.at(1).scroll_duration == seconds{ 1 }));
		PTGN_ASSERT((intro.lines.at(2).pages.at(1).content == "Bye!"));

		PTGN_ASSERT((outro.lines.size() == 2));
		PTGN_ASSERT((outro.lines.at(0).pages.size() == 2));
		PTGN_ASSERT((outro.lines.at(1).pages.size() == 2));

		PTGN_ASSERT((outro.lines.at(0).pages.at(0).color == Color{ 255, 255, 255, 255 }));
		PTGN_ASSERT((outro.lines.at(0).pages.at(0).scroll_duration == seconds{ 10 }));
		PTGN_ASSERT((outro.lines.at(0).pages.at(0).content == "Great job on the boss fight!"));
		PTGN_ASSERT((outro.lines.at(0).pages.at(1).color == Color{ 255, 255, 255, 255 }));
		PTGN_ASSERT((outro.lines.at(0).pages.at(1).scroll_duration == seconds{ 10 }));
		PTGN_ASSERT((outro.lines.at(0).pages.at(1).content == "You have won!"));
		PTGN_ASSERT((outro.lines.at(1).pages.at(0).color == Color{ 255, 255, 255, 255 }));
		PTGN_ASSERT((outro.lines.at(1).pages.at(0).scroll_duration == seconds{ 10 }));
		PTGN_ASSERT((outro.lines.at(1).pages.at(0).content == "You are the savior of our city!"));
		PTGN_ASSERT((outro.lines.at(1).pages.at(1).color == Color{ 255, 255, 255, 255 }));
		PTGN_ASSERT((outro.lines.at(1).pages.at(1).scroll_duration == seconds{ 10 }));
		PTGN_ASSERT((outro.lines.at(1).pages.at(1).content == "Thank you!"));

		PTGN_ASSERT((epilogue.lines.size() == 1));
		PTGN_ASSERT((epilogue.lines.at(0).pages.size() == 1));

		PTGN_ASSERT((epilogue.lines.at(0).pages.at(0).color == Color{ 255, 255, 255, 255 }));
		PTGN_ASSERT((epilogue.lines.at(0).pages.at(0).scroll_duration == seconds{ 10 }));
		PTGN_ASSERT((epilogue.lines.at(0).pages.at(0).content == "Woo!"));
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("DialogueScene", window_size);
	game.scene.Enter<DialogueScene>("");
	return 0;
}