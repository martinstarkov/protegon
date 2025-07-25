
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
#include "utility/span.h"

using namespace ptgn;

constexpr V2_int window_size{ 1200, 1200 };

namespace ptgn {

class DialogueComponent;

struct DialoguePageProperties {
	DialoguePageProperties() = default;

	friend bool operator==(const DialoguePageProperties& a, const DialoguePageProperties& b) {
		return a.scroll_duration == b.scroll_duration && a.color == b.color &&
			   a.font_key == b.font_key && a.box_size == b.box_size && a.font_size == b.font_size &&
			   NearlyEqual(a.padding_bottom, b.padding_bottom) &&
			   NearlyEqual(a.padding_left, b.padding_left) &&
			   NearlyEqual(a.padding_right, b.padding_right) &&
			   NearlyEqual(a.padding_top, b.padding_top);
	}

	friend bool operator!=(const DialoguePageProperties& a, const DialoguePageProperties& b) {
		return !operator==(a, b);
	}

	// Use the dialogue page properties of the json or its parent object.
	[[nodiscard]] DialoguePageProperties InheritProperties(const json& j) const {
		DialoguePageProperties properties;
		properties.color		   = j.value("color", color);
		properties.scroll_duration = j.value("scroll_duration", scroll_duration);
		properties.box_size		   = j.value("box_size", box_size);
		properties.font_key		   = j.value("font_key", font_key);
		properties.padding_top	   = j.value("padding_top", padding_top);
		properties.padding_bottom  = j.value("padding_bottom", padding_bottom);
		properties.padding_left	   = j.value("padding_left", padding_left);
		properties.padding_right   = j.value("padding_right", padding_right);
		properties.font_size	   = j.value("font_size", font_size);
		return properties;
	}

	void SetPadding(float padding) {
		padding_left   = padding;
		padding_right  = padding;
		padding_top	   = padding;
		padding_bottom = padding;
	}

	void SetPadding(const V2_float& padding) {
		padding_left   = padding.x;
		padding_right  = padding.x;
		padding_top	   = padding.y;
		padding_bottom = padding.y;
	}

	void SetPadding(float top, float right, float bottom, float left) {
		padding_left   = left;
		padding_right  = right;
		padding_top	   = top;
		padding_bottom = bottom;
	}

	Color color{ color::White };
	FontKey font_key;
	FontSize font_size;
	V2_float box_size;
	float padding_left{ 0.0f };
	float padding_right{ 0.0f };
	float padding_top{ 0.0f };
	float padding_bottom{ 0.0f };
	milliseconds scroll_duration{ 1000 };
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
				PTGN_ASSERT(chosen_index >= 0);
				index = static_cast<std::size_t>(chosen_index);
				break;
			default: PTGN_ERROR("Unrecognized dialogue behavior");
		}

		PTGN_ASSERT(chosen_index >= 0);
		PTGN_ASSERT(static_cast<std::size_t>(chosen_index) < lines.size());
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

	void UpdateText(float elapsed_fraction);

	bool OnTimerStop();
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

	Sprite bg;

	DialogueComponent(Entity parent, const json& j) {
		auto& scene{ parent.GetScene() };
		path bg_path{ "resources/box.png" };
		LoadResource(bg_path.string(), bg_path);
		bg = CreateSprite(scene, bg_path.string());
		// bg.SetScale();
		bg.SetParent(parent);
		text_ = CreateText(scene, "", color::White);
		text_.SetParent(parent);
		text_.SetFontSize(50);
		DialoguePageProperties default_properties;
		default_properties.SetPadding(80.0f);
		default_properties.box_size	 = bg.GetDisplaySize();
		default_properties.font_key	 = text_.GetFontKey();
		default_properties.font_size = text_.GetFontSize();
		AlignToTopLeft(default_properties);
		LoadFromJson(j, default_properties);
		Close();
	}

	~DialogueComponent() {
		text_.Destroy();
	}

	[[nodiscard]] const Text& GetText() const {
		return text_;
	}

	[[nodiscard]] Text& GetText() {
		return text_;
	}

	[[nodiscard]] bool IsOpen() const {
		return text_.IsVisible();
	}

	void Open(const std::string& dialogue_name = "") {
		PTGN_ASSERT(!dialogues_.empty(), "Cannot open any dialogue when none have been loaded");

		if (!dialogue_name.empty()) {
			if (dialogue_name == current_dialogue_ && IsOpen()) {
				return;
			}
			current_dialogue_ = dialogue_name;
		} else if (IsOpen()) {
			return;
		}

		if (current_dialogue_.empty()) {
			// PTGN_LOG("No current dialogue set");
			return;
		}

		auto dialogue{ GetCurrentDialogue() };

		if (!dialogue) {
			// PTGN_LOG("No available dialogues");
			return;
		}

		auto dialogue_line_index{ dialogue->GetNewDialogueLine() };

		if (dialogue_line_index == -1) {
			// PTGN_LOG("No available dialogue lines");
			return;
		}

		StartDialogueLine(dialogue_line_index);

		text_.Show();
	}

	void Close() {
		auto e{ text_ };
		e.Hide();
		e.RemoveScript<impl::DialogueWaitScript>();
		e.RemoveScript<impl::DialogueScrollScript>();
		current_line = 0;
		current_page = 0;
	}

	// Cycle the page to the next one.
	void NextPage() {
		IncrementPage();
		auto page{ GetCurrentDialoguePage() };
		if (!page) {
			Close();
		} else {
			text_.RemoveScript<impl::DialogueScrollScript>();
			auto duration{ page->properties.scroll_duration };
			text_.AddTimerScript<impl::DialogueScrollScript>(duration);
		}
	}

	// Sets the dialogue to the next dialogue according to the json.
	void SetNextDialogue() {
		auto dialogue{ GetCurrentDialogue() };
		if (!dialogue) {
			current_line = 0;
			current_page = 0;
			return;
		}
		auto next_dialogue{ dialogue->next_dialogue };
		PTGN_ASSERT(
			(next_dialogue.empty() || MapContains(dialogues_, next_dialogue)), "Next dialogue '",
			next_dialogue, "' not found in the dialogue map"
		);
		if (next_dialogue.empty()) {
			current_line = 0;
			current_page = 0;
		}
		current_dialogue_ = next_dialogue;
	}

	// Set dialogue to a specific one in the json.
	void SetDialogue(const std::string& name = "") {
		PTGN_ASSERT(
			(name.empty() || MapContains(dialogues_, name)), "Dialogue '", name,
			"' not found in the dialogue map"
		);
		current_dialogue_ = name;
		current_line	  = 0;
		current_page	  = 0;
	}

	// TODO: Move all of this to private:

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
		if (static_cast<std::size_t>(current_line) >= dialogue->lines.size()) {
			return nullptr;
		}
		return &dialogue->lines[static_cast<std::size_t>(current_line)];
	}

	[[nodiscard]] DialoguePage* GetCurrentDialoguePage() {
		if (current_page < 0) {
			return nullptr;
		}
		auto line{ GetCurrentDialogueLine() };
		if (!line) {
			return nullptr;
		}
		if (static_cast<std::size_t>(current_page) >= line->pages.size()) {
			return nullptr;
		}
		return &line->pages[static_cast<std::size_t>(current_page)];
	}

	void IncrementPage() {
		current_page++;
	}

	void DrawInfo() {
		FontSize font_size{ 32 };
		DrawDebugText(
			"Dialogue: " + current_dialogue_, { 0, 0 }, color::White, Origin::TopLeft, font_size
		);
		DrawDebugText(
			"Line: " + std::to_string(current_line), { 0, 50 }, color::White, Origin::TopLeft,
			font_size
		);
		DrawDebugText(
			"Page: " + std::to_string(current_page), { 0, 100 }, color::White, Origin::TopLeft,
			font_size
		);
	}

private:
	void AlignToTopLeft(const DialoguePageProperties& default_properties) {
		text_.SetPosition(
			V2_float{ default_properties.padding_left, default_properties.padding_top } -
			default_properties.box_size / 2.0f
		);
		text_.SetOrigin(Origin::TopLeft);
	}

	void StartDialogueLine(int dialogue_line_index) {
		PTGN_ASSERT(dialogue_line_index >= 0);
		current_line = dialogue_line_index;
		current_page = 0;
		auto page{ GetCurrentDialoguePage() };
		PTGN_ASSERT(page);
		auto duration{ page->properties.scroll_duration };
		text_.AddTimerScript<impl::DialogueScrollScript>(duration);
		text_.AddScript<impl::DialogueWaitScript>();
	}

	void LoadFromJson(const json& root, const DialoguePageProperties& default_properties);

	[[nodiscard]] std::vector<DialoguePage> SplitTextWithDuration(
		const std::string& full_text, const DialoguePageProperties& properties,
		const std::string& split_end, const std::string& split_begin
	) {
		// TODO: Potentially move these outside of this function.
		const int split_begin_width{
			game.font.GetSize(properties.font_key, split_begin, properties.font_size).x
		};
		const int split_end_width{
			game.font.GetSize(properties.font_key, split_end, properties.font_size).x
		};

		std::vector<DialoguePage> pages;

		const int text_area_width =
			properties.box_size.x - properties.padding_left - properties.padding_right;
		const int text_area_height =
			properties.box_size.y - properties.padding_top - properties.padding_bottom;

		if (text_area_width <= 0 || text_area_height <= 0) {
			return pages;
		}

		const int line_height =
			game.font.GetSize(properties.font_key, "Ay", properties.font_size).y;

		auto WrapTextToBox = [&](const std::string& text, int max_width, int max_lines,
								 int split_begin_width,
								 int split_end_width) -> std::vector<std::string> {
			std::istringstream word_stream(text);
			std::vector<std::string> lines;
			std::string word, current_line;

			int line_count	   = 0; // To count the number of lines wrapped
			bool is_first_line = true;
			bool is_last_line  = false;

			while (word_stream >> word) {
				std::string test_line = current_line.empty() ? word : current_line + " " + word;

				V2_int size =
					game.font.GetSize(properties.font_key, test_line, properties.font_size);

				// if (is_first_line && !pages.empty()) {
				// 	size.x += split_begin_width;
				// }

				// if (is_last_line) {
				// 	size.x += split_end_width;
				// }

				if (size.x > max_width) {
					// Handle word too long for a single line
					if (current_line.empty()) {
						std::string chunk;
						for (char ch : word) {
							chunk += ch;
							V2_int part_size =
								game.font.GetSize(properties.font_key, chunk, properties.font_size);
							if (part_size.x > max_width) {
								if (chunk.size() > 1) {
									chunk.pop_back();
									lines.push_back(chunk);
									chunk = ch;
								}
							}
						}
						current_line = chunk;
					} else {
						lines.push_back(current_line);
						current_line = word;
					}
					is_first_line = false;
				} else {
					current_line = test_line;
				}

				// Determine if it's the last line
				line_count++; // Count the number of lines wrapped
				is_last_line = (line_count >= max_lines);
			}

			if (!current_line.empty()) {
				V2_int size =
					game.font.GetSize(properties.font_key, current_line, properties.font_size);
				if (size.x <= max_width) {
					lines.push_back(current_line);
				} else {
					// Final check in case of leftover overlong word
					std::string chunk;
					for (char ch : current_line) {
						chunk += ch;
						V2_int part_size =
							game.font.GetSize(properties.font_key, chunk, properties.font_size);
						if (part_size.x > max_width) {
							if (chunk.size() > 1) {
								chunk.pop_back();
								lines.push_back(chunk);
								chunk = ch;
							}
						}
					}
					if (!chunk.empty()) {
						lines.push_back(chunk);
					}
				}
			}

			return lines;
		};

		// Split by manual newlines
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

			const int max_lines					   = text_area_height / line_height;
			std::vector<std::string> wrapped_lines = WrapTextToBox(
				segment, text_area_width, max_lines, split_begin_width, split_end_width
			);

			std::vector<std::string> page_lines;
			bool is_first_page = true;

			for (int i = 0; i < wrapped_lines.size(); ++i) {
				page_lines.push_back(wrapped_lines[i]);

				if (page_lines.size() == max_lines) {
					std::string page_text = JoinLines(page_lines);
					if (i < wrapped_lines.size() - 1) {
						page_text += split_end;
					}
					if (!is_first_page) {
						page_text = split_begin + page_text;
					}

					std::size_t total_length = 0;
					for (const auto& line : page_lines) {
						total_length += line.size();
					}

					auto duration = std::chrono::duration_cast<milliseconds>(
						ptgn::duration<double, milliseconds::period>(
							static_cast<double>(properties.scroll_duration.count())
						)
					);

					auto page_properties			= properties;
					page_properties.scroll_duration = duration;
					pages.emplace_back(DialoguePage{ page_text, page_properties });

					page_lines.clear();
					page_lines.push_back(wrapped_lines[i]);
					is_first_page = false;
				}
			}

			if (!page_lines.empty()) {
				std::string page_text = JoinLines(page_lines);
				if (!is_first_page) {
					page_text = split_begin + page_text;
				}

				std::size_t total_length = 0;
				for (const auto& line : page_lines) {
					total_length += line.size();
				}

				auto duration = std::chrono::duration_cast<milliseconds>(
					ptgn::duration<double, milliseconds::period>(
						static_cast<double>(properties.scroll_duration.count())
					)
				);

				auto page_properties			= properties;
				page_properties.scroll_duration = duration;
				pages.emplace_back(DialoguePage{ page_text, page_properties });
			}
		}

		return pages;
	}

	static std::string JoinLines(const std::vector<std::string>& lines) {
		std::string result;
		for (const auto& line : lines) {
			if (!result.empty()) {
				result += "\n";
			}
			result += line;
		}
		return result;
	}

	Text text_;

	int current_line{ 0 };
	int current_page{ 0 };
	std::string current_dialogue_;

	std::unordered_map<std::string, Dialogue> dialogues_;
};

inline void DialogueComponent::LoadFromJson(
	const json& root, const DialoguePageProperties& default_properties
) {
	const auto root_properties{ default_properties.InheritProperties(root) };
	PTGN_ASSERT(!root_properties.box_size.IsZero());
	std::string split_end{ root.value("split_end", "...") };
	std::string split_begin{ root.value("split_begin", ",,,") };
	int default_index{ root.value("index", 0) };
	PTGN_ASSERT(default_index >= 0, "Index must be greater than or equal to zero");
	DialogueBehavior behavior{ root.value("behavior", DialogueBehavior::Sequential) };
	bool repeatable{ root.value("repeatable", true) };
	bool scroll{ root.value("scroll", true) };

	PTGN_ASSERT(root.contains("dialogues"));
	const auto& dialogues_json = root.at("dialogues");

	std::string next{ root.value("next", "") };
	PTGN_ASSERT(
		next.empty() || dialogues_json.contains(next), "Next key not found in json of dialogues"
	);

	current_dialogue_ = root.value("start", "");

	PTGN_ASSERT(
		current_dialogue_.empty() || dialogues_json.contains(current_dialogue_),
		"Start key not found in json of dialogues"
	);

	for (const auto& [dialogue_name, dialogue_json] : dialogues_json.items()) {
		Dialogue dialogue;

		const auto dialogue_properties = root_properties.InheritProperties(dialogue_json);

		int index{ dialogue_json.value("index", default_index) };
		dialogue.repeatable	   = dialogue_json.value("repeatable", repeatable);
		dialogue.scroll		   = dialogue_json.value("scroll", scroll);
		dialogue.next_dialogue = dialogue_json.value("next", next);
		PTGN_ASSERT(
			dialogue.next_dialogue.empty() || dialogues_json.contains(dialogue.next_dialogue),
			"Next key not found in json of dialogues"
		);
		dialogue.behavior = dialogue_json.value("behavior", behavior);

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
					const auto line_properties = dialogue_properties.InheritProperties(line_json);

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
									line_properties.InheritProperties(page_json)
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
	if (game.input.KeyDown(Key::Enter)) {
		auto& dialogue_component{ GetDialogueComponent() };
		if (entity.HasScript<impl::DialogueScrollScript>()) {
			auto& script_info{ entity.GetTimerScriptInfo<impl::DialogueScrollScript>() };
			if (script_info.timer.IsRunning()) {
				entity.GetScript<impl::DialogueScrollScript>().UpdateText(1.0f);
				script_info.timer.Stop();
				return;
			}
		}
		dialogue_component.NextPage();
	}
}

DialogueComponent& GetDialogueComponent(Entity& entity) {
	auto dialogue_entity{ entity.GetParent() };

	PTGN_ASSERT(dialogue_entity);

	PTGN_ASSERT(dialogue_entity.Has<DialogueComponent>());

	return dialogue_entity.Get<DialogueComponent>();
}

DialogueComponent& DialogueWaitScript::GetDialogueComponent() {
	return impl::GetDialogueComponent(entity);
}

DialogueComponent& DialogueScrollScript::GetDialogueComponent() {
	return impl::GetDialogueComponent(entity);
}

void DialogueScrollScript::UpdateText(float elapsed_fraction) {
	PTGN_ASSERT(elapsed_fraction >= 0.0f && elapsed_fraction <= 1.0f);
	auto& dialogue_component{ GetDialogueComponent() };
	auto page{ dialogue_component.GetCurrentDialoguePage() };
	if (!page) {
		return;
	}
	const auto& text{ page->content };
	std::size_t char_count{ static_cast<std::size_t>(std::round(elapsed_fraction * text.size())) };
	TextContent revealed_text{ text.substr(0, char_count) };
	TextColor text_color{ page->properties.color };
	FontKey font_key{ page->properties.font_key };
	// For debugging purposes:
	/*
	std::stringstream s;
	s << entity.GetTimerScriptInfo<DialogueScrollScript>().duration << " ";
	auto duration_str{ s.str() };
	*/
	Text text_entity{ entity };
	// Do not recreate texture twice.
	text_entity.SetParameter(font_key, false);
	text_entity.SetParameter(text_color, false);
	text_entity.SetParameter(revealed_text, true);
}

bool DialogueScrollScript::OnTimerStop() {
	UpdateText(1.0f);
	return true;
}

void DialogueScrollScript::OnTimerUpdate(float elapsed_fraction) {
	UpdateText(elapsed_fraction);
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