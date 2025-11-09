#include "ui/dialogue.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <list>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/app/application.h"
#include "core/app/manager.h"
#include "core/assert.h"
#include "core/ecs/components/draw.h"
#include "core/ecs/components/generic.h"
#include "core/ecs/components/sprite.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "core/ecs/entity_hierarchy.h"
#include "core/ecs/game_object.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/scripting/script.h"
#include "core/util/file.h"
#include "core/util/span.h"
#include "debug/debug_system.h"
#include "math/math_utils.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "renderer/text/font.h"
#include "renderer/text/text.h"
#include "serialization/json/fwd.h"
#include "serialization/json/json.h"
#include "tween/tween.h"
#include "world/scene/scene.h"

namespace ptgn {

namespace impl {

static DialogueComponent& GetDialogueComponent(Entity& entity) {
	Entity dialogue_entity{ GetParent(entity) };

	PTGN_ASSERT(dialogue_entity);

	PTGN_ASSERT(dialogue_entity.Has<DialogueComponent>());

	return dialogue_entity.Get<DialogueComponent>();
}

DialogueComponent& DialogueWaitScript::GetDialogueComponent() {
	return impl::GetDialogueComponent(entity);
}

void DialogueWaitScript::OnUpdate() {
	auto& dialogue_component{ GetDialogueComponent() };
	auto continue_key{ dialogue_component.GetContinueKey() };
	if (!Application::Get().input_.KeyDown(continue_key)) {
		return;
	}
	PTGN_ASSERT(dialogue_component.tween_);
	if (dialogue_component.tween_.IsRunning()) {
		impl::DialogueScrollScript::UpdateText(dialogue_component.text_, 1.0f);
		dialogue_component.tween_.Clear();
		return;
	}
	dialogue_component.NextPage();
}

DialogueComponent& DialogueScrollScript::GetDialogueComponent() {
	return impl::GetDialogueComponent(entity);
}

void DialogueScrollScript::UpdateText(Entity& text_entity, float elapsed_fraction) {
	PTGN_ASSERT(elapsed_fraction >= 0.0f && elapsed_fraction <= 1.0f);
	auto& dialogue_component{ impl::GetDialogueComponent(text_entity) };
	auto page{ dialogue_component.GetCurrentDialoguePage() };
	if (!page) {
		return;
	}
	const auto& text{ page->content };
	std::size_t char_count{ static_cast<std::size_t>(std::round(elapsed_fraction * text.size())) };
	TextContent revealed_text{ text.substr(0, char_count) };
	TextColor text_color{ page->properties.color };
	ResourceHandle font_key{ page->properties.font_key };
	FontSize font_size{ page->properties.font_size };
	Text t{ text_entity };
	// Do not recreate texture twice.
	t.SetParameter(font_size, false);
	t.SetParameter(font_key, false);
	t.SetParameter(text_color, false);
	t.SetParameter(revealed_text, true);
}

void DialogueScrollScript::OnPointComplete() {
	Entity dialogue{ GetParent(entity) };
	Entity text{ GetChild(dialogue, "text") };
	UpdateText(text, 1.0f);
}

void DialogueScrollScript::OnProgress(float elapsed_fraction) {
	Entity dialogue{ GetParent(entity) };
	Entity text{ GetChild(dialogue, "text") };
	UpdateText(text, elapsed_fraction);
}

} // namespace impl

DialoguePageProperties DialoguePageProperties::InheritProperties(const json& j) const {
	DialoguePageProperties properties;
	properties.color		   = j.value("color", color);
	properties.scroll_duration = j.value("scroll_duration", scroll_duration);
	properties.box_size		   = j.value("box_size", box_size);
	properties.font_key		   = j.value("font_key", font_key);
	properties.padding_top	   = j.value("padding_top", padding_top);
	properties.padding_bottom  = j.value("padding_bottom", padding_bottom);
	properties.padding_left	   = j.value("padding_left", padding_left);
	properties.padding_right   = j.value("padding_right", padding_right);
	properties.font_size	   = j.value("font_size", font_size.GetValue());
	return properties;
}

void DialoguePageProperties::SetPadding(int padding) {
	padding_left = padding_right = padding_top = padding_bottom = padding;
}

void DialoguePageProperties::SetPadding(const V2_int& padding) {
	padding_left = padding_right = padding.x;
	padding_top = padding_bottom = padding.y;
}

void DialoguePageProperties::SetPadding(int top, int right, int bottom, int left) {
	padding_top	   = top;
	padding_right  = right;
	padding_bottom = bottom;
	padding_left   = left;
}

DialoguePage::DialoguePage(
	const std::string& text_content, const DialoguePageProperties& properties
) :
	content(text_content), properties(properties) {}

std::size_t Dialogue::PickRandomIndex() const {
	PTGN_ASSERT(lines.size() > used_line_indices.size());
	if (lines.size() == 1) {
		return 0;
	}
	RNG<std::size_t> index_rng(0, lines.size() - 1);
	std::size_t chosen_index{ index };
	do {
		chosen_index = index_rng();
	} while (VectorContains(used_line_indices, chosen_index));
	return chosen_index;
}

const DialogueLine* Dialogue::GetCurrentDialogueLine() const {
	PTGN_ASSERT(!lines.empty());
	PTGN_ASSERT(!used_line_indices.empty());
	std::size_t current_index = Mod(index, lines.size());
	PTGN_ASSERT(current_index < lines.size());
	PTGN_ASSERT(VectorContains(used_line_indices, current_index));
	return &lines[current_index];
}

int Dialogue::GetNewDialogueLine() {
	if (lines.empty()) {
		return -1;
	}
	if (lines.size() == used_line_indices.size()) {
		if (!repeatable) {
			return -1;
		}
		used_line_indices.clear();
		if (lines.size() > 1 && behavior == DialogueBehavior::Random) {
			used_line_indices.emplace_back(index);
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
			index		 = static_cast<std::size_t>(chosen_index);
			break;
		default: PTGN_ERROR("Unrecognized dialogue behavior");
	}
	PTGN_ASSERT(static_cast<std::size_t>(chosen_index) < lines.size());
	PTGN_ASSERT(!VectorContains(used_line_indices, static_cast<std::size_t>(chosen_index)));
	used_line_indices.emplace_back(static_cast<std::size_t>(chosen_index));
	if (lines[static_cast<std::size_t>(chosen_index)].pages.empty()) {
		return -1;
	}
	return chosen_index;
}

DialogueComponent::DialogueComponent(Entity parent, const path& json_path, Entity&& background) {
	json j = LoadJson(json_path);
	auto& scene{ parent.GetScene() };
	background_ = std::move(background);
	if (background_) {
		SetParent(background_, parent);
	}
	text_  = CreateText(scene, "", color::White);
	tween_ = CreateTween(scene);
	AddChild(parent, tween_, "tween");
	AddChild(parent, text_, "text");
	DialoguePageProperties default_properties;
	default_properties.box_size	 = background_.GetDisplaySize();
	default_properties.font_key	 = text_.GetFontKey();
	default_properties.font_size = text_.GetFontSize(false, {});
	LoadFromJson(j, default_properties);
	Close();
}

Key DialogueComponent::GetContinueKey() const {
	return continue_key_;
}

void DialogueComponent::SetContinueKey(Key continue_key) {
	continue_key_ = continue_key;
}

bool DialogueComponent::IsOpen() const {
	return IsVisible(text_);
}

void DialogueComponent::Open(const std::string& dialogue_name) {
	PTGN_ASSERT(!dialogues_.empty());
	if (!dialogue_name.empty()) {
		if (dialogue_name == current_dialogue_ && IsOpen()) {
			return;
		}
		current_dialogue_ = dialogue_name;
	} else if (IsOpen()) {
		return;
	}
	if (current_dialogue_.empty()) {
		return;
	}
	auto dialogue = GetCurrentDialogue();
	if (!dialogue) {
		return;
	}
	int dialogue_line_index = dialogue->GetNewDialogueLine();
	if (dialogue_line_index == -1) {
		return;
	}
	StartDialogueLine(dialogue_line_index);
	Show(text_);
	if (background_) {
		Show(background_);
	}
}

void DialogueComponent::Close() {
	Hide(text_);
	if (background_) {
		Hide(background_);
	}
	tween_.Clear();
	RemoveScripts<impl::DialogueWaitScript>(text_);
	current_line_ = 0;
	current_page_ = 0;
}

void DialogueComponent::IncrementPage() {
	current_page_++;
}

void DialogueComponent::NextPage() {
	IncrementPage();
	auto page{ GetCurrentDialoguePage() };
	if (!page) {
		Close();
		return;
	}
	auto duration{ page->properties.scroll_duration };

	tween_.Clear();
	tween_.During(duration).AddScript<impl::DialogueScrollScript>().Start();
}

void DialogueComponent::SetNextDialogue() {
	auto dialogue = GetCurrentDialogue();
	if (!dialogue) {
		current_line_ = 0;
		current_page_ = 0;
		return;
	}
	const auto& next_dialogue{ dialogue->next_dialogue };
	PTGN_ASSERT(next_dialogue.empty() || dialogues_.contains(next_dialogue));
	if (next_dialogue.empty()) {
		current_line_ = 0;
		current_page_ = 0;
	}
	current_dialogue_ = next_dialogue;
}

void DialogueComponent::SetDialogue(const std::string& name) {
	PTGN_ASSERT(name.empty() || dialogues_.contains(name));
	current_dialogue_ = name;
	current_line_	  = 0;
	current_page_	  = 0;
}

const std::unordered_map<std::string, Dialogue>& DialogueComponent::GetDialogues() const {
	return dialogues_;
}

Dialogue* DialogueComponent::GetCurrentDialogue() {
	if (current_dialogue_.empty()) {
		return nullptr;
	}
	auto it{ dialogues_.find(current_dialogue_) };
	if (it == dialogues_.end()) {
		return nullptr;
	}
	return &it->second;
}

DialogueLine* DialogueComponent::GetCurrentDialogueLine() {
	if (current_line_ < 0) {
		return nullptr;
	}
	auto dialogue = GetCurrentDialogue();
	if (!dialogue || static_cast<std::size_t>(current_line_) >= dialogue->lines.size()) {
		return nullptr;
	}
	return &dialogue->lines[static_cast<std::size_t>(current_line_)];
}

DialoguePage* DialogueComponent::GetCurrentDialoguePage() {
	if (current_page_ < 0) {
		return nullptr;
	}
	auto line = GetCurrentDialogueLine();
	if (!line || static_cast<std::size_t>(current_page_) >= line->pages.size()) {
		return nullptr;
	}
	return &line->pages[static_cast<std::size_t>(current_page_)];
}

void DialogueComponent::DrawInfo(const V2_float& position) {
	FontSize font_size{ 32 };
	Application::Get().debug_.DrawText(
		"Dialogue: " + current_dialogue_, position + V2_float{ 0, 0 }, color::White,
		Origin::TopLeft, font_size
	);
	Application::Get().debug_.DrawText(
		"Line: " + std::to_string(current_line_), position + V2_float{ 0, 50 }, color::White,
		Origin::TopLeft, font_size
	);
	Application::Get().debug_.DrawText(
		"Page: " + std::to_string(current_page_), position + V2_float{ 0, 100 }, color::White,
		Origin::TopLeft, font_size
	);
}

void DialogueComponent::AlignToTopLeft(const DialoguePageProperties& default_properties) {
	SetPosition(
		text_, V2_int{ default_properties.padding_left, default_properties.padding_top } -
				   default_properties.box_size / 2.0f
	);
	SetDrawOrigin(text_, Origin::TopLeft);
}

void DialogueComponent::StartDialogueLine(int dialogue_line_index) {
	PTGN_ASSERT(dialogue_line_index >= 0);
	current_line_ = dialogue_line_index;
	current_page_ = 0;
	auto page	  = GetCurrentDialoguePage();
	PTGN_ASSERT(page);
	auto duration = page->properties.scroll_duration;
	tween_.Clear();
	tween_.During(duration).AddScript<impl::DialogueScrollScript>().Start();
	AddScript<impl::DialogueWaitScript>(text_);
}

void DialogueComponent::LoadFromJson(
	const json& root, const DialoguePageProperties& default_properties
) {
	const auto root_properties{ default_properties.InheritProperties(root) };
	AlignToTopLeft(root_properties);
	PTGN_ASSERT(
		!root_properties.box_size.IsZero(),
		"Dialogue requires either a sprite background or a non-zero json defined box size"
	);
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

	for (auto it = dialogues_json.begin(); it != dialogues_json.end(); ++it) {
		const std::string& dialogue_name = it.key();
		const auto& dialogue_json		 = it.value();

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

std::string DialogueComponent::JoinLines(const std::vector<std::string>& lines) {
	std::string result;
	for (const auto& line : lines) {
		if (!result.empty()) {
			result += "\n";
		}
		result += line;
	}
	return result;
}

std::vector<DialoguePage> DialogueComponent::SplitTextWithDuration(
	const std::string& full_text, const DialoguePageProperties& properties,
	const std::string& split_end, const std::string& split_begin
) {
	// TODO: Potentially move these outside of this function.
	const int split_begin_width{
		Application::Get().font.GetSize(properties.font_key, split_begin, properties.font_size).x
	};
	const int split_end_width{
		Application::Get().font.GetSize(properties.font_key, split_end, properties.font_size).x
	};

	std::vector<DialoguePage> pages;

	const int text_area_width = static_cast<int>(properties.box_size.x) - properties.padding_left -
								properties.padding_right;
	const int text_area_height = static_cast<int>(properties.box_size.y) - properties.padding_top -
								 properties.padding_bottom;

	if (text_area_width <= 0 || text_area_height <= 0) {
		return pages;
	}

	const int line_height =
		Application::Get().font.GetSize(properties.font_key, "Ay", properties.font_size).y;

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

			V2_int size{ Application::Get().font.GetSize(
				properties.font_key, test_line, properties.font_size
			) };

			if (is_first_line) {
				size.x += split_begin_width; // Add split_begin width on the first line
			}

			if (is_last_line) {
				size.x += split_end_width; // Add split_end width on the last line
			}

			if (size.x > max_width) {
				// Handle word too long for a single line
				if (current_line.empty()) {
					std::string chunk;
					for (char ch : word) {
						chunk			 += ch;
						V2_int part_size  = Application::Get().font.GetSize(
							 properties.font_key, chunk, properties.font_size
						 );
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
			V2_int size = Application::Get().font.GetSize(
				properties.font_key, current_line, properties.font_size
			);
			if (size.x <= max_width) {
				lines.push_back(current_line);
			} else {
				// Final check in case of leftover overlong word
				std::string chunk;
				for (char ch : current_line) {
					chunk			 += ch;
					V2_int part_size  = Application::Get().font.GetSize(
						 properties.font_key, chunk, properties.font_size
					 );
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
	std::size_t start{ 0 };
	std::size_t newline_pos{ 0 };

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

		const int max_lines = std::max(1, text_area_height / line_height);
		std::vector<std::string> wrapped_lines =
			WrapTextToBox(segment, text_area_width, max_lines, split_begin_width, split_end_width);

		std::vector<std::string> page_lines;
		bool is_first_page = true;

		for (std::size_t i = 0; i < wrapped_lines.size(); ++i) {
			page_lines.push_back(wrapped_lines[i]);

			if (page_lines.size() == static_cast<std::size_t>(max_lines)) {
				std::string page_text{ JoinLines(page_lines) };
				if (i < wrapped_lines.size() - 1) {
					page_text += split_end;
				}
				if (!is_first_page) {
					page_text = split_begin + page_text;
				}

				auto page_properties{ properties };
				pages.emplace_back(page_text, page_properties);

				// Clear page_lines to start a new page for the next chunk of text
				page_lines.clear();
				is_first_page = false;
			}
		}

		// Add the remaining lines as the last page
		if (!page_lines.empty()) {
			std::string page_text = JoinLines(page_lines);
			if (!is_first_page) {
				page_text = split_begin + page_text;
			}

			auto page_properties{ properties };
			pages.emplace_back(page_text, page_properties);
		}
	}

	return pages;
}

} // namespace ptgn