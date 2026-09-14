#pragma once

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cfloat>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "editor/editor_context.h"
#include "panels/inspector_fields.h"
#include "panels/rich_text_editor.h"
#include "runtime/graphics/text/text.h"
#include "runtime/ui/dialogue.h"
#include "serialization/json/json.h"

namespace ptgn::editor {

namespace dialogue_editor_demo_detail {

using inspector::DrawRichTextEditor;
using inspector::RichTextEditorOptions;
using inspector::RichTextEditorSelection;

struct DialogueEditorPageDraft {
	RichText text{};
	RichTextEditorSelection selection{};
};

struct DialogueEditorLineDraft {
	std::vector<DialogueEditorPageDraft> pages{};
};

struct DialogueEditorEntryDraft {
	std::string name{};
	int index{ 0 };
	bool repeatable{ true };
	DialogueBehavior behavior{ DialogueBehavior::Sequential };
	bool scroll{ true };
	std::string next{};

	std::vector<DialogueEditorLineDraft> lines{};
};

struct DialogueEditorDocument {
	Key continue_key{ Key::Enter };
	std::string start{};

	// Root-level defaults inherited by newly created pages and serialized at the dialogue root.
	DialoguePageProperties defaults{};

	std::vector<DialogueEditorEntryDraft> dialogues{};
};

struct DialogueEditorState {
	DialogueEditorDocument document{};

	std::size_t selected_dialogue{ 0 };
	std::size_t selected_line{ 0 };
	std::size_t selected_page{ 0 };

	std::string rename_buffer{};
	bool rename_requested{ false };

	bool show_json{ false };
	std::string generated_json{};
};

struct OpenRichTag {
	std::string name{};
	std::string open_source{};
};

[[nodiscard]] inline std::string_view Trim(std::string_view value) {
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
		value.remove_prefix(1);
	}
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
		value.remove_suffix(1);
	}
	return value;
}

[[nodiscard]] inline bool IsEscaped(std::string_view source, std::size_t position) {
	std::size_t backslashes{ 0 };

	while (position > 0 && source[position - 1] == '\\') {
		--position;
		++backslashes;
	}

	return (backslashes % 2) != 0;
}

[[nodiscard]] inline std::string RichTagName(std::string_view token) {
	token = Trim(token);

	if (token.starts_with('/')) {
		token.remove_prefix(1);
		token = Trim(token);
	}

	auto equals{ token.find('=') };
	auto name{ Trim(token.substr(0, equals)) };

	std::string result{ name };
	std::ranges::transform(result, result.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return result;
}

[[nodiscard]] inline std::optional<std::vector<OpenRichTag>> GetOpenTagsAt(
	std::string_view source,
	std::size_t cursor
) {
	cursor = std::min(cursor, source.size());

	std::vector<OpenRichTag> stack;

	for (std::size_t i{ 0 }; i < cursor;) {
		if (source[i] != '<' || IsEscaped(source, i)) {
			++i;
			continue;
		}

		auto close{ source.find('>', i + 1) };

		if (close == std::string_view::npos) {
			// The cursor is inside malformed/incomplete markup. Do not split here.
			return std::nullopt;
		}

		if (cursor <= close) {
			return std::nullopt;
		}

		auto token{ Trim(source.substr(i + 1, close - i - 1)) };

		if (token.empty()) {
			i = close + 1;
			continue;
		}

		const bool closing{ token.starts_with('/') };
		const std::string name{ RichTagName(token) };

		if (name.empty()) {
			i = close + 1;
			continue;
		}

		if (closing) {
			if (!stack.empty()) {
				auto it{ std::ranges::find_if(
					stack.rbegin(),
					stack.rend(),
					[&](const OpenRichTag& tag) { return tag.name == name; }
				) };

				if (it != stack.rend()) {
					stack.erase(std::next(it).base(), stack.end());
				}
			}
		} else {
			stack.emplace_back(
				OpenRichTag{
					.name = name,
					.open_source = std::string{ source.substr(i, close - i + 1) },
				}
			);
		}

		i = close + 1;
	}

	return stack;
}

[[nodiscard]] inline bool SplitRichTextSource(
	std::string_view source,
	std::size_t cursor,
	std::string& left,
	std::string& right
) {
	cursor = std::min(cursor, source.size());

	if (cursor == 0 || cursor == source.size()) {
		return false;
	}

	auto open_tags{ GetOpenTagsAt(source, cursor) };

	if (!open_tags.has_value()) {
		return false;
	}

	left = std::string{ source.substr(0, cursor) };
	right = std::string{ source.substr(cursor) };

	// Preserve active formatting across the page boundary. This turns:
	// <b>Hello |world</b>
	// into:
	// <b>Hello </b>   +   <b>world</b>
	for (auto it{ open_tags->rbegin() }; it != open_tags->rend(); ++it) {
		left += "</" + it->name + ">";
	}

	std::string reopen;
	for (const auto& tag : open_tags.value()) {
		reopen += tag.open_source;
	}

	right.insert(0, reopen);
	return true;
}

[[nodiscard]] inline bool DialogueNameExists(
	const DialogueEditorDocument& document,
	std::string_view name,
	std::size_t ignore = std::string::npos
) {
	for (std::size_t i{ 0 }; i < document.dialogues.size(); ++i) {
		if (i != ignore && document.dialogues[i].name == name) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] inline std::string MakeUniqueDialogueName(const DialogueEditorDocument& document) {
	if (!DialogueNameExists(document, "dialogue")) {
		return "dialogue";
	}

	for (std::size_t suffix{ 2 };; ++suffix) {
		std::string candidate{ "dialogue_" + std::to_string(suffix) };

		if (!DialogueNameExists(document, candidate)) {
			return candidate;
		}
	}
}

inline void RenameDialogue(
	DialogueEditorDocument& document,
	std::size_t dialogue_index,
	std::string new_name
) {
	if (dialogue_index >= document.dialogues.size()) {
		return;
	}

	const std::string old_name{ document.dialogues[dialogue_index].name };

	if (old_name == new_name) {
		return;
	}

	document.dialogues[dialogue_index].name = new_name;

	if (document.start == old_name) {
		document.start = new_name;
	}

	for (auto& dialogue : document.dialogues) {
		if (dialogue.next == old_name) {
			dialogue.next = new_name;
		}
	}
}

inline void AppendRichTextPage(
	DialogueEditorPageDraft& destination,
	const DialogueEditorPageDraft& source
) {
	// Rebase the appended page onto the destination defaults so joining pages does not
	// silently change the appearance of spans that relied on the second page's defaults.
	const StyledText resolved{ ParseRichText(source.text).text };
	const std::string rebased{
		SerializeStyledTextToRichText(resolved, destination.text.defaults)
	};

	if (!destination.text.source.empty() && !rebased.empty()) {
		destination.text.source += '\n';
	}

	destination.text.source += rebased;
	destination.selection = {};
}

[[nodiscard]] inline DialogueEditorPageDraft MakePage(const DialogueEditorDocument& document) {
	DialogueEditorPageDraft page;
	page.text.defaults = document.defaults.text_defaults;
	return page;
}

[[nodiscard]] inline DialogueEditorLineDraft MakeLine(const DialogueEditorDocument& document) {
	DialogueEditorLineDraft line;
	line.pages.emplace_back(MakePage(document));
	return line;
}

[[nodiscard]] inline json SerializePage(
	const DialogueEditorDocument& document,
	const DialogueEditorPageDraft& page
) {
	if (page.text.defaults == document.defaults.text_defaults) {
		return page.text.source;
	}

	return json{
		{
			"text",
			json{
				{ "source", page.text.source },
				{ "defaults", page.text.defaults },
			}
		},
	};
}

[[nodiscard]] inline json BuildDialogueJson(const DialogueEditorDocument& document) {
	json root = document.defaults;

	root["continue_key"] = document.continue_key;
	root["start"] = document.start;
	root["dialogues"] = json::object();

	for (const auto& dialogue : document.dialogues) {
		json entry{
			{ "index", dialogue.index },
			{ "repeatable", dialogue.repeatable },
			{ "behavior", dialogue.behavior },
			{ "scroll", dialogue.scroll },
			{ "next", dialogue.next },
			{ "lines", json::array() },
		};

		for (const auto& line : dialogue.lines) {
			if (line.pages.size() == 1 &&
				line.pages.front().text.defaults == document.defaults.text_defaults) {
				entry["lines"].push_back(line.pages.front().text.source);
				continue;
			}

			json pages{ json::array() };

			for (const auto& page : line.pages) {
				pages.push_back(SerializePage(document, page));
			}

			entry["lines"].push_back(
				json{
					{ "pages", std::move(pages) },
				}
			);
		}

		root["dialogues"][dialogue.name] = std::move(entry);
	}

	return root;
}

[[nodiscard]] inline DialogueEditorState MakeDemoState() {
	DialogueEditorState state;

	state.document.start = "intro";
	state.document.defaults.box_size = { 600.0f, 160.0f };
	state.document.defaults.text_defaults.style.size = 24.0f;

	DialogueEditorEntryDraft intro{
		.name = "intro",
		.index = 0,
		.repeatable = false,
		.behavior = DialogueBehavior::Sequential,
		.scroll = true,
		.next = "outro",
	};

	DialogueEditorLineDraft intro_line;
	intro_line.pages.emplace_back(MakePage(state.document));
	intro_line.pages.back().text.source =
		"Hey. <b>You made it.</b>\n\n"
		"I wasn't sure you'd actually come.";

	intro_line.pages.emplace_back(MakePage(state.document));
	intro_line.pages.back().text.source =
		"There's something I need to show you before we leave.\n\n"
		"<i>Try not to freak out.</i>";

	intro.lines.emplace_back(std::move(intro_line));

	auto alternate{ MakeLine(state.document) };
	alternate.pages.front().text.source =
		"Oh, good. You're here.\n\nCome on, we're already late.";
	intro.lines.emplace_back(std::move(alternate));

	DialogueEditorEntryDraft outro{
		.name = "outro",
		.index = 0,
		.repeatable = true,
		.behavior = DialogueBehavior::Sequential,
		.scroll = true,
		.next = "epilogue",
	};

	auto outro_line{ MakeLine(state.document) };
	outro_line.pages.front().text.source = "That's it, then.";
	outro_line.pages.emplace_back(MakePage(state.document));
	outro_line.pages.back().text.source =
		"Whatever happens next, <b>we do it together.</b>";
	outro.lines.emplace_back(std::move(outro_line));

	DialogueEditorEntryDraft epilogue{
		.name = "epilogue",
		.index = 0,
		.repeatable = true,
		.behavior = DialogueBehavior::Sequential,
		.scroll = true,
		.next = "",
	};

	auto epilogue_line{ MakeLine(state.document) };
	epilogue_line.pages.front().text.source = "The end.";
	epilogue.lines.emplace_back(std::move(epilogue_line));

	state.document.dialogues = {
		std::move(intro),
		std::move(outro),
		std::move(epilogue),
	};

	return state;
}

inline void ClampSelection(DialogueEditorState& state) {
	if (state.document.dialogues.empty()) {
		return;
	}

	state.selected_dialogue = std::min(
		state.selected_dialogue,
		state.document.dialogues.size() - 1
	);

	auto& dialogue{ state.document.dialogues[state.selected_dialogue] };

	if (dialogue.lines.empty()) {
		dialogue.lines.emplace_back(MakeLine(state.document));
	}

	state.selected_line = std::min(state.selected_line, dialogue.lines.size() - 1);

	auto& line{ dialogue.lines[state.selected_line] };

	if (line.pages.empty()) {
		line.pages.emplace_back(MakePage(state.document));
	}

	state.selected_page = std::min(state.selected_page, line.pages.size() - 1);
}

inline void DrawRenamePopup(DialogueEditorState& state) {
	if (state.rename_requested) {
		ImGui::OpenPopup("Rename Dialogue");
		state.rename_requested = false;
	}

	if (!ImGui::BeginPopupModal(
			"Rename Dialogue",
			nullptr,
			ImGuiWindowFlags_AlwaysAutoResize
		)) {
		return;
	}

	if (ImGui::IsWindowAppearing()) {
		ImGui::SetKeyboardFocusHere();
	}

	ImGui::SetNextItemWidth(300.0f);
	ImGui::InputText("##DialogueName", &state.rename_buffer);

	const bool valid{
		!state.rename_buffer.empty() &&
		!DialogueNameExists(
			state.document,
			state.rename_buffer,
			state.selected_dialogue
		)
	};

	if (!valid) {
		ImGui::TextDisabled(
			state.rename_buffer.empty()
				? "Name cannot be empty."
				: "That dialogue name already exists."
		);
	}

	{
		const bool disable{ !valid };
		if (disable) {
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Rename")) {
			RenameDialogue(
				state.document,
				state.selected_dialogue,
				state.rename_buffer
			);
			ImGui::CloseCurrentPopup();
		}

		if (disable) {
			ImGui::EndDisabled();
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Cancel")) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

inline void DrawDialogueList(DialogueEditorState& state) {
	auto& document{ state.document };

	ImGui::BeginChild(
		"##DialogueList",
		ImVec2{ 190.0f, 0.0f },
		ImGuiChildFlags_Borders
	);

	ImGui::TextUnformatted("Dialogues");
	ImGui::Separator();

	if (ImGui::Button("+ Dialogue", ImVec2{ -FLT_MIN, 0.0f })) {
		const std::string name{ MakeUniqueDialogueName(document) };

		DialogueEditorEntryDraft dialogue{
			.name = name,
			.lines = {
				MakeLine(document),
			},
		};

		document.dialogues.emplace_back(std::move(dialogue));
		state.selected_dialogue = document.dialogues.size() - 1;
		state.selected_line = 0;
		state.selected_page = 0;

		if (document.start.empty()) {
			document.start = name;
		}
	}

	ImGui::Spacing();

	for (std::size_t i{ 0 }; i < document.dialogues.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));

		const bool selected{ i == state.selected_dialogue };

		if (ImGui::Selectable(document.dialogues[i].name.c_str(), selected)) {
			state.selected_dialogue = i;
			state.selected_line = 0;
			state.selected_page = 0;
		}

		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Rename")) {
				state.selected_dialogue = i;
				state.rename_buffer = document.dialogues[i].name;
				state.rename_requested = true;
			}

			if (ImGui::MenuItem("Duplicate")) {
				auto duplicate{ document.dialogues[i] };
				duplicate.name = MakeUniqueDialogueName(document);

				document.dialogues.insert(
					document.dialogues.begin() +
						static_cast<std::ptrdiff_t>(i + 1),
					std::move(duplicate)
				);

				state.selected_dialogue = i + 1;
				state.selected_line = 0;
				state.selected_page = 0;
			}

			if (ImGui::MenuItem(
					"Delete",
					nullptr,
					false,
					document.dialogues.size() > 1
				)) {
				const std::string deleted_name{ document.dialogues[i].name };

				document.dialogues.erase(
					document.dialogues.begin() +
						static_cast<std::ptrdiff_t>(i)
				);

				if (document.start == deleted_name) {
					document.start = document.dialogues.front().name;
				}

				for (auto& dialogue : document.dialogues) {
					if (dialogue.next == deleted_name) {
						dialogue.next.clear();
					}
				}

				state.selected_dialogue = std::min(
					state.selected_dialogue,
					document.dialogues.size() - 1
				);
				state.selected_line = 0;
				state.selected_page = 0;
			}

			ImGui::EndPopup();
		}

		ImGui::PopID();
	}

	ImGui::EndChild();
}

inline void DrawLineTabs(
	DialogueEditorState& state,
	DialogueEditorEntryDraft& dialogue
) {
	ImGui::TextUnformatted("Variants");
	ImGui::SameLine();

	if (ImGui::SmallButton("+")) {
		dialogue.lines.emplace_back(MakeLine(state.document));
		state.selected_line = dialogue.lines.size() - 1;
		state.selected_page = 0;
	}

	ImGui::SameLine();

	if (ImGui::SmallButton("Duplicate")) {
		dialogue.lines.insert(
			dialogue.lines.begin() +
				static_cast<std::ptrdiff_t>(state.selected_line + 1),
			dialogue.lines[state.selected_line]
		);
		++state.selected_line;
		state.selected_page = 0;
	}

	ImGui::SameLine();

	const bool can_delete_line{ dialogue.lines.size() > 1 };
	if (!can_delete_line) {
		ImGui::BeginDisabled();
	}

	if (ImGui::SmallButton("Delete")) {
		dialogue.lines.erase(
			dialogue.lines.begin() +
				static_cast<std::ptrdiff_t>(state.selected_line)
		);

		state.selected_line = std::min(
			state.selected_line,
			dialogue.lines.size() - 1
		);
		state.selected_page = 0;
	}

	if (!can_delete_line) {
		ImGui::EndDisabled();
	}

	ImGui::Spacing();

	if (ImGui::BeginTabBar(
			"##DialogueVariants",
			ImGuiTabBarFlags_AutoSelectNewTabs |
				ImGuiTabBarFlags_FittingPolicyScroll
		)) {
		for (std::size_t i{ 0 }; i < dialogue.lines.size(); ++i) {
			const std::string label{
				"Line " + std::to_string(i + 1) + "###DialogueLine_" + std::to_string(i)
			};

			if (ImGui::BeginTabItem(label.c_str())) {
				if (state.selected_line != i) {
					state.selected_line = i;
					state.selected_page = 0;
				}
				ImGui::EndTabItem();
			}
		}

		ImGui::EndTabBar();
	}
}

inline void DrawPageTabs(
	DialogueEditorState& state,
	DialogueEditorLineDraft& line
) {
	ImGui::TextUnformatted("Pages");
	ImGui::SameLine();

	if (ImGui::SmallButton("+ Page")) {
		auto page{ MakePage(state.document) };

		if (!line.pages.empty()) {
			page.text.defaults = line.pages[state.selected_page].text.defaults;
		}

		line.pages.insert(
			line.pages.begin() +
				static_cast<std::ptrdiff_t>(state.selected_page + 1),
			std::move(page)
		);

		++state.selected_page;
	}

	ImGui::SameLine();

	if (ImGui::SmallButton("Duplicate Page")) {
		line.pages.insert(
			line.pages.begin() +
				static_cast<std::ptrdiff_t>(state.selected_page + 1),
			line.pages[state.selected_page]
		);
		++state.selected_page;
	}

	ImGui::SameLine();

	const bool can_move_left{ state.selected_page > 0 };
	if (!can_move_left) {
		ImGui::BeginDisabled();
	}

	if (ImGui::SmallButton("<")) {
		std::swap(
			line.pages[state.selected_page],
			line.pages[state.selected_page - 1]
		);
		--state.selected_page;
	}

	if (!can_move_left) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	const bool can_move_right{ state.selected_page + 1 < line.pages.size() };
	if (!can_move_right) {
		ImGui::BeginDisabled();
	}

	if (ImGui::SmallButton(">")) {
		std::swap(
			line.pages[state.selected_page],
			line.pages[state.selected_page + 1]
		);
		++state.selected_page;
	}

	if (!can_move_right) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	const bool can_delete_page{ line.pages.size() > 1 };
	if (!can_delete_page) {
		ImGui::BeginDisabled();
	}

	if (ImGui::SmallButton("Delete Page")) {
		line.pages.erase(
			line.pages.begin() +
				static_cast<std::ptrdiff_t>(state.selected_page)
		);

		state.selected_page = std::min(
			state.selected_page,
			line.pages.size() - 1
		);
	}

	if (!can_delete_page) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	const bool can_join_previous{ state.selected_page > 0 };
	if (!can_join_previous) {
		ImGui::BeginDisabled();
	}

	if (ImGui::SmallButton("Join Previous")) {
		auto& previous{ line.pages[state.selected_page - 1] };
		AppendRichTextPage(previous, line.pages[state.selected_page]);
		line.pages.erase(
			line.pages.begin() + static_cast<std::ptrdiff_t>(state.selected_page)
		);
		--state.selected_page;
	}

	if (!can_join_previous) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	const bool can_join_next{ state.selected_page + 1 < line.pages.size() };
	if (!can_join_next) {
		ImGui::BeginDisabled();
	}

	if (ImGui::SmallButton("Join Next")) {
		AppendRichTextPage(
			line.pages[state.selected_page],
			line.pages[state.selected_page + 1]
		);
		line.pages.erase(
			line.pages.begin() + static_cast<std::ptrdiff_t>(state.selected_page + 1)
		);
	}

	if (!can_join_next) {
		ImGui::EndDisabled();
	}

	ImGui::Spacing();

	if (ImGui::BeginTabBar(
			"##DialoguePages",
			ImGuiTabBarFlags_AutoSelectNewTabs |
				ImGuiTabBarFlags_FittingPolicyScroll
		)) {
		for (std::size_t i{ 0 }; i < line.pages.size(); ++i) {
			const std::string label{
				"Page " + std::to_string(i + 1) + "###DialoguePage_" + std::to_string(i)
			};

			if (ImGui::BeginTabItem(label.c_str())) {
				state.selected_page = i;
				ImGui::EndTabItem();
			}
		}

		ImGui::EndTabBar();
	}
}

inline void DrawCurrentPageEditor(
	EditorContext& ctx,
	DialogueEditorState& state,
	DialogueEditorLineDraft& line
) {
	auto& page{ line.pages[state.selected_page] };
	const TextRunDefaults page_defaults{ page.text.defaults };

	ImGui::PushID(static_cast<int>(state.selected_dialogue));
	ImGui::PushID(static_cast<int>(state.selected_line));
	ImGui::PushID(static_cast<int>(state.selected_page));

	DrawRichTextEditor(
		ctx,
		page.text.source,
		page.text.defaults,
		RichTextEditorOptions{
			.show_preview = true,
			.line_count = 10,
			.selection = &page.selection,
		}
	);

	const std::size_t split_position{
		std::min(page.selection.cursor, page.text.source.size())
	};

	std::string split_left;
	std::string split_right;
	const bool can_split{
		SplitRichTextSource(
			page.text.source,
			split_position,
			split_left,
			split_right
		)
	};

	if (!can_split) {
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("Split Page at Cursor")) {
		DialogueEditorPageDraft second{ page };
		page.text.source = std::move(split_left);
		second.text.source = std::move(split_right);
		page.selection = {};
		second.selection = {};

		line.pages.insert(
			line.pages.begin() +
				static_cast<std::ptrdiff_t>(state.selected_page + 1),
			std::move(second)
		);

		++state.selected_page;
	}

	if (!can_split) {
		ImGui::EndDisabled();

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip(
				"Place the cursor somewhere inside the page source.\n"
				"Splitting inside an incomplete <tag> is not allowed."
			);
		}
	} else if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Split this page at the rich-text cursor.\n"
			"Open formatting tags are automatically closed and reopened across the new page."
		);
	}

	ImGui::SameLine();

	if (ImGui::Button("Use Defaults for New Pages")) {
		state.document.defaults.text_defaults = page_defaults;
	}

	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Make this page's rich-text Defaults the root defaults used by newly created pages."
		);
	}

	ImGui::PopID();
	ImGui::PopID();
	ImGui::PopID();
}

inline void DrawEntryProperties(
	EditorContext& ctx,
	DialogueEditorState& state,
	DialogueEditorEntryDraft& dialogue
) {
	ImGui::BeginChild(
		"##DialogueProperties",
		ImVec2{ 270.0f, 0.0f },
		ImGuiChildFlags_Borders
	);

	ImGui::TextUnformatted("Dialogue");
	ImGui::Separator();

	if (ImGui::Button("Rename", ImVec2{ -FLT_MIN, 0.0f })) {
		state.rename_buffer = dialogue.name;
		state.rename_requested = true;
	}

	ImGui::Spacing();

	ImGui::TextUnformatted("Start Dialogue");
	ImGui::SetNextItemWidth(-FLT_MIN);

	if (ImGui::BeginCombo("##StartDialogue", state.document.start.c_str())) {
		for (const auto& candidate : state.document.dialogues) {
			const bool selected{ state.document.start == candidate.name };

			if (ImGui::Selectable(candidate.name.c_str(), selected)) {
				state.document.start = candidate.name;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::Spacing();
	ImGui::TextUnformatted("Next Dialogue");
	ImGui::SetNextItemWidth(-FLT_MIN);

	const char* next_preview{
		dialogue.next.empty()
			? "(none)"
			: dialogue.next.c_str()
	};

	if (ImGui::BeginCombo("##NextDialogue", next_preview)) {
		if (ImGui::Selectable("(none)", dialogue.next.empty())) {
			dialogue.next.clear();
		}

		for (const auto& candidate : state.document.dialogues) {
			const bool selected{ dialogue.next == candidate.name };

			if (ImGui::Selectable(candidate.name.c_str(), selected)) {
				dialogue.next = candidate.name;
			}
		}

		ImGui::EndCombo();
	}

	ImGui::Spacing();

	ImGui::TextUnformatted("Behavior");
	ImGui::SetNextItemWidth(-FLT_MIN);

	const char* behavior_preview{
		dialogue.behavior == DialogueBehavior::Sequential
			? "Sequential"
			: "Random"
	};

	if (ImGui::BeginCombo("##DialogueBehavior", behavior_preview)) {
		if (ImGui::Selectable(
				"Sequential",
				dialogue.behavior == DialogueBehavior::Sequential
			)) {
			dialogue.behavior = DialogueBehavior::Sequential;
		}

		if (ImGui::Selectable(
				"Random",
				dialogue.behavior == DialogueBehavior::Random
			)) {
			dialogue.behavior = DialogueBehavior::Random;
		}

		ImGui::EndCombo();
	}

	ImGui::Checkbox("Repeatable", &dialogue.repeatable);
	ImGui::Checkbox("Scroll Text", &dialogue.scroll);

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputInt("Initial Line", &dialogue.index);
	dialogue.index = std::max(dialogue.index, 0);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (ImGui::TreeNodeEx(
			"Root Dialogue Defaults",
			ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_FramePadding
		)) {
		inspector::DrawValue(ctx, "Continue Key", state.document.continue_key);
		inspector::DrawValue(ctx, "Box Size", state.document.defaults.box_size);
		inspector::DrawValue(ctx, "Padding", state.document.defaults.padding);
		inspector::DrawValue(
			ctx,
			"Scroll Duration",
			state.document.defaults.scroll_duration
		);
		inspector::DrawValue(
			ctx,
			"Horizontal Align",
			state.document.defaults.horizontal_align
		);
		inspector::DrawValue(
			ctx,
			"Vertical Align",
			state.document.defaults.vertical_align
		);
		inspector::DrawValue(
			ctx,
			"Wrap Mode",
			state.document.defaults.wrap_mode
		);
		inspector::DrawValue(
			ctx,
			"Overflow Mode",
			state.document.defaults.overflow_mode
		);

		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::TextWrapped(
		"Each page is authored as RichText. The runtime receives resolved StyledText plus the "
		"page's TextRunDefaults, matching the normal text component architecture."
	);

	ImGui::EndChild();
}

inline void DrawDialogueEditorBody(EditorContext& ctx, DialogueEditorState& state) {
	ClampSelection(state);

	DrawDialogueList(state);
	ImGui::SameLine();

	ImGui::BeginGroup();

	const float property_width{ 270.0f };
	const float editor_width{
		std::max(
			360.0f,
			ImGui::GetContentRegionAvail().x -
				property_width -
				ImGui::GetStyle().ItemSpacing.x
		)
	};

	ImGui::BeginChild(
		"##DialogueEditorCenter",
		ImVec2{ editor_width, 0.0f },
		ImGuiChildFlags_Borders
	);

	auto& dialogue{
		state.document.dialogues[state.selected_dialogue]
	};

	DrawLineTabs(state, dialogue);
	ClampSelection(state);

	auto& line{ dialogue.lines[state.selected_line] };

	ImGui::Separator();
	DrawPageTabs(state, line);
	ClampSelection(state);

	ImGui::SeparatorText("Rich Text");
	DrawCurrentPageEditor(ctx, state, line);

	ImGui::EndChild();

	ImGui::SameLine();

	DrawEntryProperties(ctx, state, dialogue);

	ImGui::EndGroup();

	DrawRenamePopup(state);
}

inline void DrawJsonPreview(DialogueEditorState& state) {
	if (!state.show_json) {
		return;
	}

	ImGui::SetNextWindowSize(
		ImVec2{ 720.0f, 600.0f },
		ImGuiCond_FirstUseEver
	);

	if (!ImGui::Begin("Dialogue JSON###DialogueJsonPreview", &state.show_json)) {
		ImGui::End();
		return;
	}

	state.generated_json = BuildDialogueJson(state.document).dump(2);

	if (ImGui::Button("Copy JSON")) {
		ImGui::SetClipboardText(state.generated_json.c_str());
	}

	ImGui::SameLine();

	ImGui::TextDisabled(
		"%zu bytes",
		state.generated_json.size()
	);

	ImGui::Separator();

	ImGui::InputTextMultiline(
		"##DialogueJson",
		&state.generated_json,
		ImVec2{ -FLT_MIN, -FLT_MIN },
		ImGuiInputTextFlags_ReadOnly
	);

	ImGui::End();
}

} // namespace dialogue_editor_demo_detail

inline void DrawDialogueEditorDemoWindow(EditorContext& ctx, bool* open = nullptr) {
	using namespace dialogue_editor_demo_detail;
	static DialogueEditorState state{ MakeDemoState() };

	if (open && !*open) {
		return;
	}

	ImGui::SetNextWindowSize(
		ImVec2{ 1280.0f, 780.0f },
		ImGuiCond_FirstUseEver
	);

	if (!ImGui::Begin("Dialogue Editor Demo", open)) {
		ImGui::End();
		DrawJsonPreview(state);
		return;
	}

	if (state.document.dialogues.empty()) {
		DialogueEditorEntryDraft dialogue{
			.name = "dialogue",
			.lines = {
				MakeLine(state.document),
			},
		};

		state.document.dialogues.emplace_back(std::move(dialogue));
		state.document.start = "dialogue";
		state.selected_dialogue = 0;
		state.selected_line = 0;
		state.selected_page = 0;
	}

	if (ImGui::Button("Preview JSON")) {
		state.show_json = true;
	}

	ImGui::SameLine();

	if (ImGui::Button("Reset Demo")) {
		state = MakeDemoState();
	}

	ImGui::SameLine();
	ImGui::TextDisabled(
		"Pages are real RichText documents; add, duplicate, reorder, delete, or split at the cursor."
	);

	ImGui::Separator();

	DrawDialogueEditorBody(ctx, state);

	ImGui::End();

	DrawJsonPreview(state);
}

} // namespace ptgn::editor
