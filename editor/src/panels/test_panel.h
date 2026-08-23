#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "runtime/ui/dialogue.h"
#include "serialization/json/json.h"

namespace ptgn::editor {

namespace {

using json = nlohmann::json;

constexpr std::string_view kPageBreakToken{ "[[PAGE]]" };

struct DialogueEditorTextCursor {
	int cursor_pos{ 0 };
	int requested_cursor_pos{ 0 };
	bool request_cursor{ false };
};

struct DialogueEditorLineDraft {
	std::string source;
	DialogueEditorTextCursor cursor;
};

struct DialogueEditorEntryDraft {
	std::string name;
	int index{ 0 };
	bool repeatable{ true };
	DialogueBehavior behavior{ DialogueBehavior::Sequential };
	bool scroll{ true };
	std::string next;

	std::vector<DialogueEditorLineDraft> lines;
};

struct DialogueEditorDocument {
	std::string start;
	std::vector<DialogueEditorEntryDraft> dialogues;
};

struct DialogueEditorState {
	DialogueEditorDocument document;

	std::size_t selected_dialogue{ 0 };
	std::size_t selected_line{ 0 };

	std::string rename_buffer;
	bool rename_requested{ false };

	std::string generated_json;
	bool show_json{ false };
};

struct InputTextContext {
	std::string* text{ nullptr };
	DialogueEditorTextCursor* cursor{ nullptr };
};

int InputTextCallback(ImGuiInputTextCallbackData* data) {
	auto* context{ static_cast<InputTextContext*>(data->UserData) };

	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		context->text->resize(static_cast<std::size_t>(data->BufTextLen));
		data->Buf = context->text->data();
		return 0;
	}

	if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways && context->cursor) {
		if (context->cursor->request_cursor) {
			auto requested{ std::clamp(
				context->cursor->requested_cursor_pos,
				0,
				data->BufTextLen
			) };

			data->CursorPos = requested;
			data->SelectionStart = requested;
			data->SelectionEnd = requested;
			context->cursor->request_cursor = false;
		}

		context->cursor->cursor_pos = data->CursorPos;
	}

	return 0;
}

bool InputTextMultiline(
	const char* label,
	std::string& value,
	DialogueEditorTextCursor& cursor,
	ImVec2 size
) {
	if (value.capacity() < 256) {
		value.reserve(256);
	}

	InputTextContext context{
		.text = &value,
		.cursor = &cursor,
	};

	auto flags{
		ImGuiInputTextFlags_CallbackResize |
		ImGuiInputTextFlags_CallbackAlways |
		ImGuiInputTextFlags_AllowTabInput
	};

	return ImGui::InputTextMultiline(
		label,
		value.data(),
		value.capacity() + 1,
		size,
		flags,
		InputTextCallback,
		&context
	);
}

std::size_t FindPageBreak(const std::string& source, std::size_t break_index) {
	std::size_t search_from{ 0 };

	for (std::size_t i{ 0 }; i <= break_index; ++i) {
		auto position{ source.find(kPageBreakToken, search_from) };

		if (position == std::string::npos) {
			return std::string::npos;
		}

		if (i == break_index) {
			return position;
		}

		search_from = position + kPageBreakToken.size();
	}

	return std::string::npos;
}

std::size_t GetPageCount(const std::string& source) {
	std::size_t count{ 1 };
	std::size_t search_from{ 0 };

	while (true) {
		auto position{ source.find(kPageBreakToken, search_from) };

		if (position == std::string::npos) {
			break;
		}

		++count;
		search_from = position + kPageBreakToken.size();
	}

	return count;
}

std::size_t GetCurrentPageIndex(const std::string& source, int cursor_pos) {
	auto cursor{ static_cast<std::size_t>(std::max(cursor_pos, 0)) };
	std::size_t page{ 0 };
	std::size_t search_from{ 0 };

	while (true) {
		auto position{ source.find(kPageBreakToken, search_from) };

		if (position == std::string::npos || position >= cursor) {
			break;
		}

		++page;
		search_from = position + kPageBreakToken.size();
	}

	return page;
}

std::size_t GetPageStart(const std::string& source, std::size_t page_index) {
	if (page_index == 0) {
		return 0;
	}

	auto marker{ FindPageBreak(source, page_index - 1) };

	if (marker == std::string::npos) {
		return source.size();
	}

	auto start{ marker + kPageBreakToken.size() };

	if (start < source.size() && source[start] == '\n') {
		++start;
	}

	return start;
}

void RequestCursor(DialogueEditorTextCursor& cursor, std::size_t position) {
	cursor.requested_cursor_pos = static_cast<int>(position);
	cursor.request_cursor = true;
}

void InsertPageBreak(
	std::string& source,
	DialogueEditorTextCursor& cursor,
	std::size_t position
) {
	position = std::min(position, source.size());

	std::string insertion;

	if (position > 0 && source[position - 1] != '\n') {
		insertion += '\n';
	}

	insertion += kPageBreakToken;

	if (position < source.size() && source[position] != '\n') {
		insertion += '\n';
	}

	source.insert(position, insertion);
	RequestCursor(cursor, position + insertion.size());
}

void RemovePageBreak(
	std::string& source,
	DialogueEditorTextCursor& cursor,
	std::size_t break_index
) {
	auto marker{ FindPageBreak(source, break_index) };

	if (marker == std::string::npos) {
		return;
	}

	auto erase_begin{ marker };
	auto erase_end{ marker + kPageBreakToken.size() };

	if (erase_begin > 0 &&
		source[erase_begin - 1] == '\n' &&
		erase_end < source.size() &&
		source[erase_end] == '\n') {
		++erase_end;
	}

	source.erase(erase_begin, erase_end - erase_begin);
	RequestCursor(cursor, erase_begin);
}

void MovePageBreakToCursor(
	std::string& source,
	DialogueEditorTextCursor& cursor,
	std::size_t break_index
) {
	auto marker{ FindPageBreak(source, break_index) };

	if (marker == std::string::npos) {
		return;
	}

	auto old_end{ marker + kPageBreakToken.size() };
	auto destination{ static_cast<std::size_t>(std::max(cursor.cursor_pos, 0)) };

	if (marker > 0 &&
		source[marker - 1] == '\n' &&
		old_end < source.size() &&
		source[old_end] == '\n') {
		++old_end;
	}

	if (destination > old_end) {
		destination -= old_end - marker;
	} else if (destination >= marker) {
		destination = marker;
	}

	source.erase(marker, old_end - marker);
	destination = std::min(destination, source.size());

	InsertPageBreak(source, cursor, destination);
}

std::vector<std::string> SplitIntoPages(const std::string& source) {
	std::vector<std::string> pages;
	std::size_t begin{ 0 };

	while (true) {
		auto marker{ source.find(kPageBreakToken, begin) };

		if (marker == std::string::npos) {
			auto page{ source.substr(begin) };

			if (!page.empty() && page.front() == '\n') {
				page.erase(page.begin());
			}

			pages.emplace_back(std::move(page));
			break;
		}

		auto page{ source.substr(begin, marker - begin) };

		if (!page.empty() && page.back() == '\n') {
			page.pop_back();
		}

		if (!page.empty() && page.front() == '\n') {
			page.erase(page.begin());
		}

		pages.emplace_back(std::move(page));

		begin = marker + kPageBreakToken.size();

		if (begin < source.size() && source[begin] == '\n') {
			++begin;
		}
	}

	return pages;
}

std::string FirstLine(std::string_view text) {
	auto newline{ text.find('\n') };
	auto result{ std::string{ text.substr(0, newline) } };

	if (result.size() > 54) {
		result.resize(51);
		result += "...";
	}

	if (result.empty()) {
		result = "(empty page)";
	}

	return result;
}

bool DialogueNameExists(
	const DialogueEditorDocument& document,
	std::string_view name,
	std::size_t ignore_index = std::string::npos
) {
	for (std::size_t i{ 0 }; i < document.dialogues.size(); ++i) {
		if (i != ignore_index && document.dialogues[i].name == name) {
			return true;
		}
	}

	return false;
}

std::string MakeUniqueDialogueName(const DialogueEditorDocument& document) {
	std::string base{ "dialogue" };

	if (!DialogueNameExists(document, base)) {
		return base;
	}

	for (int suffix{ 2 };; ++suffix) {
		auto candidate{ base + "_" + std::to_string(suffix) };

		if (!DialogueNameExists(document, candidate)) {
			return candidate;
		}
	}
}

void RenameDialogue(
	DialogueEditorDocument& document,
	std::size_t dialogue_index,
	std::string new_name
) {
	if (dialogue_index >= document.dialogues.size()) {
		return;
	}

	auto old_name{ document.dialogues[dialogue_index].name };

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

json BuildDialogueJson(const DialogueEditorDocument& document) {
	json root{
		{ "start", document.start },
		{ "dialogues", json::object() },
	};

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
			auto pages{ SplitIntoPages(line.source) };

			if (pages.size() == 1) {
				entry["lines"].push_back(pages.front());
				continue;
			}

			json page_array{ json::array() };

			for (auto& page : pages) {
				page_array.push_back(std::move(page));
			}

			entry["lines"].push_back(
				json{
					{ "pages", std::move(page_array) },
				}
			);
		}

		root["dialogues"][dialogue.name] = std::move(entry);
	}

	return root;
}

DialogueEditorState MakeDemoState() {
	DialogueEditorState state;

	state.document.start = "intro";

	state.document.dialogues = {
		DialogueEditorEntryDraft{
			.name = "intro",
			.index = 0,
			.repeatable = false,
			.behavior = DialogueBehavior::Sequential,
			.scroll = true,
			.next = "outro",
			.lines = {
				DialogueEditorLineDraft{
					.source =
						"Hey. You made it.\n\n"
						"I wasn't sure you'd actually come.\n"
						"[[PAGE]]\n"
						"There's something I need to show you before we leave.\n\n"
						"Try not to freak out.",
				},
				DialogueEditorLineDraft{
					.source =
						"Oh, good. You're here.\n\n"
						"Come on, we're already late.",
				},
			},
		},
		DialogueEditorEntryDraft{
			.name = "outro",
			.index = 0,
			.repeatable = true,
			.behavior = DialogueBehavior::Sequential,
			.scroll = true,
			.next = "epilogue",
			.lines = {
				DialogueEditorLineDraft{
					.source =
						"That's it, then.\n"
						"[[PAGE]]\n"
						"Whatever happens next, we do it together.",
				},
			},
		},
		DialogueEditorEntryDraft{
			.name = "epilogue",
			.index = 0,
			.repeatable = true,
			.behavior = DialogueBehavior::Sequential,
			.scroll = true,
			.next = "",
			.lines = {
				DialogueEditorLineDraft{
					.source = "The end.",
				},
			},
		},
	};

	return state;
}

void DrawRenamePopup(DialogueEditorState& state) {
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

	static char buffer[256]{};

	if (ImGui::IsWindowAppearing()) {
		std::fill(std::begin(buffer), std::end(buffer), '\0');

		auto copy_count{
			std::min(state.rename_buffer.size(), sizeof(buffer) - 1)
		};

		std::copy_n(state.rename_buffer.data(), copy_count, buffer);
		ImGui::SetKeyboardFocusHere();
	}

	ImGui::InputText("##DialogueName", buffer, sizeof(buffer));

	std::string proposed_name{ buffer };

	auto valid{
		!proposed_name.empty() &&
		!DialogueNameExists(
			state.document,
			proposed_name,
			state.selected_dialogue
		)
	};

	if (!valid) {
		ImGui::TextDisabled(
			proposed_name.empty()
				? "Name cannot be empty."
				: "That dialogue name already exists."
		);
	}

	if (!valid) {
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("Rename")) {
		RenameDialogue(
			state.document,
			state.selected_dialogue,
			std::move(proposed_name)
		);
		ImGui::CloseCurrentPopup();
	}

	if (!valid) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	if (ImGui::Button("Cancel")) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void DrawDialogueList(DialogueEditorState& state) {
	auto& document{ state.document };

	ImGui::BeginChild("##DialogueList", ImVec2{ 190.0f, 0.0f }, true);

	ImGui::TextUnformatted("Dialogues");
	ImGui::Separator();

	if (ImGui::Button("+ Dialogue", ImVec2{ -1.0f, 0.0f })) {
		auto name{ MakeUniqueDialogueName(document) };

		document.dialogues.emplace_back(
			DialogueEditorEntryDraft{
				.name = name,
				.lines = {
					DialogueEditorLineDraft{},
				},
			}
		);

		state.selected_dialogue = document.dialogues.size() - 1;
		state.selected_line = 0;

		if (document.start.empty()) {
			document.start = name;
		}
	}

	ImGui::Spacing();

	for (std::size_t i{ 0 }; i < document.dialogues.size(); ++i) {
		auto selected{ i == state.selected_dialogue };

		if (ImGui::Selectable(
				document.dialogues[i].name.c_str(),
				selected
			)) {
			state.selected_dialogue = i;
			state.selected_line = 0;
		}

		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Rename")) {
				state.rename_buffer = document.dialogues[i].name;
				state.selected_dialogue = i;
				state.rename_requested = true;
			}

			if (ImGui::MenuItem("Duplicate")) {
				auto duplicate{ document.dialogues[i] };
				duplicate.name = MakeUniqueDialogueName(document);

				document.dialogues.insert(
					document.dialogues.begin() + static_cast<std::ptrdiff_t>(i + 1),
					std::move(duplicate)
				);

				state.selected_dialogue = i + 1;
				state.selected_line = 0;
			}

			if (ImGui::MenuItem("Delete", nullptr, false, document.dialogues.size() > 1)) {
				auto deleted_name{ document.dialogues[i].name };

				document.dialogues.erase(
					document.dialogues.begin() + static_cast<std::ptrdiff_t>(i)
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
			}

			ImGui::EndPopup();
		}
	}

	ImGui::EndChild();
}

void DrawEntryProperties(
	DialogueEditorState& state,
	DialogueEditorEntryDraft& dialogue
) {
	ImGui::BeginChild("##DialogueProperties", ImVec2{ 250.0f, 0.0f }, true);

	ImGui::TextUnformatted("Properties");
	ImGui::Separator();

	if (ImGui::Button("Rename", ImVec2{ -1.0f, 0.0f })) {
		state.rename_buffer = dialogue.name;
		state.rename_requested = true;
	}

	ImGui::Spacing();

	ImGui::TextUnformatted("Start dialogue");
	ImGui::SetNextItemWidth(-1.0f);

	if (ImGui::BeginCombo("##StartDialogue", state.document.start.c_str())) {
		for (const auto& candidate : state.document.dialogues) {
			auto selected{ state.document.start == candidate.name };

			if (ImGui::Selectable(candidate.name.c_str(), selected)) {
				state.document.start = candidate.name;
			}
		}

		ImGui::EndCombo();
	}

	ImGui::Spacing();

	ImGui::TextUnformatted("Behavior");
	ImGui::SetNextItemWidth(-1.0f);

	auto behavior_name{
		dialogue.behavior == DialogueBehavior::Sequential
			? "Sequential"
			: "Random"
	};

	if (ImGui::BeginCombo("##Behavior", behavior_name)) {
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
	ImGui::Checkbox("Scroll text", &dialogue.scroll);

	ImGui::Spacing();

	ImGui::TextUnformatted("Initial line index");
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputInt("##InitialLine", &dialogue.index);
	dialogue.index = std::max(dialogue.index, 0);

	ImGui::Spacing();

	ImGui::TextUnformatted("Next dialogue");
	ImGui::SetNextItemWidth(-1.0f);

	auto next_label{
		dialogue.next.empty()
			? "(none)"
			: dialogue.next.c_str()
	};

	if (ImGui::BeginCombo("##NextDialogue", next_label)) {
		if (ImGui::Selectable("(none)", dialogue.next.empty())) {
			dialogue.next.clear();
		}

		for (const auto& candidate : state.document.dialogues) {
			auto selected{ dialogue.next == candidate.name };

			if (ImGui::Selectable(candidate.name.c_str(), selected)) {
				dialogue.next = candidate.name;
			}
		}

		ImGui::EndCombo();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::TextWrapped(
		"[[PAGE]] is an editor-only authoring marker. Export converts it "
		"to the existing JSON pages array."
	);

	ImGui::EndChild();
}

void DrawLineSelector(
	DialogueEditorState& state,
	DialogueEditorEntryDraft& dialogue
) {
	ImGui::TextUnformatted("Variants");
	ImGui::SameLine();

	if (ImGui::SmallButton("+")) {
		dialogue.lines.emplace_back();
		state.selected_line = dialogue.lines.size() - 1;
	}

	ImGui::SameLine();

	if (ImGui::SmallButton("Duplicate") && !dialogue.lines.empty()) {
		auto duplicate{ dialogue.lines[state.selected_line] };
		duplicate.cursor = {};

		dialogue.lines.insert(
			dialogue.lines.begin() +
				static_cast<std::ptrdiff_t>(state.selected_line + 1),
			std::move(duplicate)
		);

		++state.selected_line;
	}

	ImGui::SameLine();

	if (dialogue.lines.size() <= 1) {
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
	}

	if (dialogue.lines.size() <= 1) {
		ImGui::EndDisabled();
	}

	ImGui::Spacing();

	for (std::size_t i{ 0 }; i < dialogue.lines.size(); ++i) {
		if (i > 0) {
			ImGui::SameLine();
		}

		auto label{ "Line " + std::to_string(i + 1) };

		if (ImGui::Selectable(
				label.c_str(),
				i == state.selected_line,
				0,
				ImVec2{ 72.0f, 0.0f }
			)) {
			state.selected_line = i;
		}
	}
}

void DrawPageToolbar(DialogueEditorLineDraft& line) {
	auto page_count{ GetPageCount(line.source) };
	auto current_page{
		std::min(
			GetCurrentPageIndex(line.source, line.cursor.cursor_pos),
			page_count - 1
		)
	};

	if (ImGui::Button("+ Page Break")) {
		InsertPageBreak(
			line.source,
			line.cursor,
			static_cast<std::size_t>(std::max(line.cursor.cursor_pos, 0))
		);
	}

	ImGui::SameLine();

	if (current_page > 0) {
		if (ImGui::Button("Move Previous Break Here")) {
			MovePageBreakToCursor(
				line.source,
				line.cursor,
				current_page - 1
			);
		}
	} else {
		ImGui::BeginDisabled();
		ImGui::Button("Move Previous Break Here");
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	if (current_page + 1 < page_count) {
		if (ImGui::Button("Move Next Break Here")) {
			MovePageBreakToCursor(
				line.source,
				line.cursor,
				current_page
			);
		}
	} else {
		ImGui::BeginDisabled();
		ImGui::Button("Move Next Break Here");
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	if (page_count > 1) {
		if (ImGui::Button("Remove Previous Break")) {
			auto break_index{
				current_page > 0
					? current_page - 1
					: current_page
			};

			RemovePageBreak(line.source, line.cursor, break_index);
		}
	} else {
		ImGui::BeginDisabled();
		ImGui::Button("Remove Previous Break");
		ImGui::EndDisabled();
	}
}

void DrawPageNavigator(DialogueEditorLineDraft& line) {
	auto pages{ SplitIntoPages(line.source) };
	auto page_count{ pages.size() };
	auto current_page{
		std::min(
			GetCurrentPageIndex(line.source, line.cursor.cursor_pos),
			page_count - 1
		)
	};

	ImGui::Separator();
	ImGui::Text(
		"Page %zu / %zu",
		current_page + 1,
		page_count
	);

	ImGui::SameLine();

	if (current_page == 0) {
		ImGui::BeginDisabled();
	}

	if (ImGui::SmallButton("<")) {
		RequestCursor(line.cursor, GetPageStart(line.source, current_page - 1));
	}

	if (current_page == 0) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	if (current_page + 1 >= page_count) {
		ImGui::BeginDisabled();
	}

	if (ImGui::SmallButton(">")) {
		RequestCursor(line.cursor, GetPageStart(line.source, current_page + 1));
	}

	if (current_page + 1 >= page_count) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	ImGui::TextDisabled(
		"%zu chars",
		pages[current_page].size()
	);

	ImGui::Spacing();

	for (std::size_t i{ 0 }; i < pages.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));

		auto selected{ i == current_page };
		auto page_label{
			"Page " + std::to_string(i + 1) + "  " + FirstLine(pages[i])
		};

		if (ImGui::Selectable(page_label.c_str(), selected)) {
			RequestCursor(line.cursor, GetPageStart(line.source, i));
		}

		ImGui::PopID();
	}
}

void DrawDialogueTextEditor(
	DialogueEditorState& state,
	DialogueEditorEntryDraft& dialogue
) {
	if (dialogue.lines.empty()) {
		dialogue.lines.emplace_back();
	}

	state.selected_line = std::min(
		state.selected_line,
		dialogue.lines.size() - 1
	);

	DrawLineSelector(state, dialogue);

	ImGui::Separator();
	ImGui::Spacing();

	auto& line{ dialogue.lines[state.selected_line] };

	DrawPageToolbar(line);

	ImGui::Spacing();

	auto available{ ImGui::GetContentRegionAvail() };
	auto navigator_height{ 145.0f };
	auto editor_height{ std::max(180.0f, available.y - navigator_height) };

	InputTextMultiline(
		"##DialogueSource",
		line.source,
		line.cursor,
		ImVec2{ -1.0f, editor_height }
	);

	DrawPageNavigator(line);
}

void DrawJsonPreview(DialogueEditorState& state) {
	if (!state.show_json) {
		return;
	}

	ImGui::SetNextWindowSize(ImVec2{ 650.0f, 520.0f }, ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("Dialogue JSON", &state.show_json)) {
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
		"##GeneratedDialogueJson",
		state.generated_json.data(),
		state.generated_json.size() + 1,
		ImVec2{ -1.0f, -1.0f },
		ImGuiInputTextFlags_ReadOnly
	);

	ImGui::End();
}

} // namespace

void DrawDialogueEditorDemoWindow(bool* open) {
	static DialogueEditorState state{ MakeDemoState() };

	if (open && !*open) {
		return;
	}

	ImGui::SetNextWindowSize(ImVec2{ 1100.0f, 700.0f }, ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("Dialogue Editor Demo", open)) {
		ImGui::End();
		return;
	}

	if (state.document.dialogues.empty()) {
		state.document.dialogues.emplace_back(
			DialogueEditorEntryDraft{
				.name = "dialogue",
				.lines = {
					DialogueEditorLineDraft{},
				},
			}
		);

		state.document.start = "dialogue";
		state.selected_dialogue = 0;
		state.selected_line = 0;
	}

	state.selected_dialogue = std::min(
		state.selected_dialogue,
		state.document.dialogues.size() - 1
	);

	if (ImGui::Button("Preview JSON")) {
		state.show_json = true;
	}

	ImGui::SameLine();
	ImGui::TextDisabled(
		"Author one continuous line; insert [[PAGE]] wherever a hard page break belongs."
	);

	ImGui::Separator();

	DrawDialogueList(state);

	ImGui::SameLine();

	ImGui::BeginGroup();

	auto right_panel_width{ 250.0f };
	auto center_width{
		std::max(
			300.0f,
			ImGui::GetContentRegionAvail().x - right_panel_width - ImGui::GetStyle().ItemSpacing.x
		)
	};

	ImGui::BeginChild(
		"##DialogueCenter",
		ImVec2{ center_width, 0.0f },
		true
	);

	auto& dialogue{
		state.document.dialogues[state.selected_dialogue]
	};

	DrawDialogueTextEditor(state, dialogue);

	ImGui::EndChild();

	ImGui::SameLine();

	DrawEntryProperties(state, dialogue);

	ImGui::EndGroup();

	DrawRenamePopup(state);

	ImGui::End();

	DrawJsonPreview(state);
}

} // namespace ptgn::editor
