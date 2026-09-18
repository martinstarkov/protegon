#include "panels/rich_text_editor.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "commands/entity/entity_reference.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/angle.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/util/hash.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "editor/color_picker.h"
#include "panels/entity_filter_editor.h"
#include "panels/inspector_feature_helpers.h"
#include "panels/inspector_fields.h"
#include "panels/scene_hierarchy.h"
#include "panels/scene_list.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/renderer.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/prefab.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_group.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/custom_shader.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/blur.h"
#include "runtime/graphics/fx/gaussian_blur.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/graphics.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/sprite_stack.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/platformer_jump.h"
#include "runtime/physics/platformer_jump_registry.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/scripting/script.h"
#include "runtime/timer/timer.h"
#include "runtime/timer/timer_event.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"
#include "tools/debug/debug_system.h"

namespace ptgn::editor::inspector {

namespace {

enum class RichTextPendingActionType : std::uint8_t {
	ToggleTag,
	SetTag,
	RemoveTag,
	RemoveEffects,
	InsertToken,
	ToggleStandaloneLineToken,
};

enum class RichTextEffectEditorKind : std::uint8_t {
	Glyph,
	Outline,
	Shadow,
	OuterGlow,
	InnerGlow,
};

struct RichTextPendingAction {
	RichTextPendingActionType type{ RichTextPendingActionType::ToggleTag };
	std::size_t cursor{ 0 };
	std::size_t selection_start{ 0 };
	std::size_t selection_end{ 0 };
	std::string tag{};
	std::string open{};
	std::string close{};
	std::string token{};
};

struct RichTextSelectionState {
	std::size_t cursor{ 0 };
	std::size_t selection_start{ 0 };
	std::size_t selection_end{ 0 };
	bool apply_selection{ false };

	// BIUS actions reactivate the source input so ImGui continues drawing the
	// selected range. The source input is always rendered without its active/nav
	// highlight, so restoring focus never adds a blue frame around the editor.
	bool focus_source{ false };

	// Popup/drag controls necessarily take ImGui's active ID away from the source input.
	// Keep drawing the last source selection ourselves while those controls are active so
	// it remains obvious which text the formatting operation targets.
	bool keep_selection_highlight{ false };

	// While the mouse is dragging in the source input, use the editor's tag-aware visual
	// row map as the authoritative hit-test so the highlight stays directly under the mouse.
	std::optional<std::size_t> drag_selection_anchor{};

	// Double-click dragging needs both edges of the selected word. Dragging right
	// extends from the word's left edge; dragging left extends from its right edge.
	std::optional<std::pair<std::size_t, std::size_t>> drag_word_selection{};

	std::optional<RichTextPendingAction> pending_action{};
};

struct RichTextSourceHistoryEntry {
	std::string source{};
	std::size_t cursor{ 0 };
	std::size_t selection_start{ 0 };
	std::size_t selection_end{ 0 };
};

struct RichTextEditorState {
	RichTextSelectionState inline_selection{};
	RichTextSelectionState window_selection{};
	bool initialized{ false };
	bool window_open{ false };
	std::string source{};
	std::array<float, 4> color{ 1.0f, 1.0f, 1.0f, 1.0f };
	std::string font{};
	float size{ kDefaultFontSize };

	std::optional<RichTextEffectEditorKind> effect_editor_kind{};
	GlyphEffectType effect_type{ GlyphEffectType::None };
	float effect_amplitude{ 0.0f };
	float effect_frequency{ 0.0f };
	float effect_speed{ 0.0f };
	float effect_phase{ 0.0f };
	std::array<float, 4> effect_color{ 1.0f, 1.0f, 1.0f, 1.0f };
	float effect_width{ 0.0f };
	float effect_softness{ 1.0f };
	V2_float effect_shadow_offset{};

	std::vector<RichTextSourceHistoryEntry> source_history{};
	std::size_t source_history_index{ 0 };
	bool source_history_applied{ false };

	// Editor-only display preferences. These never modify the authored rich text source.
	bool word_wrap{ true };
	bool show_page_numbers{ false };

	// Source/preview heights are editor-only UI state. Keep detached and inline editors independent.
	float inline_source_height{ 0.0f };
	float window_source_height{ 0.0f };
	float inline_preview_height{ 0.0f };
	float window_preview_height{ 0.0f };

	std::string selection_warning{};
	double selection_warning_until{ 0.0 };

	// Used to prune editor instances whose ImGui IDs are no longer being drawn.
	int last_seen_frame{ 0 };
};

inline constexpr std::array<float, 17> kRichTextCommonFontSizes{
	8.0f, 9.0f, 10.0f, 10.5f, 11.0f, 12.0f, 14.0f, 16.0f, 18.0f,
	20.0f, 22.0f, 24.0f, 26.0f, 28.0f, 36.0f, 48.0f, 72.0f,
};

void ClampRichTextSelection(RichTextSelectionState& state, std::size_t size) {
	state.cursor = std::min(state.cursor, size);
	state.selection_start = std::min(state.selection_start, size);
	state.selection_end = std::min(state.selection_end, size);
}


void ResetRichTextSourceHistory(
	RichTextEditorState& editor_state,
	std::string_view source,
	const RichTextSelectionState& selection
) {
	editor_state.source_history.clear();
	editor_state.source_history.emplace_back(RichTextSourceHistoryEntry{
		.source = std::string{ source },
		.cursor = selection.cursor,
		.selection_start = selection.selection_start,
		.selection_end = selection.selection_end,
	});
	editor_state.source_history_index = 0;
	editor_state.source_history_applied = false;
}

void RecordRichTextSourceHistory(
	RichTextEditorState& editor_state,
	std::string_view source,
	const RichTextSelectionState& selection
) {
	if (editor_state.source_history.empty()) {
		ResetRichTextSourceHistory(editor_state, source, selection);
		return;
	}

	auto& current{ editor_state.source_history[editor_state.source_history_index] };
	if (current.source == source) {
		current.cursor = selection.cursor;
		current.selection_start = selection.selection_start;
		current.selection_end = selection.selection_end;
		return;
	}

	if (editor_state.source_history_index + 1 < editor_state.source_history.size()) {
		editor_state.source_history.erase(
			editor_state.source_history.begin() +
				static_cast<std::ptrdiff_t>(editor_state.source_history_index + 1),
			editor_state.source_history.end()
		);
	}

	editor_state.source_history.emplace_back(RichTextSourceHistoryEntry{
		.source = std::string{ source },
		.cursor = selection.cursor,
		.selection_start = selection.selection_start,
		.selection_end = selection.selection_end,
	});
	editor_state.source_history_index = editor_state.source_history.size() - 1;
}

// Pending toolbar/popup actions store the exact source selection that existed when the action
// was requested. Commit that selection to the pre-action history entry before mutating the
// source, so undo restores both the old markup and the text range the user originally selected.
void PreserveRichTextHistorySelectionBeforeAction(
	RichTextEditorState& editor_state, std::string_view source,
	const RichTextPendingAction& action
) {
	RichTextSelectionState selection{
		.cursor = std::min(action.cursor, source.size()),
		.selection_start = std::min(action.selection_start, source.size()),
		.selection_end = std::min(action.selection_end, source.size()),
	};

	if (editor_state.source_history.empty() ||
		editor_state.source_history[editor_state.source_history_index].source != source) {
		RecordRichTextSourceHistory(editor_state, source, selection);
		return;
	}

	auto& current{ editor_state.source_history[editor_state.source_history_index] };
	current.cursor = selection.cursor;
	current.selection_start = selection.selection_start;
	current.selection_end = selection.selection_end;
}

bool ApplyRichTextSourceHistory(
	ImGuiInputTextCallbackData& data,
	RichTextEditorState& editor_state,
	RichTextSelectionState& selection,
	int direction
) {
	if (editor_state.source_history.empty()) {
		return false;
	}

	if (direction < 0) {
		if (editor_state.source_history_index == 0) {
			return false;
		}
		--editor_state.source_history_index;
	} else {
		if (editor_state.source_history_index + 1 >= editor_state.source_history.size()) {
			return false;
		}
		++editor_state.source_history_index;
	}

	const auto& entry{ editor_state.source_history[editor_state.source_history_index] };

	data.DeleteChars(0, data.BufTextLen);
	data.InsertChars(0, entry.source.c_str());

	const auto clamp_position = [&data](std::size_t position) {
		return std::max(
			0,
			static_cast<int>(
				std::min(position, static_cast<std::size_t>(data.BufTextLen))
			)
		);
	};

	data.CursorPos = clamp_position(entry.cursor);
	data.SelectionStart = clamp_position(entry.selection_start);
	data.SelectionEnd = clamp_position(entry.selection_end);

	selection.cursor = static_cast<std::size_t>(data.CursorPos);
	selection.selection_start = static_cast<std::size_t>(data.SelectionStart);
	selection.selection_end = static_cast<std::size_t>(data.SelectionEnd);
	selection.apply_selection = false;

	editor_state.source_history_applied = true;
	return true;
}

void RequestRichTextSelection(
	RichTextSelectionState& state, std::size_t cursor, std::size_t selection_start,
	std::size_t selection_end
) {
	state.cursor = cursor;
	state.selection_start = selection_start;
	state.selection_end = selection_end;
	state.apply_selection = true;

}

bool WrapRichTextSelection(
	std::string& source, RichTextSelectionState& state, std::string_view open,
	std::string_view close
) {
	ClampRichTextSelection(state, source.size());

	std::size_t begin{ std::min(state.selection_start, state.selection_end) };
	std::size_t end{ std::max(state.selection_start, state.selection_end) };

	if (begin == end) {
		begin = state.cursor;
		end = state.cursor;
	}

	source.insert(end, close);
	source.insert(begin, open);

	if (begin == end) {
		const std::size_t cursor{ begin + open.size() };
		RequestRichTextSelection(state, cursor, cursor, cursor);
	} else {
		const std::size_t selection_start{ begin + open.size() };
		const std::size_t selection_end{ end + open.size() };
		RequestRichTextSelection(state, selection_end, selection_start, selection_end);
	}

	return true;
}

struct RichTextSurroundingTag {
	std::size_t open_begin{};
	std::size_t open_size{};
	std::size_t close_begin{};
	std::size_t close_size{};
};

[[nodiscard]] bool RichTextTagNameMatches(
	std::string_view name, std::string_view tag
) {
	if (name == tag) {
		return true;
	}
	if (tag == "s" && name == "strike") {
		return true;
	}
	return tag == "c" && name == "color";
}

[[nodiscard]] bool IsEscapedRichTextCharacter(
	std::string_view source, std::size_t position
) {
	std::size_t backslashes{ 0 };
	while (position > backslashes && source[position - backslashes - 1] == '\\') {
		++backslashes;
	}
	return (backslashes % 2) != 0;
}

[[nodiscard]] bool IsRichTextPositionInsideTag(
	std::string_view source, std::size_t position
) {
	position = std::min(position, source.size());

	for (std::size_t search_from{ 0 }; search_from < source.size();) {
		std::size_t open{ source.find('<', search_from) };
		while (open != std::string_view::npos && IsEscapedRichTextCharacter(source, open)) {
			open = source.find('<', open + 1);
		}
		if (open == std::string_view::npos) {
			return false;
		}

		const std::size_t close{ source.find('>', open + 1) };
		if (close == std::string_view::npos) {
			// An unterminated tag has no valid boundary after '<'. Treat every cursor
			// position after its opening delimiter as being inside the partial tag.
			return position > open;
		}

		// Cursor/selection positions are between characters. The position immediately
		// before '<' and immediately after '>' are valid tag boundaries; positions
		// between them would split the tag.
		if (position > open && position <= close) {
			return true;
		}

		if (position <= open) {
			return false;
		}

		search_from = close + 1;
	}

	return false;
}

[[nodiscard]] bool IsRichTextPositionInsideVariable(
	std::string_view source, std::size_t position
) {
	position = std::min(position, source.size());
	for (std::size_t search_from{ 0 }; search_from < source.size();) {
		const std::size_t open{ source.find("${", search_from) };
		if (open == std::string_view::npos) {
			return false;
		}
		const std::size_t close{ source.find('}', open + 2) };
		if (close == std::string_view::npos) {
			return position > open;
		}
		if (position > open && position <= close) {
			return true;
		}
		if (position <= open) {
			return false;
		}
		search_from = close + 1;
	}
	return false;
}

[[nodiscard]] bool RichTextSelectionContainsPartialTag(
	std::string_view source, const RichTextSelectionState& selection
) {
	std::size_t begin{ std::min(selection.selection_start, selection.selection_end) };
	std::size_t end{ std::max(selection.selection_start, selection.selection_end) };
	begin = std::min(begin, source.size());
	end = std::min(end, source.size());

	if (begin == end) {
		begin = std::min(selection.cursor, source.size());
		end = begin;
	}

	return IsRichTextPositionInsideTag(source, begin) ||
		   IsRichTextPositionInsideTag(source, end);
}

[[nodiscard]] bool RichTextSelectionHasRange(
	std::string_view source, const RichTextSelectionState& selection
) {
	const std::size_t begin{
		std::min(std::min(selection.selection_start, selection.selection_end), source.size())
	};
	const std::size_t end{
		std::min(std::max(selection.selection_start, selection.selection_end), source.size())
	};
	return begin < end;
}

[[nodiscard]] bool RichTextSelectionContainsTextOutsideTags(
	std::string_view source, const RichTextSelectionState& selection
) {
	std::size_t begin{ std::min(selection.selection_start, selection.selection_end) };
	std::size_t end{ std::max(selection.selection_start, selection.selection_end) };
	begin = std::min(begin, source.size());
	end = std::min(end, source.size());

	if (begin >= end) {
		return false;
	}

	for (std::size_t i{ begin }; i < end;) {
		if (source[i] == '<' && !IsEscapedRichTextCharacter(source, i)) {
			const std::size_t close{ source.find('>', i + 1) };
			if (close != std::string_view::npos && close < end) {
				i = close + 1;
				continue;
			}
		}

		// Formatting a selection containing only markup (or only whitespace between markup)
		// is almost certainly accidental. Escaped '<' characters reach this branch and are
		// therefore treated as ordinary source text, as they should be.
		if (std::isspace(static_cast<unsigned char>(source[i])) == 0) {
			return true;
		}
		++i;
	}

	return false;
}

[[nodiscard]] bool RichTextSelectionContainsBalancedTags(
	std::string_view source, const RichTextSelectionState& selection
) {
	std::size_t begin{ std::min(selection.selection_start, selection.selection_end) };
	std::size_t end{ std::max(selection.selection_start, selection.selection_end) };
	begin = std::min(begin, source.size());
	end = std::min(end, source.size());

	if (begin >= end) {
		return true;
	}

	std::vector<std::string> open_tags;
	for (std::size_t i{ begin }; i < end;) {
		if (source[i] != '<' || IsEscapedRichTextCharacter(source, i)) {
			++i;
			continue;
		}

		const std::size_t close{ source.find('>', i + 1) };
		if (close == std::string_view::npos || close >= end) {
			return false;
		}

		std::size_t token_begin{ i + 1 };
		std::size_t token_end{ close };
		while (token_begin < token_end &&
			std::isspace(static_cast<unsigned char>(source[token_begin])) != 0) {
			++token_begin;
		}
		while (token_end > token_begin &&
			std::isspace(static_cast<unsigned char>(source[token_end - 1])) != 0) {
			--token_end;
		}

		if (token_begin >= token_end) {
			i = close + 1;
			continue;
		}

		const bool closing{ source[token_begin] == '/' };
		if (closing) {
			++token_begin;
		}

		const std::size_t equals{ source.find('=', token_begin) };
		const std::size_t name_end{
			equals != std::string_view::npos && equals < token_end ? equals : token_end
		};
		std::string_view name{ source.substr(token_begin, name_end - token_begin) };
		while (!name.empty() &&
			std::isspace(static_cast<unsigned char>(name.back())) != 0) {
			name.remove_suffix(1);
		}

		if (name.empty()) {
			i = close + 1;
			continue;
		}

		if (!closing) {
			open_tags.emplace_back(name);
			i = close + 1;
			continue;
		}

		if (open_tags.empty()) {
			return false;
		}

		const std::string_view opening{ open_tags.back() };
		const bool names_match{
			RichTextTagNameMatches(opening, name) || RichTextTagNameMatches(name, opening)
		};
		if (!names_match) {
			return false;
		}

		open_tags.pop_back();
		i = close + 1;
	}

	return open_tags.empty();
}

[[nodiscard]] std::optional<std::string> RichTextFormattingTargetError(
	std::string_view source, const RichTextSelectionState& selection,
	bool require_text_for_range = true
) {
	if (RichTextSelectionContainsPartialTag(source, selection)) {
		return "Don't place the caret inside a tag or select only part of a tag.";
	}

	const std::size_t begin{
		std::min(std::min(selection.selection_start, selection.selection_end), source.size())
	};
	const std::size_t end{
		std::min(std::max(selection.selection_start, selection.selection_end), source.size())
	};
	if (IsRichTextPositionInsideVariable(source, begin) ||
		IsRichTextPositionInsideVariable(source, end)) {
		return "Select the complete ${variable} token or move the caret outside it.";
	}

	const bool has_range{ RichTextSelectionHasRange(source, selection) };
	if (!has_range) {
		if (IsRichTextPositionInsideTag(
				source, std::min(selection.cursor, source.size())
			)) {
			return "Move the caret outside the tag before applying formatting.";
		}
		return std::nullopt;
	}

	if (!RichTextSelectionContainsBalancedTags(source, selection)) {
		return "Select matching opening and closing tags.";
	}
	if (require_text_for_range &&
		!RichTextSelectionContainsTextOutsideTags(source, selection)) {
		return "Selection contains only rich text tags. Select actual text as well.";
	}
	return std::nullopt;
}

[[nodiscard]] std::optional<RichTextSurroundingTag> FindRichTextSelectedTag(
	std::string_view source, std::size_t selection_begin, std::size_t selection_end,
	std::string_view tag
) {
	selection_begin = std::min(selection_begin, source.size());
	selection_end = std::min(selection_end, source.size());
	if (selection_begin >= selection_end || source[selection_begin] != '<') {
		return std::nullopt;
	}

	const std::size_t open_end{ source.find('>', selection_begin + 1) };
	if (open_end == std::string_view::npos || open_end + 1 >= selection_end) {
		return std::nullopt;
	}

	std::string_view open_token{
		source.substr(selection_begin + 1, open_end - selection_begin - 1)
	};
	if (open_token.empty() || open_token.starts_with('/')) {
		return std::nullopt;
	}

	const auto equals{ open_token.find('=') };
	const std::string_view open_name{ open_token.substr(0, equals) };
	if (!RichTextTagNameMatches(open_name, tag)) {
		return std::nullopt;
	}

	const std::size_t close_begin{ source.rfind('<', selection_end - 1) };
	if (close_begin == std::string_view::npos || close_begin <= open_end) {
		return std::nullopt;
	}

	const std::size_t close_end{ source.find('>', close_begin + 1) };
	if (close_end == std::string_view::npos || close_end + 1 != selection_end) {
		return std::nullopt;
	}

	std::string_view close_token{
		source.substr(close_begin + 1, close_end - close_begin - 1)
	};
	if (!close_token.starts_with('/')) {
		return std::nullopt;
	}
	close_token.remove_prefix(1);

	if (!RichTextTagNameMatches(close_token, tag)) {
		return std::nullopt;
	}

	return RichTextSurroundingTag{
		.open_begin = selection_begin,
		.open_size = open_end - selection_begin + 1,
		.close_begin = close_begin,
		.close_size = close_end - close_begin + 1,
	};
}

[[nodiscard]] std::optional<RichTextSurroundingTag> FindRichTextSurroundingTag(
	std::string_view source, std::size_t selection_begin, std::size_t selection_end,
	std::string_view tag
) {
	selection_begin = std::min(selection_begin, source.size());
	selection_end = std::min(selection_end, source.size());

	// A selection may include the complete wrapper itself. Keep the exact-match fast path
	// because it also preserves the existing alias handling for <strike> and <color>.
	if (const auto selected{
			FindRichTextSelectedTag(source, selection_begin, selection_end, tag)
		}) {
		return selected;
	}

	struct OpenTag {
		std::size_t begin{};
		std::size_t end{};
	};

	std::vector<OpenTag> stack;
	std::optional<RichTextSurroundingTag> result;

	for (std::size_t search_from{ 0 }; search_from < source.size();) {
		std::size_t open{ source.find('<', search_from) };
		while (open != std::string_view::npos && IsEscapedRichTextCharacter(source, open)) {
			open = source.find('<', open + 1);
		}
		if (open == std::string_view::npos) {
			break;
		}

		const std::size_t close{ source.find('>', open + 1) };
		if (close == std::string_view::npos) {
			break;
		}

		std::string_view token{ source.substr(open + 1, close - open - 1) };
		if (token.empty()) {
			search_from = close + 1;
			continue;
		}

		const bool closing{ token.front() == '/' };
		if (closing) {
			token.remove_prefix(1);
		}

		const auto equals{ token.find('=') };
		std::string name{ token.substr(0, equals) };
		std::ranges::transform(name, name.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		if (name == "strike") name = "s";
		if (name == "color") name = "c";
		if (!RichTextTagNameMatches(name, tag)) {
			search_from = close + 1;
			continue;
		}

		if (!closing) {
			stack.push_back(OpenTag{ .begin = open, .end = close + 1 });
			search_from = close + 1;
			continue;
		}

		if (stack.empty()) {
			search_from = close + 1;
			continue;
		}

		const OpenTag opening{ stack.back() };
		stack.pop_back();

		const std::size_t close_end{ close + 1 };
		const bool contains_inner_selection{
			opening.end <= selection_begin && selection_end <= open
		};
		const bool selected_complete_wrapper{
			opening.begin == selection_begin && close_end == selection_end
		};

		if (contains_inner_selection || selected_complete_wrapper) {
			const RichTextSurroundingTag candidate{
				.open_begin = opening.begin,
				.open_size = opening.end - opening.begin,
				.close_begin = open,
				.close_size = close_end - open,
			};

			// If multiple same-kind wrappers contain the selection, use the innermost one.
			if (!result.has_value() || candidate.open_begin > result->open_begin) {
				result = candidate;
			}
		}

		search_from = close + 1;
	}

	return result;
}

struct RichTextEditorResolvedStyle {
	FontKey font{ kDefaultFont };
	TextRunStyle style{};
};

struct RichTextEditorSelectionFormat {
	bool valid{ false };
	bool has_range{ false };
	std::vector<RichTextEditorResolvedStyle> styles{};
};

[[nodiscard]] bool RichTextEditorGlyphEffectEqual(
	const GlyphEffectStyle& a, const GlyphEffectStyle& b
) {
	return a.type == b.type &&
		std::abs(a.amplitude - b.amplitude) <= 0.0001f &&
		std::abs(a.frequency - b.frequency) <= 0.0001f &&
		std::abs(a.speed - b.speed) <= 0.0001f &&
		std::abs(a.phase - b.phase) <= 0.0001f;
}

template <typename T, typename Get, typename Equal = std::equal_to<>>
[[nodiscard]] std::optional<T> RichTextUniformValue(
	const RichTextEditorSelectionFormat& format, Get&& get, Equal&& equal = {}
) {
	if (!format.valid || format.styles.empty()) {
		return std::nullopt;
	}

	T value{ std::invoke(get, format.styles.front()) };
	for (std::size_t i{ 1 }; i < format.styles.size(); ++i) {
		const T candidate{ std::invoke(get, format.styles[i]) };
		if (!std::invoke(equal, value, candidate)) {
			return std::nullopt;
		}
	}
	return value;
}

[[nodiscard]] std::string RichTextEditorMarker(
	std::string_view source, std::string_view base
) {
	std::string marker{ base };
	while (source.find(marker) != std::string_view::npos) {
		marker.push_back('_');
	}
	return marker;
}

[[nodiscard]] RichTextEditorSelectionFormat ResolveRichTextEditorSelectionFormat(
	std::string_view source, const RichTextSelectionState& selection,
	const TextRunDefaults& defaults
) {
	RichTextEditorSelectionFormat result;

	std::size_t begin{ std::min(selection.selection_start, selection.selection_end) };
	std::size_t end{ std::max(selection.selection_start, selection.selection_end) };
	begin = std::min(begin, source.size());
	end = std::min(end, source.size());
	result.has_range = begin < end;

	const std::size_t cursor{ std::min(selection.cursor, source.size()) };
	if (RichTextSelectionContainsPartialTag(source, selection) ||
		(!result.has_range && IsRichTextPositionInsideTag(source, cursor))) {
		return result;
	}

	if (result.has_range && !RichTextSelectionContainsBalancedTags(source, selection)) {
		return result;
	}

	const std::string begin_marker{
		RichTextEditorMarker(source, "\x1DPTGN_RICH_SELECTION_BEGIN\x1D")
	};
	const std::string end_marker{
		RichTextEditorMarker(source, "\x1DPTGN_RICH_SELECTION_END\x1D")
	};

	std::string marked{ source };
	if (result.has_range) {
		marked.insert(end, end_marker);
		marked.insert(begin, begin_marker);
	} else {
		marked.insert(cursor, begin_marker);
	}

	const auto parsed{ ParseRichText(marked, defaults) };
	if (!parsed.diagnostics.empty()) {
		return result;
	}

	if (!result.has_range) {
		for (const auto& run : parsed.text.runs) {
			if (run.text.find(begin_marker) == std::string::npos) {
				continue;
			}
			result.styles.push_back(
				RichTextEditorResolvedStyle{ .font = run.font, .style = run.style }
			);
			result.valid = true;
			return result;
		}
		return result;
	}

	bool inside{ false };
	for (const auto& run : parsed.text.runs) {
		std::string_view text{ run.text };
		std::size_t position{ 0 };

		while (position <= text.size()) {
			const std::size_t begin_pos{ text.find(begin_marker, position) };
			const std::size_t end_pos{ text.find(end_marker, position) };

			std::size_t next_marker{ std::string_view::npos };
			bool is_begin{ false };
			if (begin_pos != std::string_view::npos &&
				(end_pos == std::string_view::npos || begin_pos < end_pos)) {
				next_marker = begin_pos;
				is_begin = true;
			} else if (end_pos != std::string_view::npos) {
				next_marker = end_pos;
			}

			const std::size_t segment_end{
				next_marker == std::string_view::npos ? text.size() : next_marker
			};
			if (inside && segment_end > position) {
				result.styles.push_back(
					RichTextEditorResolvedStyle{ .font = run.font, .style = run.style }
				);
			}

			if (next_marker == std::string_view::npos) {
				break;
			}

			if (is_begin) {
				inside = true;
				position = next_marker + begin_marker.size();
			} else {
				inside = false;
				position = next_marker + end_marker.size();
			}
		}
	}

	result.valid = !result.styles.empty();
	return result;
}

[[nodiscard]] std::optional<bool> RichTextSelectionFlag(
	const RichTextEditorSelectionFormat& format, FontStyle flag
) {
	return RichTextUniformValue<bool>(
		format,
		[flag](const RichTextEditorResolvedStyle& value) {
			return HasFontFlag(value.style.flags, flag);
		}
	);
}

[[nodiscard]] std::optional<Color> RichTextSelectionColor(
	const RichTextEditorSelectionFormat& format
) {
	return RichTextUniformValue<Color>(
		format, [](const RichTextEditorResolvedStyle& value) { return value.style.color; }
	);
}

[[nodiscard]] std::optional<FontKey> RichTextSelectionFont(
	const RichTextEditorSelectionFormat& format
) {
	return RichTextUniformValue<FontKey>(
		format, [](const RichTextEditorResolvedStyle& value) { return value.font; }
	);
}

[[nodiscard]] std::optional<float> RichTextSelectionSize(
	const RichTextEditorSelectionFormat& format
) {
	return RichTextUniformValue<float>(
		format,
		[](const RichTextEditorResolvedStyle& value) { return value.style.size; },
		[](float a, float b) { return std::abs(a - b) <= 0.0001f; }
	);
}

[[nodiscard]] std::optional<GlyphEffectStyle> RichTextSelectionGlyphEffect(
	const RichTextEditorSelectionFormat& format
) {
	return RichTextUniformValue<GlyphEffectStyle>(
		format,
		[](const RichTextEditorResolvedStyle& value) { return value.style.effect; },
		RichTextEditorGlyphEffectEqual
	);
}

[[nodiscard]] std::optional<DistanceFieldLayerStyle> RichTextSelectionLayer(
	const RichTextEditorSelectionFormat& format,
	RichTextEffectEditorKind kind
) {
	return RichTextUniformValue<DistanceFieldLayerStyle>(
		format,
		[kind](const RichTextEditorResolvedStyle& value) -> DistanceFieldLayerStyle {
			switch (kind) {
				case RichTextEffectEditorKind::Outline: return value.style.sdf.outline;
				case RichTextEffectEditorKind::Shadow: return value.style.sdf.shadow;
				case RichTextEffectEditorKind::OuterGlow: return value.style.sdf.outer_glow;
				case RichTextEffectEditorKind::InnerGlow: return value.style.sdf.inner_glow;
				case RichTextEffectEditorKind::Glyph: break;
			}
			return {};
		}
	);
}

[[nodiscard]] std::optional<V2_float> RichTextSelectionShadowOffset(
	const RichTextEditorSelectionFormat& format
) {
	return RichTextUniformValue<V2_float>(
		format,
		[](const RichTextEditorResolvedStyle& value) {
			return value.style.sdf.shadow_offset;
		}
	);
}

[[nodiscard]] std::optional<FontStyle> RichTextTagFontFlag(std::string_view tag) {
	if (tag == "b") return FontStyle::Bold;
	if (tag == "i") return FontStyle::Italic;
	if (tag == "u") return FontStyle::Underline;
	if (tag == "s") return FontStyle::Strikethrough;
	return std::nullopt;
}

bool RemoveRichTextLocatedTag(
	std::string& source, RichTextSelectionState& state,
	const RichTextSurroundingTag& wrapper,
	std::size_t selection_begin, std::size_t selection_end
) {
	const std::size_t wrapper_end{
		wrapper.close_begin + wrapper.close_size
	};
	const bool selected_wrapper{
		selection_begin == wrapper.open_begin &&
		selection_end == wrapper_end
	};
	const std::size_t inner_size{
		wrapper.close_begin -
		(wrapper.open_begin + wrapper.open_size)
	};

	source.erase(wrapper.close_begin, wrapper.close_size);
	source.erase(wrapper.open_begin, wrapper.open_size);

	if (selected_wrapper) {
		const std::size_t selection_start{ wrapper.open_begin };
		const std::size_t selection_end_after{
			wrapper.open_begin + inner_size
		};
		RequestRichTextSelection(
			state,
			selection_end_after,
			selection_start,
			selection_end_after
		);
		return true;
	}

	if (selection_begin == selection_end) {
		const std::size_t cursor{
			selection_begin >= wrapper.open_size
				? selection_begin - wrapper.open_size
				: wrapper.open_begin
		};
		RequestRichTextSelection(state, cursor, cursor, cursor);
		return true;
	}

	const std::size_t selection_start{
		selection_begin >= wrapper.open_size
			? selection_begin - wrapper.open_size
			: wrapper.open_begin
	};
	const std::size_t selection_end_after{
		selection_end >= wrapper.open_size
			? selection_end - wrapper.open_size
			: selection_start
	};
	RequestRichTextSelection(
		state,
		selection_end_after,
		selection_start,
		selection_end_after
	);
	return true;
}

void AdjustRichTextSelectionForErase(
	RichTextSelectionState& state, std::size_t erase_begin, std::size_t erase_size
) {
	const std::size_t erase_end{ erase_begin + erase_size };
	auto adjust = [&](std::size_t& position) {
		if (position <= erase_begin) {
			return;
		}
		if (position >= erase_end) {
			position -= erase_size;
			return;
		}
		position = erase_begin;
	};

	adjust(state.cursor);
	adjust(state.selection_start);
	adjust(state.selection_end);
}

void EraseRichTextSourceRange(
	std::string& source, RichTextSelectionState& state,
	std::size_t erase_begin, std::size_t erase_size
) {
	source.erase(erase_begin, erase_size);
	AdjustRichTextSelectionForErase(state, erase_begin, erase_size);
}

[[nodiscard]] bool RichTextSelectionMatchesWrapper(
	const RichTextSurroundingTag& wrapper,
	std::size_t selection_begin,
	std::size_t selection_end
) {
	const std::size_t wrapper_end{ wrapper.close_begin + wrapper.close_size };
	const std::size_t inner_begin{ wrapper.open_begin + wrapper.open_size };
	return
		(selection_begin == wrapper.open_begin && selection_end == wrapper_end) ||
		(selection_begin == inner_begin && selection_end == wrapper.close_begin);
}

bool RemoveRichTextContainedTagWrappers(
	std::string& source, RichTextSelectionState& state, std::string_view tag
) {
	ClampRichTextSelection(state, source.size());
	std::size_t selection_begin{ std::min(state.selection_start, state.selection_end) };
	std::size_t selection_end{ std::max(state.selection_start, state.selection_end) };
	if (selection_begin >= selection_end) {
		return false;
	}

	struct OpenTag {
		std::size_t begin{};
		std::size_t end{};
	};
	std::vector<OpenTag> stack;
	std::vector<RichTextSurroundingTag> wrappers;

	for (std::size_t search_from{ selection_begin }; search_from < selection_end;) {
		std::size_t open{ source.find('<', search_from) };
		while (open != std::string::npos && open < selection_end &&
			IsEscapedRichTextCharacter(source, open)) {
			open = source.find('<', open + 1);
		}
		if (open == std::string::npos || open >= selection_end) {
			break;
		}

		const std::size_t close{ source.find('>', open + 1) };
		if (close == std::string::npos || close >= selection_end) {
			break;
		}

		std::string_view token{ source.substr(open + 1, close - open - 1) };
		while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front())) != 0) {
			token.remove_prefix(1);
		}
		while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back())) != 0) {
			token.remove_suffix(1);
		}
		if (token.empty()) {
			search_from = close + 1;
			continue;
		}

		const bool closing{ token.front() == '/' };
		if (closing) {
			token.remove_prefix(1);
		}
		const auto equals{ token.find('=') };
		std::string name{ token.substr(0, equals) };
		std::ranges::transform(name, name.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		if (name == "strike") name = "s";
		if (name == "color") name = "c";
		if (!RichTextTagNameMatches(name, tag)) {
			search_from = close + 1;
			continue;
		}

		if (!closing) {
			stack.push_back(OpenTag{ .begin = open, .end = close + 1 });
		} else if (!stack.empty()) {
			const OpenTag opening{ stack.back() };
			stack.pop_back();
			wrappers.push_back(RichTextSurroundingTag{
				.open_begin = opening.begin,
				.open_size = opening.end - opening.begin,
				.close_begin = open,
				.close_size = close - open + 1,
			});
		}
		search_from = close + 1;
	}

	if (wrappers.empty()) {
		return false;
	}

	// Remove every delimiter from right to left so nested wrappers cannot invalidate
	// the recorded source offsets of their parents.
	std::vector<std::pair<std::size_t, std::size_t>> erases;
	erases.reserve(wrappers.size() * 2);
	for (const auto& wrapper : wrappers) {
		erases.emplace_back(wrapper.open_begin, wrapper.open_size);
		erases.emplace_back(wrapper.close_begin, wrapper.close_size);
	}
	std::ranges::sort(erases, [](const auto& a, const auto& b) {
		return a.first > b.first;
	});
	for (const auto& [erase_begin, erase_size] : erases) {
		EraseRichTextSourceRange(source, state, erase_begin, erase_size);
	}
	return true;
}

[[nodiscard]] bool RichTextTagIgnoresWhitespaceForMerging(std::string_view tag) {
	// Spaces do not have a visible bold/italic state, so a whitespace-only gap does not
	// prevent neighboring bold/italic wrappers from being represented by one wrapper.
	// Underline and strikethrough intentionally do not get this exception because their
	// whitespace is visibly styled.
	return tag == "b" || tag == "i";
}

[[nodiscard]] bool RichTextWrapperMatchesExactMarkup(
	std::string_view source, const RichTextSurroundingTag& wrapper,
	std::string_view open, std::string_view close
) {
	return source.substr(wrapper.open_begin, wrapper.open_size) == open &&
		   source.substr(wrapper.close_begin, wrapper.close_size) == close;
}

bool MergeRichTextNeighboringTagWrappers(
	std::string& source, RichTextSelectionState& state, std::string_view tag,
	std::string_view open, std::string_view close
) {
	const bool ignore_whitespace{
		RichTextTagIgnoresWhitespaceForMerging(tag)
	};
	bool changed{ false };

	// Re-query the wrapper after every merge. Erasing either boundary changes all later
	// source offsets, and FindRichTextSurroundingTag gives us the new combined wrapper
	// around the still-selected text.
	while (true) {
		ClampRichTextSelection(state, source.size());

		const std::size_t selection_begin{
			std::min(state.selection_start, state.selection_end)
		};
		const std::size_t selection_end{
			std::max(state.selection_start, state.selection_end)
		};

		const auto current{
			FindRichTextSurroundingTag(
				source, selection_begin, selection_end, tag
			)
		};
		if (!current ||
			!RichTextWrapperMatchesExactMarkup(source, *current, open, close)) {
			break;
		}

		// Merge a matching wrapper on the left:
		// <b>left</b> [optional whitespace] <b>selection</b>
		std::size_t left_close_end{ current->open_begin };
		if (ignore_whitespace) {
			while (left_close_end > 0 &&
				std::isspace(
					static_cast<unsigned char>(source[left_close_end - 1])
				) != 0) {
				--left_close_end;
			}
		}

		if (left_close_end >= close.size()) {
			const std::size_t left_close_begin{
				left_close_end - close.size()
			};
			if (std::string_view{ source }.substr(
					left_close_begin, close.size()
				) == close) {
				const auto left_wrapper{
					FindRichTextSurroundingTag(
						source, left_close_begin, left_close_begin, tag
					)
				};
				if (left_wrapper &&
					left_wrapper->close_begin == left_close_begin &&
					RichTextWrapperMatchesExactMarkup(
						source, *left_wrapper, open, close
					)) {
					// Remove from right to left so the earlier close-tag offset remains valid.
					EraseRichTextSourceRange(
						source, state, current->open_begin, current->open_size
					);
					EraseRichTextSourceRange(
						source, state, left_wrapper->close_begin,
						left_wrapper->close_size
					);
					changed = true;
					continue;
				}
			}
		}

		// Merge a matching wrapper on the right:
		// <b>selection</b> [optional whitespace] <b>right</b>
		const std::size_t current_close_begin{ current->close_begin };
		const std::size_t current_close_size{ current->close_size };
		std::size_t right_open_begin{
			current_close_begin + current_close_size
		};
		if (ignore_whitespace) {
			while (right_open_begin < source.size() &&
				std::isspace(
					static_cast<unsigned char>(source[right_open_begin])
				) != 0) {
				++right_open_begin;
			}
		}

		if (right_open_begin + open.size() <= source.size() &&
			std::string_view{ source }.substr(
				right_open_begin, open.size()
			) == open) {
			const std::size_t right_inner_begin{
				right_open_begin + open.size()
			};
			const auto right_wrapper{
				FindRichTextSurroundingTag(
					source, right_inner_begin, right_inner_begin, tag
				)
			};
			if (right_wrapper &&
				right_wrapper->open_begin == right_open_begin &&
				RichTextWrapperMatchesExactMarkup(
					source, *right_wrapper, open, close
				)) {
				// Remove from right to left so the current close-tag offset remains valid.
				EraseRichTextSourceRange(
					source, state, right_wrapper->open_begin,
					right_wrapper->open_size
				);
				EraseRichTextSourceRange(
					source, state, current_close_begin, current_close_size
				);
				changed = true;
				continue;
			}
		}

		break;
	}

	return changed;
}

struct RichTextOpenMarkup {
	std::string name{};
	std::string open{};
	std::string close{};
};

[[nodiscard]] std::vector<RichTextOpenMarkup> GetRichTextOpenMarkupAt(
	std::string_view source, std::size_t position
) {
	position = std::min(position, source.size());
	std::vector<RichTextOpenMarkup> stack;

	for (std::size_t search_from{ 0 }; search_from < position;) {
		std::size_t open{ source.find('<', search_from) };
		while (open != std::string_view::npos && open < position &&
			IsEscapedRichTextCharacter(source, open)) {
			open = source.find('<', open + 1);
		}
		if (open == std::string_view::npos || open >= position) {
			break;
		}

		const std::size_t close{ source.find('>', open + 1) };
		if (close == std::string_view::npos || close >= position) {
			break;
		}

		std::string_view token{ source.substr(open + 1, close - open - 1) };
		while (!token.empty() &&
			std::isspace(static_cast<unsigned char>(token.front())) != 0) {
			token.remove_prefix(1);
		}
		while (!token.empty() &&
			std::isspace(static_cast<unsigned char>(token.back())) != 0) {
			token.remove_suffix(1);
		}
		if (token.empty()) {
			search_from = close + 1;
			continue;
		}

		const bool closing{ token.front() == '/' };
		if (closing) {
			token.remove_prefix(1);
			while (!token.empty() &&
				std::isspace(static_cast<unsigned char>(token.front())) != 0) {
				token.remove_prefix(1);
			}
		}

		const auto equals{ token.find('=') };
		std::string_view name{ token.substr(0, equals) };
		while (!name.empty() &&
			std::isspace(static_cast<unsigned char>(name.back())) != 0) {
			name.remove_suffix(1);
		}
		if (name.empty()) {
			search_from = close + 1;
			continue;
		}

		if (closing) {
			if (!stack.empty() &&
				(RichTextTagNameMatches(stack.back().name, name) ||
				 RichTextTagNameMatches(name, stack.back().name))) {
				stack.pop_back();
			}
		} else {
			stack.push_back(RichTextOpenMarkup{
				.name = std::string{ name },
				.open = std::string{ source.substr(open, close - open + 1) },
				.close = "</" + std::string{ name } + ">",
			});
		}

		search_from = close + 1;
	}

	return stack;
}

[[nodiscard]] std::string OpenRichTextMarkup(
	const std::vector<RichTextOpenMarkup>& stack
) {
	std::string result;
	for (const auto& entry : stack) {
		result += entry.open;
	}
	return result;
}

[[nodiscard]] std::string CloseRichTextMarkup(
	const std::vector<RichTextOpenMarkup>& stack
) {
	std::string result;
	for (auto it{ stack.rbegin() }; it != stack.rend(); ++it) {
		result += it->close;
	}
	return result;
}

void RemoveEmptyRichTextWrappers(std::string& source) {
	bool removed{ true };
	while (removed) {
		removed = false;
		for (std::size_t open{ 0 }; open < source.size();) {
			open = source.find('<', open);
			if (open == std::string::npos) {
				break;
			}
			if (IsEscapedRichTextCharacter(source, open)) {
				++open;
				continue;
			}

			const std::size_t open_end{ source.find('>', open + 1) };
			if (open_end == std::string::npos) {
				break;
			}

			std::string_view open_token{
				std::string_view{ source }.substr(open + 1, open_end - open - 1)
			};
			while (!open_token.empty() &&
				std::isspace(static_cast<unsigned char>(open_token.front())) != 0) {
				open_token.remove_prefix(1);
			}
			if (open_token.empty() || open_token.front() == '/') {
				open = open_end + 1;
				continue;
			}

			const std::size_t close_begin{ open_end + 1 };
			if (close_begin >= source.size() || source[close_begin] != '<') {
				open = open_end + 1;
				continue;
			}
			const std::size_t close_end{ source.find('>', close_begin + 1) };
			if (close_end == std::string::npos) {
				break;
			}

			std::string_view close_token{
				std::string_view{ source }.substr(
					close_begin + 1, close_end - close_begin - 1
				)
			};
			while (!close_token.empty() &&
				std::isspace(static_cast<unsigned char>(close_token.front())) != 0) {
				close_token.remove_prefix(1);
			}
			if (close_token.empty() || close_token.front() != '/') {
				open = open_end + 1;
				continue;
			}
			close_token.remove_prefix(1);
			while (!close_token.empty() &&
				std::isspace(static_cast<unsigned char>(close_token.front())) != 0) {
				close_token.remove_prefix(1);
			}
			while (!close_token.empty() &&
				std::isspace(static_cast<unsigned char>(close_token.back())) != 0) {
				close_token.remove_suffix(1);
			}

			const auto equals{ open_token.find('=') };
			std::string_view open_name{ open_token.substr(0, equals) };
			while (!open_name.empty() &&
				std::isspace(static_cast<unsigned char>(open_name.back())) != 0) {
				open_name.remove_suffix(1);
			}

			if (!open_name.empty() &&
				(RichTextTagNameMatches(open_name, close_token) ||
				 RichTextTagNameMatches(close_token, open_name))) {
				source.erase(open, close_end - open + 1);
				removed = true;
				break;
			}

			open = open_end + 1;
		}
	}
}

bool SplitRichTextWrapperAroundSelection(
	std::string& source, RichTextSelectionState& state,
	const RichTextSurroundingTag& wrapper,
	std::size_t selection_begin, std::size_t selection_end
) {
	const std::size_t inner_begin{ wrapper.open_begin + wrapper.open_size };
	const std::size_t inner_end{ wrapper.close_begin };
	if (selection_begin < inner_begin || selection_end > inner_end ||
		selection_begin > selection_end) {
		return false;
	}

	const std::string target_open{
		source.substr(wrapper.open_begin, wrapper.open_size)
	};
	const std::string target_close{
		source.substr(wrapper.close_begin, wrapper.close_size)
	};
	const std::string inner{ source.substr(inner_begin, inner_end - inner_begin) };
	const std::size_t local_begin{ selection_begin - inner_begin };
	const std::size_t local_end{ selection_end - inner_begin };

	const auto begin_stack{ GetRichTextOpenMarkupAt(inner, local_begin) };
	const auto end_stack{ GetRichTextOpenMarkupAt(inner, local_end) };

	std::string replacement;
	replacement.reserve(
		inner.size() + target_open.size() * 2 + target_close.size() * 2 + 64
	);

	// Preserve the target style on text before the selection, while balancing any
	// nested tags that cross the selection boundary.
	if (local_begin > 0) {
		std::string left{ target_open };
		left.append(inner.data(), local_begin);
		left += CloseRichTextMarkup(begin_stack);
		left += target_close;
		RemoveEmptyRichTextWrappers(left);
		replacement += left;
	}

	// Re-open every non-target style that was active at the selection start. The
	// target wrapper itself is intentionally omitted, so this middle region falls
	// back to the enclosing/default value without needing any '=off' markup.
	replacement += OpenRichTextMarkup(begin_stack);
	const std::size_t new_selection_begin{ wrapper.open_begin + replacement.size() };
	replacement.append(
		inner.data() + static_cast<std::ptrdiff_t>(local_begin),
		local_end - local_begin
	);
	const std::size_t new_selection_end{ wrapper.open_begin + replacement.size() };
	replacement += CloseRichTextMarkup(end_stack);

	// Preserve the target style after the selection and restore nested styles in
	// their original order before appending the untouched right-hand source.
	if (local_end < inner.size()) {
		std::string right{ target_open };
		right += OpenRichTextMarkup(end_stack);
		right.append(
			inner.data() + static_cast<std::ptrdiff_t>(local_end),
			inner.size() - local_end
		);
		right += target_close;
		RemoveEmptyRichTextWrappers(right);
		replacement += right;
	}

	const std::size_t wrapper_end{ wrapper.close_begin + wrapper.close_size };
	source.replace(
		wrapper.open_begin,
		wrapper_end - wrapper.open_begin,
		replacement
	);
	RequestRichTextSelection(
		state, new_selection_end, new_selection_begin, new_selection_end
	);
	return true;
}

bool RemoveRichTextSelectionTag(
	std::string& source, RichTextSelectionState& state, std::string_view tag
);

bool ToggleRichTextSelectionTag(
	std::string& source, RichTextSelectionState& state, std::string_view tag,
	std::string_view default_open, std::string_view close,
	const TextRunDefaults& defaults
) {
	ClampRichTextSelection(state, source.size());

	std::size_t begin{ std::min(state.selection_start, state.selection_end) };
	std::size_t end{ std::max(state.selection_start, state.selection_end) };
	const bool has_range{ begin < end };

	const auto format{ ResolveRichTextEditorSelectionFormat(source, state, defaults) };
	const auto flag{ RichTextTagFontFlag(tag) };
	const std::optional<bool> active{
		flag ? RichTextSelectionFlag(format, *flag) : std::nullopt
	};

	// Mixed selection follows conventional rich-text behavior: clicking the toggle
	// makes the entire selected range active. A uniformly active target removes the
	// surrounding tag. Adding a new style always requires a non-empty selection;
	// a collapsed caret is only allowed to remove a tag it is already inside.
	const bool desired_active{ !active.has_value() || !active.value() };
	if (!desired_active) {
		return RemoveRichTextSelectionTag(source, state, tag);
	}

	if (!has_range) {
		return false;
	}

	if (has_range) {
		// Remove same-kind overrides wholly contained by the selected range before
		// applying one authoritative wrapper. This makes Mixed -> On deterministic.
		RemoveRichTextContainedTagWrappers(source, state, tag);
		begin = std::min(state.selection_start, state.selection_end);
		end = std::max(state.selection_start, state.selection_end);
	}

	if (!WrapRichTextSelection(source, state, default_open, close)) {
		return false;
	}

	MergeRichTextNeighboringTagWrappers(
		source, state, tag, default_open, close
	);
	return true;
}

// Applies a parameterized rich text tag to exactly the selected range. Reapplying
// a value to an exact same-tag wrapper edits that wrapper in place; applying to a
// subset creates a nested override so surrounding text is never modified.
bool SetRichTextSelectionTag(
	std::string& source, RichTextSelectionState& state, std::string_view tag,
	std::string_view open, std::string_view close
) {
	ClampRichTextSelection(state, source.size());

	std::size_t begin{ std::min(state.selection_start, state.selection_end) };
	std::size_t end{ std::max(state.selection_start, state.selection_end) };
	const bool has_range{ begin < end };
	if (!has_range) {
		const std::size_t cursor{ std::min(state.cursor, source.size()) };
		const auto wrapper{
			FindRichTextSurroundingTag(source, cursor, cursor, tag)
		};
		if (!wrapper) {
			// Never create an empty formatting/effect wrapper at a caret. A selection
			// is required to add a new tag; a caret may only edit an existing wrapper.
			return false;
		}

		const std::string current_open{
			source.substr(wrapper->open_begin, wrapper->open_size)
		};
		if (current_open == open) {
			return false;
		}

		source.replace(wrapper->open_begin, wrapper->open_size, open);
		const std::ptrdiff_t delta{
			static_cast<std::ptrdiff_t>(open.size()) -
			static_cast<std::ptrdiff_t>(wrapper->open_size)
		};

		// The opening tag is before the caret, so keep the caret on the same visible
		// character after changing the tag's source length.
		const std::size_t new_cursor{
			static_cast<std::size_t>(
				static_cast<std::ptrdiff_t>(cursor) + delta
			)
		};
		RequestRichTextSelection(state, new_cursor, new_cursor, new_cursor);
		return true;
	}

	RemoveRichTextContainedTagWrappers(source, state, tag);
	begin = std::min(state.selection_start, state.selection_end);
	end = std::max(state.selection_start, state.selection_end);

	if (const auto wrapper{ FindRichTextSurroundingTag(source, begin, end, tag) };
		wrapper && RichTextSelectionMatchesWrapper(*wrapper, begin, end)) {
		const std::string current_open{
			source.substr(wrapper->open_begin, wrapper->open_size)
		};
		if (current_open == open) {
			return MergeRichTextNeighboringTagWrappers(source, state, tag, open, close);
		}

		const bool selected_wrapper{
			begin == wrapper->open_begin &&
			end == wrapper->close_begin + wrapper->close_size
		};
		source.replace(wrapper->open_begin, wrapper->open_size, open);
		const std::ptrdiff_t delta{
			static_cast<std::ptrdiff_t>(open.size()) -
			static_cast<std::ptrdiff_t>(wrapper->open_size)
		};
		const auto shift = [delta](std::size_t position) {
			return static_cast<std::size_t>(
				static_cast<std::ptrdiff_t>(position) + delta
			);
		};

		if (selected_wrapper) {
			const std::size_t selection_start{ wrapper->open_begin };
			const std::size_t selection_end{
				static_cast<std::size_t>(
					static_cast<std::ptrdiff_t>(end) + delta
				)
			};
			RequestRichTextSelection(
				state, selection_end, selection_start, selection_end
			);
		} else {
			const std::size_t selection_start{ shift(begin) };
			const std::size_t selection_end{ shift(end) };
			RequestRichTextSelection(
				state, selection_end, selection_start, selection_end
			);
		}

		MergeRichTextNeighboringTagWrappers(source, state, tag, open, close);
		return true;
	}

	if (!WrapRichTextSelection(source, state, open, close)) {
		return false;
	}
	MergeRichTextNeighboringTagWrappers(source, state, tag, open, close);
	return true;
}

bool RemoveRichTextSelectionTag(
	std::string& source, RichTextSelectionState& state, std::string_view tag
) {
	ClampRichTextSelection(state, source.size());
	bool changed{ false };

	std::size_t begin{ std::min(state.selection_start, state.selection_end) };
	std::size_t end{ std::max(state.selection_start, state.selection_end) };

	// With a collapsed caret, removal targets the complete innermost wrapper that
	// contains the caret. This is what lets <b>Hello</b> show Bold as active anywhere
	// inside "Hello" and lets one click remove that authored tag without a selection.
	if (begin == end) {
		const std::size_t cursor{ std::min(state.cursor, source.size()) };
		if (const auto wrapper{
				FindRichTextSurroundingTag(source, cursor, cursor, tag)
			}) {
			return RemoveRichTextLocatedTag(
				source, state, *wrapper, cursor, cursor
			);
		}
		return false;
	}

	changed |= RemoveRichTextContainedTagWrappers(source, state, tag);

	// Remove every same-kind wrapper affecting the target. If the target is only a
	// subset of a wrapper, split that wrapper into styled left/right segments and
	// leave the middle unwrapped. This produces clean source such as
	// <b>left</b>plain<b>right</b> rather than introducing <b=off> tags.
	while (true) {
		ClampRichTextSelection(state, source.size());
		begin = std::min(state.selection_start, state.selection_end);
		end = std::max(state.selection_start, state.selection_end);

		const auto wrapper{ FindRichTextSurroundingTag(source, begin, end, tag) };
		if (!wrapper) {
			break;
		}

		if (RichTextSelectionMatchesWrapper(*wrapper, begin, end)) {
			changed |= RemoveRichTextLocatedTag(source, state, *wrapper, begin, end);
			continue;
		}

		const std::size_t inner_begin{ wrapper->open_begin + wrapper->open_size };
		const std::size_t inner_end{ wrapper->close_begin };
		if (begin < inner_begin || end > inner_end) {
			break;
		}

		if (!SplitRichTextWrapperAroundSelection(
				source, state, *wrapper, begin, end
			)) {
			break;
		}
		changed = true;
	}

	return changed;
}

bool RemoveRichTextSelectionEffects(
	std::string& source, RichTextSelectionState& state
) {
	static constexpr std::array<std::string_view, 5> kEffectTags{
		"outline", "shadow", "outerglow", "innerglow", "fx",
	};

	bool changed{ false };
	for (const auto tag : kEffectTags) {
		changed |= RemoveRichTextSelectionTag(source, state, tag);
	}
	return changed;
}

bool InsertRichTextToken(
	std::string& source, RichTextSelectionState& state, std::string_view token
) {
	ClampRichTextSelection(state, source.size());

	std::size_t begin{ std::min(state.selection_start, state.selection_end) };
	std::size_t end{ std::max(state.selection_start, state.selection_end) };
	if (begin == end) {
		begin = state.cursor;
		end = state.cursor;
	}

	source.replace(begin, end - begin, token);
	const std::size_t cursor{ begin + token.size() };
	RequestRichTextSelection(state, cursor, cursor, cursor);
	return true;
}

[[nodiscard]] std::string_view TrimRichTextStandaloneLine(std::string_view line) {
	while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) {
		line.remove_prefix(1);
	}
	while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
		line.remove_suffix(1);
	}
	return line;
}

[[nodiscard]] std::pair<std::size_t, std::size_t> RichTextLogicalLineBounds(
	std::string_view source, std::size_t position
) {
	position = std::min(position, source.size());
	const std::size_t begin_search{ position == 0 ? 0 : position - 1 };
	const std::size_t previous{ position == 0 ? std::string_view::npos : source.rfind('\n', begin_search) };
	const std::size_t begin{ previous == std::string_view::npos ? 0 : previous + 1 };
	const std::size_t next{ source.find('\n', position) };
	return {
		begin,
		next == std::string_view::npos ? source.size() : next
	};
}

[[nodiscard]] bool RichTextLineEqualsToken(
	std::string_view source, std::size_t begin, std::size_t end, std::string_view token
) {
	if (begin > end || end > source.size()) {
		return false;
	}
	return TrimRichTextStandaloneLine(source.substr(begin, end - begin)) == token;
}

bool ToggleRichTextStandaloneLineToken(
	std::string& source, RichTextSelectionState& state, std::string_view token
) {
	if (token.empty()) {
		return false;
	}

	ClampRichTextSelection(state, source.size());
	const std::size_t selection_begin{
		std::min(state.selection_start, state.selection_end)
	};
	const std::size_t anchor{
		state.selection_start != state.selection_end
			? selection_begin
			: state.cursor
	};
	const auto [line_begin, line_end]{
		RichTextLogicalLineBounds(source, anchor)
	};
	const std::string_view line{
		std::string_view{ source }.substr(line_begin, line_end - line_begin)
	};
	const std::string_view trimmed{ TrimRichTextStandaloneLine(line) };

	if (trimmed == token) {
		std::size_t erase_end{ line_end };
		if (line_begin == 0 && erase_end < source.size() &&
			source[erase_end] == '\n') {
			++erase_end;
		}
		source.erase(line_begin, erase_end - line_begin);
		RequestRichTextSelection(state, line_begin, line_begin, line_begin);
		return true;
	}

	if (trimmed.empty()) {
		source.replace(line_begin, line_end - line_begin, token);
		const std::size_t cursor{ line_begin + token.size() };
		RequestRichTextSelection(state, cursor, cursor, cursor);
		return true;
	}

	// For text selection/caret, resolve the beginning of the surrounding authored paragraph.
	// A paragraph is bounded by a blank line or an existing standalone control line.
	std::size_t paragraph_begin{ line_begin };
	while (paragraph_begin > 0) {
		const std::size_t previous_newline{ paragraph_begin - 1 };
		const std::size_t previous_line_end{ previous_newline };
		const std::size_t before_previous{
			previous_line_end == 0
				? std::string_view::npos
				: std::string_view{ source }.rfind('\n', previous_line_end - 1)
		};
		const std::size_t previous_line_begin{
			before_previous == std::string_view::npos ? 0 : before_previous + 1
		};
		const std::string_view previous_line{
			std::string_view{ source }.substr(
				previous_line_begin,
				previous_line_end - previous_line_begin
			)
		};
		const std::string_view previous_trimmed{
			TrimRichTextStandaloneLine(previous_line)
		};
		if (previous_trimmed.empty() || previous_trimmed == token) {
			break;
		}
		paragraph_begin = previous_line_begin;
	}

	// If this paragraph already has the control immediately above it, toggle it off.
	if (paragraph_begin > 0) {
		const std::size_t previous_line_end{ paragraph_begin - 1 };
		const std::size_t before_previous{
			previous_line_end == 0
				? std::string_view::npos
				: std::string_view{ source }.rfind('\n', previous_line_end - 1)
		};
		const std::size_t previous_line_begin{
			before_previous == std::string_view::npos ? 0 : before_previous + 1
		};
		if (RichTextLineEqualsToken(
				source, previous_line_begin, previous_line_end, token
			)) {
			std::size_t erase_end{ previous_line_end };
			if (previous_line_begin == 0 && erase_end < source.size() &&
				source[erase_end] == '\n') {
				++erase_end;
			}
			const std::size_t erased_size{ erase_end - previous_line_begin };
			source.erase(previous_line_begin, erased_size);
			const std::size_t adjusted{
				anchor >= erase_end
					? anchor - erased_size
					: previous_line_begin
			};
			RequestRichTextSelection(state, adjusted, adjusted, adjusted);
			return true;
		}
	}

	const std::string insertion{ std::string{ token } + "\n" };
	source.insert(paragraph_begin, insertion);
	const std::size_t cursor{ anchor + insertion.size() };
	RequestRichTextSelection(state, cursor, cursor, cursor);
	return true;
}

struct RichTextInputCallbackContext {
	RichTextSelectionState* selection{ nullptr };
	RichTextEditorState* editor_state{ nullptr };
	const TextRunDefaults* defaults{ nullptr };
	bool action_applied{ false };
};

int RichTextInputCallback(ImGuiInputTextCallbackData* data) {
	auto* context{ static_cast<RichTextInputCallbackContext*>(data->UserData) };
	if (!context || !context->selection) {
		return 0;
	}

	auto& state{ *context->selection };

	const auto clamp_position = [data](std::size_t position) {
		return std::max(
			0,
			static_cast<int>(std::min(position, static_cast<std::size_t>(data->BufTextLen)))
		);
	};

	if (state.apply_selection) {
		data->CursorPos = clamp_position(state.cursor);
		data->SelectionStart = clamp_position(state.selection_start);
		data->SelectionEnd = clamp_position(state.selection_end);
		state.apply_selection = false;
	}

	if (context->editor_state) {
		const auto& io{ ImGui::GetIO() };
		const bool command_modifier{ io.KeyCtrl || io.KeySuper };
		const bool undo_pressed{
			command_modifier &&
			ImGui::IsKeyPressed(ImGuiKey_Z, false) &&
			!io.KeyShift
		};
		const bool redo_pressed{
			command_modifier &&
			(
				(ImGui::IsKeyPressed(ImGuiKey_Z, false) && io.KeyShift) ||
				ImGui::IsKeyPressed(ImGuiKey_Y, false)
			)
		};

		if (undo_pressed || redo_pressed) {
			context->action_applied |= ApplyRichTextSourceHistory(
				*data,
				*context->editor_state,
				state,
				undo_pressed ? -1 : 1
			);
		}
	}

	if (state.pending_action.has_value()) {
		auto action{ std::move(state.pending_action.value()) };
		state.pending_action.reset();

		RichTextSelectionState action_selection{
			.cursor = action.cursor,
			.selection_start = action.selection_start,
			.selection_end = action.selection_end,
		};
		std::string edited_source{ data->Buf, static_cast<std::size_t>(data->BufTextLen) };

		if (context->editor_state) {
			PreserveRichTextHistorySelectionBeforeAction(
				*context->editor_state, edited_source, action
			);
		}

		bool changed{ false };
		switch (action.type) {
			case RichTextPendingActionType::ToggleTag:
				changed = ToggleRichTextSelectionTag(
					edited_source, action_selection, action.tag, action.open, action.close,
					context->defaults ? *context->defaults : TextRunDefaults{}
				);
				break;

			case RichTextPendingActionType::SetTag:
				changed = SetRichTextSelectionTag(
					edited_source, action_selection, action.tag, action.open, action.close
				);
				break;

			case RichTextPendingActionType::RemoveTag:
				changed = RemoveRichTextSelectionTag(
					edited_source, action_selection, action.tag
				);
				break;

			case RichTextPendingActionType::RemoveEffects:
				changed = RemoveRichTextSelectionEffects(edited_source, action_selection);
				break;

			case RichTextPendingActionType::InsertToken:
				changed = InsertRichTextToken(edited_source, action_selection, action.token);
				break;

			case RichTextPendingActionType::ToggleStandaloneLineToken:
				changed = ToggleRichTextStandaloneLineToken(
					edited_source, action_selection, action.token
				);
				break;
		}

		if (changed) {
			data->DeleteChars(0, data->BufTextLen);
			data->InsertChars(0, edited_source.c_str());
			context->action_applied = true;
		}

		ClampRichTextSelection(action_selection, static_cast<std::size_t>(data->BufTextLen));
		data->CursorPos = clamp_position(action_selection.cursor);
		data->SelectionStart = clamp_position(action_selection.selection_start);
		data->SelectionEnd = clamp_position(action_selection.selection_end);

		state.cursor = static_cast<std::size_t>(data->CursorPos);
		state.selection_start = static_cast<std::size_t>(data->SelectionStart);
		state.selection_end = static_cast<std::size_t>(data->SelectionEnd);
		state.apply_selection = false;
	}

	state.cursor = static_cast<std::size_t>(std::max(data->CursorPos, 0));
	state.selection_start = static_cast<std::size_t>(std::max(data->SelectionStart, 0));
	state.selection_end = static_cast<std::size_t>(std::max(data->SelectionEnd, 0));
	return 0;
}

bool ApplyPendingRichTextActionToInactiveSource(
	std::string& source, RichTextSelectionState& state, RichTextEditorState& editor_state,
	const TextRunDefaults& defaults
) {
	if (!state.pending_action.has_value()) {
		return false;
	}

	auto action{ std::move(state.pending_action.value()) };
	state.pending_action.reset();
	RichTextSelectionState action_selection{
		.cursor = action.cursor,
		.selection_start = action.selection_start,
		.selection_end = action.selection_end,
	};

	PreserveRichTextHistorySelectionBeforeAction(editor_state, source, action);

	bool changed{ false };
	switch (action.type) {
		case RichTextPendingActionType::ToggleTag:
			changed = ToggleRichTextSelectionTag(
				source, action_selection, action.tag, action.open, action.close, defaults
			);
			break;

		case RichTextPendingActionType::SetTag:
			changed = SetRichTextSelectionTag(
				source, action_selection, action.tag, action.open, action.close
			);
			break;

		case RichTextPendingActionType::RemoveTag:
			changed = RemoveRichTextSelectionTag(source, action_selection, action.tag);
			break;

		case RichTextPendingActionType::RemoveEffects:
			changed = RemoveRichTextSelectionEffects(source, action_selection);
			break;

		case RichTextPendingActionType::InsertToken:
			changed = InsertRichTextToken(source, action_selection, action.token);
			break;

		case RichTextPendingActionType::ToggleStandaloneLineToken:
			changed = ToggleRichTextStandaloneLineToken(
				source, action_selection, action.token
			);
			break;
	}

	state.cursor = action_selection.cursor;
	state.selection_start = action_selection.selection_start;
	state.selection_end = action_selection.selection_end;
	state.apply_selection = true;
	return changed;
}

std::string RichTextEditorColorTag(const std::array<float, 4>& value) {
	auto byte = [](float component) {
		return static_cast<std::uint8_t>(
			std::clamp(std::lround(component * 255.0f), 0l, 255l)
		);
	};

	constexpr char digits[]{ "0123456789ABCDEF" };
	const std::array<std::uint8_t, 4> rgba{
		byte(value[0]), byte(value[1]), byte(value[2]), byte(value[3])
	};

	std::string result{ "#00000000" };
	for (std::size_t i{ 0 }; i < rgba.size(); ++i) {
		result[1 + i * 2] = digits[(rgba[i] >> 4) & 0x0F];
		result[2 + i * 2] = digits[rgba[i] & 0x0F];
	}

	if (rgba[3] == 255) {
		result.resize(7);
	}
	return result;
}

void SetRichTextEditorColor(std::array<float, 4>& destination, Color value) {
	destination = {
		static_cast<float>(value.r) / 255.0f,
		static_cast<float>(value.g) / 255.0f,
		static_cast<float>(value.b) / 255.0f,
		static_cast<float>(value.a) / 255.0f,
	};
}

struct RichTextFontAvailability {
	bool exists{ false };
	bool loaded{ false };
	bool project_preloaded{ false };
	bool selected_scene_dependency{ false };

	[[nodiscard]] bool IsAvailable() const {
		return exists && (loaded || project_preloaded || selected_scene_dependency);
	}
};

[[nodiscard]] RichTextFontAvailability GetRichTextFontAvailability(
	EditorContext& ctx, const FontKey& font_key
) {
	// The empty key is the built-in/default font and is always available.
	if (font_key.value.empty()) {
		return RichTextFontAvailability{
			.exists = true,
			.loaded = true,
			.project_preloaded = true,
			.selected_scene_dependency = true,
		};
	}

	RichTextFontAvailability result;
	::ptgn::impl::AssetAccessor assets{ ctx.editor.GetAssetManager() };
	const auto records{ assets.GetAssets() };
	for (const auto& record : records) {
		if (record.kind != AssetKind::Font || record.key.value != font_key.value) {
			continue;
		}

		result.exists = true;
		result.loaded = result.loaded || record.load_state == AssetLoadState::Loaded;
		result.project_preloaded = result.project_preloaded || record.globally_pinned;
	}

	// TryGet is authoritative for resident font objects, including non-cataloged fonts.
	if (assets.TryGet<Font>(font_key).has_value()) {
		result.exists = true;
		result.loaded = true;
	}

	if (const Scene* selected_scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() }) {
		result.selected_scene_dependency = selected_scene->HasAssetDependency(font_key);
	}

	return result;
}

[[nodiscard]] const char* RichTextFontAvailabilityTooltip(
	const RichTextFontAvailability& availability
) {
	if (!availability.exists) {
		return "Font key does not exist.";
	}
	if (!availability.IsAvailable()) {
		return "Font is not loaded or referenced by the project preload / selected scene.";
	}
	return nullptr;
}

[[nodiscard]] std::vector<std::string> GetLoadedRichTextFontKeys(EditorContext& ctx) {
	std::vector<std::string> keys;
	keys.emplace_back(kDefaultFont);

	::ptgn::impl::AssetAccessor assets{ ctx.editor.GetAssetManager() };
	const auto records{ assets.GetAssets() };
	for (const auto& record : records) {
		if (record.kind != AssetKind::Font || record.key.value.empty()) {
			continue;
		}

		const FontKey key{ record.key.value };
		if (!assets.TryGet<Font>(key).has_value() || std::ranges::contains(keys, key.value)) {
			continue;
		}

		keys.emplace_back(key.value);
	}

	if (keys.size() > 1) {
		std::ranges::sort(keys.begin() + 1, keys.end());
	}
	return keys;
}

void StepRichTextCommonFontSize(float& size, int direction) {
	if (direction > 0) {
		for (const float common_size : kRichTextCommonFontSizes) {
			if (common_size > size + 0.001f) {
				size = common_size;
				return;
			}
		}
		size = kRichTextCommonFontSizes.back();
		return;
	}

	for (auto it{ kRichTextCommonFontSizes.rbegin() }; it != kRichTextCommonFontSizes.rend(); ++it) {
		if (*it < size - 0.001f) {
			size = *it;
			return;
		}
	}
	size = kRichTextCommonFontSizes.front();
}

struct RichTextUtf8Codepoint {
	std::uint32_t value{ 0 };
	std::size_t length{ 1 };
};

[[nodiscard]] RichTextUtf8Codepoint DecodeRichTextUtf8At(
	std::string_view source, std::size_t position
) {
	if (position >= source.size()) {
		return {};
	}

	const auto first{ static_cast<unsigned char>(source[position]) };
	if ((first & 0x80u) == 0u) {
		return { first, 1 };
	}

	std::size_t length{ 1 };
	std::uint32_t value{ first };
	if ((first & 0xE0u) == 0xC0u) {
		length = 2;
		value = first & 0x1Fu;
	} else if ((first & 0xF0u) == 0xE0u) {
		length = 3;
		value = first & 0x0Fu;
	} else if ((first & 0xF8u) == 0xF0u) {
		length = 4;
		value = first & 0x07u;
	} else {
		return { first, 1 };
	}

	if (position + length > source.size()) {
		return { first, 1 };
	}
	for (std::size_t i{ 1 }; i < length; ++i) {
		const auto continuation{ static_cast<unsigned char>(source[position + i]) };
		if ((continuation & 0xC0u) != 0x80u) {
			return { first, 1 };
		}
		value = (value << 6u) | (continuation & 0x3Fu);
	}
	return { value, length };
}

[[nodiscard]] bool IsRichTextCombiningCodepoint(std::uint32_t cp) {
	return
		(cp >= 0x0300u && cp <= 0x036Fu) ||
		(cp >= 0x0483u && cp <= 0x0489u) ||
		(cp >= 0x0591u && cp <= 0x05BDu) ||
		cp == 0x05BFu ||
		(cp >= 0x05C1u && cp <= 0x05C2u) ||
		(cp >= 0x05C4u && cp <= 0x05C5u) ||
		cp == 0x05C7u ||
		(cp >= 0x0610u && cp <= 0x061Au) ||
		(cp >= 0x064Bu && cp <= 0x065Fu) ||
		cp == 0x0670u ||
		(cp >= 0x06D6u && cp <= 0x06EDu) ||
		(cp >= 0x1AB0u && cp <= 0x1AFFu) ||
		(cp >= 0x1DC0u && cp <= 0x1DFFu) ||
		(cp >= 0x20D0u && cp <= 0x20FFu) ||
		(cp >= 0xFE20u && cp <= 0xFE2Fu);
}

[[nodiscard]] bool IsRichTextVariationSelector(std::uint32_t cp) {
	return (cp >= 0xFE00u && cp <= 0xFE0Fu) ||
		(cp >= 0xE0100u && cp <= 0xE01EFu);
}

[[nodiscard]] bool IsRichTextEmojiModifier(std::uint32_t cp) {
	return cp >= 0x1F3FBu && cp <= 0x1F3FFu;
}

[[nodiscard]] bool IsRichTextRegionalIndicator(std::uint32_t cp) {
	return cp >= 0x1F1E6u && cp <= 0x1F1FFu;
}

[[nodiscard]] std::size_t NextRichTextGraphemeBoundary(
	std::string_view source, std::size_t position
) {
	position = std::min(position, source.size());
	if (position >= source.size()) {
		return source.size();
	}

	const auto first{ DecodeRichTextUtf8At(source, position) };
	std::size_t cursor{ std::min(source.size(), position + first.length) };

	// Treat CRLF as one user-visible cluster.
	if (first.value == '\r' && cursor < source.size()) {
		const auto next{ DecodeRichTextUtf8At(source, cursor) };
		if (next.value == '\n') {
			return std::min(source.size(), cursor + next.length);
		}
	}

	// Regional indicators form flag pairs.
	if (IsRichTextRegionalIndicator(first.value) && cursor < source.size()) {
		const auto next{ DecodeRichTextUtf8At(source, cursor) };
		if (IsRichTextRegionalIndicator(next.value)) {
			cursor = std::min(source.size(), cursor + next.length);
		}
	}

	while (cursor < source.size()) {
		const auto next{ DecodeRichTextUtf8At(source, cursor) };
		if (IsRichTextCombiningCodepoint(next.value) ||
			IsRichTextVariationSelector(next.value) ||
			IsRichTextEmojiModifier(next.value) ||
			next.value == 0x20E3u) {
			cursor = std::min(source.size(), cursor + next.length);
			continue;
		}

		if (next.value == 0x200Du) {
			// Keep a zero-width-joiner and the following base character in the same
			// grapheme, then continue so that base's modifiers are consumed too.
			cursor = std::min(source.size(), cursor + next.length);
			if (cursor < source.size()) {
				const auto joined{ DecodeRichTextUtf8At(source, cursor) };
				cursor = std::min(source.size(), cursor + joined.length);
			}
			continue;
		}
		break;
	}

	return std::max(position + 1, cursor);
}

[[nodiscard]] std::size_t PreviousRichTextGraphemeBoundary(
	std::string_view source, std::size_t position
) {
	position = std::min(position, source.size());
	if (position == 0) {
		return 0;
	}

	std::size_t previous{ 0 };
	for (std::size_t cursor{ 0 }; cursor < position;) {
		const std::size_t next{ std::min(
			position, NextRichTextGraphemeBoundary(source, cursor)
		) };
		if (next >= position) {
			return cursor;
		}
		previous = cursor;
		cursor = std::max(next, cursor + 1);
	}
	return previous;
}

[[nodiscard]] bool IsRichTextSourceWordGrapheme(
	std::string_view source, std::size_t begin
) {
	if (begin >= source.size()) {
		return false;
	}
	const auto cp{ DecodeRichTextUtf8At(source, begin).value };
	if (cp >= 0x80u) {
		// Preserve the editor's previous behavior for non-ASCII scripts while making
		// selection operate on complete grapheme clusters instead of raw UTF-8 bytes.
		return true;
	}
	return std::isalnum(static_cast<unsigned char>(cp)) != 0 || cp == '_';
}

struct RichTextSourceVisualLine {
	std::size_t begin{ 0 };
	std::size_t end{ 0 };
};

[[nodiscard]] ImU32 RichTextSourceTextColor() {
	return IM_COL32(255, 255, 255, 255);
}

[[nodiscard]] ImU32 RichTextSourceTagColor() {
	return IM_COL32(0, 0, 0, 255);
}

[[nodiscard]] std::vector<ImU32> BuildRichTextSourceColors(
	std::string_view source, const TextRunDefaults&
) {
	const ImU32 text_color{ RichTextSourceTextColor() };
	const ImU32 tag_color{ RichTextSourceTagColor() };
	std::vector<ImU32> colors(source.size(), text_color);

	for (std::size_t i{ 0 }; i < source.size();) {
		// Escaped markup is ordinary input text, not syntax markup.
		if (source[i] == '\\' && i + 1 < source.size() &&
			(source[i + 1] == '<' || source[i + 1] == '>' || source[i + 1] == '\\')) {
			i += 2;
			continue;
		}

		if (source[i] != '<' || IsEscapedRichTextCharacter(source, i)) {
			++i;
			continue;
		}

		const std::size_t close{ source.find('>', i + 1) };
		if (close == std::string_view::npos) {
			std::fill(
				colors.begin() + static_cast<std::ptrdiff_t>(i), colors.end(), tag_color
			);
			break;
		}

		std::fill(
			colors.begin() + static_cast<std::ptrdiff_t>(i),
			colors.begin() + static_cast<std::ptrdiff_t>(close + 1),
			tag_color
		);
		i = close + 1;
	}

	return colors;
}

struct RichTextSourceTagToken {
	std::size_t begin{ 0 };
	std::size_t end{ 0 };
	std::string name{};
	std::optional<std::string> argument{};
	bool closing{ false };
};

[[nodiscard]] std::optional<RichTextSourceTagToken> ParseRichTextSourceTagToken(
	std::string_view source, std::size_t begin, std::size_t limit
) {
	if (begin >= limit || source[begin] != '<' || IsEscapedRichTextCharacter(source, begin)) {
		return std::nullopt;
	}

	const std::size_t close{ source.find('>', begin + 1) };
	if (close == std::string_view::npos || close >= limit) {
		return std::nullopt;
	}

	std::size_t token_begin{ begin + 1 };
	std::size_t token_end{ close };
	while (token_begin < token_end &&
		std::isspace(static_cast<unsigned char>(source[token_begin])) != 0) {
		++token_begin;
	}
	while (token_end > token_begin &&
		std::isspace(static_cast<unsigned char>(source[token_end - 1])) != 0) {
		--token_end;
	}
	if (token_begin >= token_end) {
		return RichTextSourceTagToken{ .begin = begin, .end = close + 1 };
	}

	bool closing{ source[token_begin] == '/' };
	if (closing) {
		++token_begin;
	}

	const std::size_t equals{ source.find('=', token_begin) };
	const std::size_t name_end{
		equals != std::string_view::npos && equals < token_end ? equals : token_end
	};
	std::string_view name{ source.substr(token_begin, name_end - token_begin) };
	while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())) != 0) {
		name.remove_suffix(1);
	}
	std::string canonical_name{ name };
	std::ranges::transform(canonical_name, canonical_name.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	if (canonical_name == "strike") canonical_name = "s";
	if (canonical_name == "color") canonical_name = "c";

	std::optional<std::string> argument;
	if (!closing && equals != std::string_view::npos && equals < token_end) {
		std::string_view value{ source.substr(equals + 1, token_end - equals - 1) };
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
			value.remove_prefix(1);
		}
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
			value.remove_suffix(1);
		}
		argument = std::string{ value };
	}

	return RichTextSourceTagToken{
		.begin = begin,
		.end = close + 1,
		.name = std::move(canonical_name),
		.argument = std::move(argument),
		.closing = closing,
	};
}

struct RichTextEditorDiagnostic {
	std::size_t position{ 0 };
	std::size_t length{ 1 };
	std::string message{};
};

[[nodiscard]] std::size_t RichTextEditorDiagnosticLength(
	std::string_view source, std::size_t position
) {
	position = std::min(position, source.size());
	if (position >= source.size()) {
		return 1;
	}
	if (source[position] == '<' && !IsEscapedRichTextCharacter(source, position)) {
		if (const std::size_t close{ source.find('>', position + 1) };
			close != std::string_view::npos) {
			return close - position + 1;
		}
		return source.size() - position;
	}
	return std::max<std::size_t>(
		1, NextRichTextGraphemeBoundary(source, position) - position
	);
}

[[nodiscard]] std::vector<RichTextEditorDiagnostic> BuildRichTextEditorDiagnostics(
	EditorContext& ctx, std::string_view source, const TextRunDefaults& defaults,
	const RichTextEditorOptions& options
) {
	std::vector<RichTextEditorDiagnostic> diagnostics;

	for (const auto& diagnostic : ParseRichText(source, defaults).diagnostics) {
		diagnostics.push_back(RichTextEditorDiagnostic{
			.position = diagnostic.position,
			.length = RichTextEditorDiagnosticLength(source, diagnostic.position),
			.message = diagnostic.message,
		});
	}

	// Variables are context-sensitive. An unterminated expression is always suspicious;
	// an unknown name is only an error when the caller supplied an explicit variable set.
	for (std::size_t i{ 0 }; i < source.size();) {
		if (source[i] != '$' || i + 1 >= source.size() || source[i + 1] != '{') {
			++i;
			continue;
		}

		const std::size_t close{ source.find('}', i + 2) };
		if (close == std::string_view::npos) {
			diagnostics.push_back(RichTextEditorDiagnostic{
				.position = i,
				.length = source.size() - i,
				.message = "Unterminated rich text variable.",
			});
			break;
		}

		const std::string_view name{ source.substr(i + 2, close - i - 2) };
		if (!options.variables.empty()) {
			const bool known{ std::ranges::any_of(
				options.variables,
				[name](const RichTextVariableOption& option) {
					return option.variable == name;
				}
			) };
			if (!known) {
				diagnostics.push_back(RichTextEditorDiagnostic{
					.position = i,
					.length = close - i + 1,
					.message = name.empty()
						? "Empty rich text variable."
						: "Unknown rich text variable '${" + std::string{ name } + "}'.",
				});
			}
		}
		i = close + 1;
	}

	// Font keys are syntactically valid to the runtime parser even when their assets do
	// not exist. Surface that authoring problem directly in the source editor.
	for (std::size_t i{ 0 }; i < source.size();) {
		const std::size_t open{ source.find('<', i) };
		if (open == std::string_view::npos) {
			break;
		}
		const auto token{ ParseRichTextSourceTagToken(source, open, source.size()) };
		if (!token.has_value()) {
			i = open + 1;
			continue;
		}
		i = token->end;
		if (token->closing || token->name != "font" || !token->argument.has_value()) {
			continue;
		}

		const FontKey key{ token->argument.value() };
		const auto availability{ GetRichTextFontAvailability(ctx, key) };
		if (const char* message{ RichTextFontAvailabilityTooltip(availability) }) {
			diagnostics.push_back(RichTextEditorDiagnostic{
				.position = token->begin,
				.length = token->end - token->begin,
				.message = message,
			});
		}
	}

	std::ranges::sort(diagnostics, [](const auto& a, const auto& b) {
		if (a.position != b.position) return a.position < b.position;
		if (a.length != b.length) return a.length < b.length;
		return a.message < b.message;
	});
	diagnostics.erase(
		std::unique(
			diagnostics.begin(), diagnostics.end(),
			[](const auto& a, const auto& b) {
				return a.position == b.position &&
					a.length == b.length &&
					a.message == b.message;
			}
		),
		diagnostics.end()
	);
	return diagnostics;
}

[[nodiscard]] float RichTextSourceRangeWidth(
	std::string_view source, std::size_t begin, std::size_t end
) {
	begin = std::min(begin, source.size());
	end = std::clamp(end, begin, source.size());
	return ImGui::CalcTextSize(source.data() + begin, source.data() + end, false).x;
}

[[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>>
BuildRichTextProtectedSpans(
	std::string_view source, std::size_t paragraph_begin, std::size_t paragraph_end,
	float wrap_width
) {
	struct OpenTag {
		std::string name{};
		std::size_t begin{ 0 };
	};

	std::vector<OpenTag> stack;
	std::vector<std::pair<std::size_t, std::size_t>> candidates;

	for (std::size_t i{ paragraph_begin }; i < paragraph_end;) {
		const auto token{ ParseRichTextSourceTagToken(source, i, paragraph_end) };
		if (!token.has_value()) {
			++i;
			continue;
		}

		if (!token->closing) {
			if (!token->name.empty()) {
				stack.push_back(OpenTag{ .name = token->name, .begin = token->begin });
			}
			i = token->end;
			continue;
		}

		auto match{ stack.end() };
		for (auto it{ stack.end() }; it != stack.begin();) {
			--it;
			if (RichTextTagNameMatches(it->name, token->name)) {
				match = it;
				break;
			}
		}

		if (match != stack.end()) {
			const std::size_t begin{ match->begin };
			stack.erase(match, stack.end());
			if (RichTextSourceRangeWidth(source, begin, token->end) <= wrap_width) {
				candidates.emplace_back(begin, token->end);
			}
		}
		i = token->end;
	}

	// For a given opening position prefer the widest balanced span that still fits.
	std::ranges::sort(candidates, [](const auto& a, const auto& b) {
		if (a.first != b.first) {
			return a.first < b.first;
		}
		return a.second > b.second;
	});

	std::vector<std::pair<std::size_t, std::size_t>> result;
	std::size_t covered_until{ paragraph_begin };
	for (const auto& span : candidates) {
		if (span.first < covered_until) {
			continue;
		}
		result.push_back(span);
		covered_until = span.second;
	}
	return result;
}

[[nodiscard]] std::size_t RichTextSourceCharacterWrapEnd(
	std::string_view source, std::size_t begin, std::size_t end, float wrap_width
) {
	std::size_t best{ begin };
	std::size_t cursor{ begin };
	while (cursor < end) {
		const std::size_t next{ std::min(
			end, NextRichTextGraphemeBoundary(source, cursor)
		) };
		cursor = std::max(next, std::min(end, cursor + 1));

		if (RichTextSourceRangeWidth(source, begin, cursor) > wrap_width && best > begin) {
			break;
		}
		best = cursor;
		if (RichTextSourceRangeWidth(source, begin, cursor) > wrap_width) {
			break;
		}
	}
	return std::max(best, std::min(NextRichTextGraphemeBoundary(source, begin), end));
}

[[nodiscard]] std::vector<RichTextSourceVisualLine> BuildRichTextSourceVisualLines(
	std::string_view source, bool word_wrap, float wrap_width
) {
	std::vector<RichTextSourceVisualLine> lines;
	wrap_width = std::max(1.0f, wrap_width);

	for (std::size_t paragraph_begin{ 0 }; paragraph_begin <= source.size();) {
		const std::size_t newline{ source.find('\n', paragraph_begin) };
		const std::size_t paragraph_end{
			newline == std::string_view::npos ? source.size() : newline
		};

		if (!word_wrap || paragraph_begin == paragraph_end) {
			lines.push_back({ paragraph_begin, paragraph_end });
		} else {
			const auto protected_spans{ BuildRichTextProtectedSpans(
				source, paragraph_begin, paragraph_end, wrap_width
			) };
			std::size_t protected_index{ 0 };
			std::size_t line_begin{ paragraph_begin };
			std::size_t position{ paragraph_begin };
			float line_width{ 0.0f };

			auto skip_line_leading_blanks = [&]() {
				while (position < paragraph_end &&
					(source[position] == ' ' || source[position] == '\t')) {
					++position;
				}
				line_begin = position;
			};

			auto emit_line_before = [&](std::size_t end) {
				lines.push_back({ line_begin, std::max(line_begin, end) });
				position = end;
				skip_line_leading_blanks();
				line_width = 0.0f;
			};

			while (position < paragraph_end) {
				while (protected_index < protected_spans.size() &&
					protected_spans[protected_index].second <= position) {
					++protected_index;
				}

				std::size_t unit_end{ position + 1 };

				// Highest-priority wrap unit: a complete balanced tagged span which can fit on
				// one row by itself, e.g. <b>Hello</b> or <b><i>Hello</i></b>.
				if (protected_index < protected_spans.size() &&
					protected_spans[protected_index].first == position) {
					unit_end = protected_spans[protected_index].second;
				} else if (const auto token{
					ParseRichTextSourceTagToken(source, position, paragraph_end)
				}) {
					// If a whole balanced span cannot fit, keep consecutive opening tags or
					// consecutive closing tags together while that group still fits. If the group
					// itself is too wide we fall back to one complete tag, then characters.
					unit_end = token->end;
					std::size_t group_end{ token->end };
					for (std::size_t next{ token->end }; next < paragraph_end;) {
						const auto next_token{
							ParseRichTextSourceTagToken(source, next, paragraph_end)
						};
						if (!next_token.has_value() || next_token->closing != token->closing) {
							break;
						}
						if (RichTextSourceRangeWidth(source, position, next_token->end) > wrap_width) {
							break;
						}
						group_end = next_token->end;
						next = next_token->end;
					}
					unit_end = group_end;
				} else {
					// Ordinary source text wraps by words. Keep the blanks immediately before a
					// word with that word so they are discarded naturally when the word moves to
					// the next visual row.
					std::size_t cursor{ position };
					while (cursor < paragraph_end &&
						(source[cursor] == ' ' || source[cursor] == '\t')) {
						++cursor;
					}
					if (cursor > position) {
						while (cursor < paragraph_end && source[cursor] != ' ' &&
							source[cursor] != '\t' && source[cursor] != '<') {
							++cursor;
						}
						unit_end = cursor;
					} else {
						while (cursor < paragraph_end && source[cursor] != ' ' &&
							source[cursor] != '\t' && source[cursor] != '<') {
							++cursor;
						}
						unit_end = std::max(cursor, position + 1);
					}
				}

				const float unit_width{ RichTextSourceRangeWidth(source, position, unit_end) };
				if (line_width > 0.0f && line_width + unit_width > wrap_width) {
					emit_line_before(position);
					continue;
				}

				if (unit_width <= wrap_width) {
					line_width += unit_width;
					position = unit_end;
					continue;
				}

				// Final fallback: a single word/tag still cannot fit, so split by UTF-8
				// character exactly as a conventional text box would.
				if (line_width > 0.0f) {
					emit_line_before(position);
					continue;
				}

				const std::size_t chunk_end{ RichTextSourceCharacterWrapEnd(
					source, position, unit_end, wrap_width
				) };
				if (chunk_end < unit_end) {
					lines.push_back({ line_begin, chunk_end });
					position = chunk_end;
					line_begin = position;
					line_width = 0.0f;
					continue;
				}

				line_width = unit_width;
				position = unit_end;
			}

			lines.push_back({ line_begin, paragraph_end });
		}

		if (newline == std::string_view::npos) {
			break;
		}
		paragraph_begin = newline + 1;
		if (paragraph_begin == source.size()) {
			lines.push_back({ source.size(), source.size() });
			break;
		}
	}

	if (lines.empty()) {
		lines.push_back({ 0, 0 });
	}
	return lines;
}

void DrawRichTextSourceSelectionHighlight(
	std::string_view source, const RichTextSelectionState& selection,
	std::span<const RichTextSourceVisualLine> lines, ImVec2 text_origin,
	ImVec2 item_min, ImVec2 item_max
) {
	std::size_t begin{ std::min(selection.selection_start, selection.selection_end) };
	std::size_t end{ std::max(selection.selection_start, selection.selection_end) };
	begin = std::min(begin, source.size());
	end = std::min(end, source.size());
	if (begin >= end) {
		return;
	}

	ImDrawList* draw_list{ ImGui::GetWindowDrawList() };
	const float line_height{ ImGui::GetTextLineHeight() };
	const ImU32 selection_color{ ImGui::GetColorU32(ImGuiCol_TextSelectedBg) };

	draw_list->PushClipRect(item_min, item_max, true);
	for (std::size_t line_index{ 0 }; line_index < lines.size(); ++line_index) {
		const auto& line{ lines[line_index] };
		const std::size_t selected_begin{ std::max(begin, line.begin) };
		const std::size_t selected_end{ std::min(end, line.end) };
		if (selected_begin >= selected_end) {
			continue;
		}

		const float x0{
			text_origin.x + ImGui::CalcTextSize(
				source.data() + line.begin, source.data() + selected_begin, false
			).x
		};
		const float x1{
			text_origin.x + ImGui::CalcTextSize(
				source.data() + line.begin, source.data() + selected_end, false
			).x
		};
		const float y{ text_origin.y + static_cast<float>(line_index) * line_height };

		draw_list->AddRectFilled(
			ImVec2{ x0, y }, ImVec2{ std::max(x1, x0 + 1.0f), y + line_height },
			selection_color
		);
	}
	draw_list->PopClipRect();
}

void DrawRichTextSourceSyntax(
	std::string_view source, const TextRunDefaults& defaults,
	std::span<const RichTextSourceVisualLine> lines, ImVec2 text_origin,
	ImVec2 item_min, ImVec2 item_max
) {
	if (source.empty()) {
		return;
	}

	const auto colors{ BuildRichTextSourceColors(source, defaults) };
	ImDrawList* draw_list{ ImGui::GetWindowDrawList() };
	const float line_height{ ImGui::GetTextLineHeight() };

	draw_list->PushClipRect(item_min, item_max, true);
	for (std::size_t line_index{ 0 }; line_index < lines.size(); ++line_index) {
		const auto& line{ lines[line_index] };
		if (line.begin >= line.end) {
			continue;
		}

		const float y{ text_origin.y + static_cast<float>(line_index) * line_height };
		if (y + line_height < item_min.y || y > item_max.y) {
			continue;
		}

		std::size_t segment_begin{ line.begin };
		while (segment_begin < line.end) {
			const ImU32 segment_color{ colors[segment_begin] };
			std::size_t segment_end{ segment_begin + 1 };
			while (segment_end < line.end && colors[segment_end] == segment_color) {
				++segment_end;
			}

			const float x{
				text_origin.x + ImGui::CalcTextSize(
					source.data() + line.begin, source.data() + segment_begin, false
				).x
			};
			draw_list->AddText(
				ImVec2{ x, y }, segment_color,
				source.data() + segment_begin, source.data() + segment_end
			);
			segment_begin = segment_end;
		}
	}
	draw_list->PopClipRect();
}

void DrawRichTextSourceDiagnostics(
	std::string_view source,
	std::span<const RichTextEditorDiagnostic> diagnostics,
	std::span<const RichTextSourceVisualLine> lines,
	ImVec2 text_origin,
	ImVec2 item_min,
	ImVec2 item_max
) {
	if (diagnostics.empty() || lines.empty()) {
		return;
	}

	ImDrawList* draw_list{ ImGui::GetWindowDrawList() };
	const float line_height{ ImGui::GetTextLineHeight() };
	const ImU32 color{ ImGui::GetColorU32(ImVec4{ 1.0f, 0.35f, 0.2f, 1.0f }) };
	const ImVec2 mouse{ ImGui::GetIO().MousePos };
	std::optional<std::string_view> hovered_message;

	draw_list->PushClipRect(item_min, item_max, true);
	for (const auto& diagnostic : diagnostics) {
		const std::size_t begin{ std::min(diagnostic.position, source.size()) };
		const std::size_t end{ std::min(
			source.size(), begin + std::max<std::size_t>(diagnostic.length, 1)
		) };

		for (std::size_t line_index{ 0 }; line_index < lines.size(); ++line_index) {
			const auto& line{ lines[line_index] };
			if (end <= line.begin || begin >= line.end) {
				continue;
			}

			const std::size_t range_begin{ std::clamp(begin, line.begin, line.end) };
			const std::size_t range_end{ std::clamp(end, range_begin, line.end) };
			const float x0{
				text_origin.x + RichTextSourceRangeWidth(source, line.begin, range_begin)
			};
			float x1{
				text_origin.x + RichTextSourceRangeWidth(source, line.begin, range_end)
			};
			x1 = std::max(x1, x0 + 6.0f);

			const float y0{
				text_origin.y + static_cast<float>(line_index) * line_height
			};
			const float y{ y0 + line_height - 1.5f };

			// A small zig-zag reads like an editor diagnostic without obscuring the source.
			constexpr float step{ 4.0f };
			for (float x{ x0 }; x < x1;) {
				const float mid{ std::min(x + step * 0.5f, x1) };
				const float next{ std::min(x + step, x1) };
				draw_list->AddLine(ImVec2{ x, y }, ImVec2{ mid, y - 1.5f }, color, 1.0f);
				draw_list->AddLine(ImVec2{ mid, y - 1.5f }, ImVec2{ next, y }, color, 1.0f);
				x = next;
			}

			if (mouse.x >= x0 && mouse.x <= x1 &&
				mouse.y >= y0 && mouse.y <= y0 + line_height) {
				hovered_message = diagnostic.message;
			}
		}
	}
	draw_list->PopClipRect();

	if (hovered_message.has_value()) {
		ImGui::SetTooltip("%.*s",
			static_cast<int>(hovered_message->size()), hovered_message->data());
	}
}

void DrawRichTextSourceCaret(
	std::string_view source, const RichTextSelectionState& selection,
	std::span<const RichTextSourceVisualLine> lines, ImVec2 text_origin,
	ImVec2 item_min, ImVec2 item_max
) {
	if (lines.empty() || selection.selection_start != selection.selection_end) {
		return;
	}

	// Keep one editor-owned caret because the tag-aware visual rows can intentionally differ from
	// Dear ImGui's generic word-wrap rows. The native caret is hidden while this input is drawn.
	// A fixed line-height caret avoids the old double-cursor/variable-height blink artifact.
	if (std::fmod(ImGui::GetTime(), 1.2) >= 0.8) {
		return;
	}

	const std::size_t cursor{ std::min(selection.cursor, source.size()) };
	std::size_t row{ lines.size() - 1 };
	for (std::size_t i{ 0 }; i < lines.size(); ++i) {
		const auto& line{ lines[i] };
		if (cursor < line.end) {
			row = i;
			break;
		}
		if (cursor == line.end) {
			if (i + 1 < lines.size() && lines[i + 1].begin == cursor) {
				continue;
			}
			row = i;
			break;
		}
	}

	const auto& line{ lines[row] };
	const std::size_t clamped_cursor{ std::clamp(cursor, line.begin, line.end) };
	const float x{
		text_origin.x + RichTextSourceRangeWidth(source, line.begin, clamped_cursor)
	};
	const float line_height{ ImGui::GetTextLineHeight() };
	const float y{ text_origin.y + static_cast<float>(row) * line_height };
	const ImU32 color{
#if IMGUI_VERSION_NUM >= 19230
		ImGui::GetColorU32(ImGuiCol_InputTextCursor)
#else
		ImGui::GetColorU32(ImGuiCol_Text)
#endif
	};

	ImDrawList* draw_list{ ImGui::GetWindowDrawList() };
	draw_list->PushClipRect(item_min, item_max, true);
	draw_list->AddLine(ImVec2{ x, y }, ImVec2{ x, y + line_height }, color, 1.0f);
	draw_list->PopClipRect();
}

[[nodiscard]] float RichTextSourceWidth(std::string_view source) {
	float width{ 0.0f };
	while (true) {
		const auto newline{ source.find('\n') };
		const std::string_view line{
			newline == std::string_view::npos ? source : source.substr(0, newline)
		};
		width = std::max(
			width,
			ImGui::CalcTextSize(line.data(), line.data() + line.size(), false).x
		);

		if (newline == std::string_view::npos) {
			break;
		}
		source.remove_prefix(newline + 1);
	}

	return width + ImGui::GetStyle().FramePadding.x * 2.0f + 16.0f;
}

[[nodiscard]] std::size_t RichTextSourcePositionFromMouse(
	std::string_view source,
	std::span<const RichTextSourceVisualLine> lines,
	ImVec2 text_origin,
	ImVec2 mouse_position
) {
	if (lines.empty()) {
		return 0;
	}

	const float line_height{ std::max(ImGui::GetTextLineHeight(), 1.0f) };
	const float row_float{ (mouse_position.y - text_origin.y) / line_height };
	const std::size_t row{ static_cast<std::size_t>(std::clamp(
		static_cast<int>(std::floor(row_float)), 0, static_cast<int>(lines.size() - 1)
	)) };
	const auto& line{ lines[row] };

	if (mouse_position.x <= text_origin.x || line.begin >= line.end) {
		return line.begin;
	}

	const float target_x{ mouse_position.x - text_origin.x };
	std::size_t cursor{ line.begin };
	float previous_width{ 0.0f };
	while (cursor < line.end) {
		const std::size_t next{ std::min(
			line.end, NextRichTextGraphemeBoundary(source, cursor)
		) };
		const float next_width{ RichTextSourceRangeWidth(source, line.begin, next) };
		if (target_x < (previous_width + next_width) * 0.5f) {
			return cursor;
		}
		previous_width = next_width;
		cursor = std::max(next, cursor + 1);
	}
	return line.end;
}

void SelectRichTextSourceWordAt(
	std::string_view source, std::size_t position, RichTextSelectionState& selection
) {
	position = std::min(position, source.size());
	if (source.empty()) {
		RequestRichTextSelection(selection, 0, 0, 0);
		return;
	}

	std::size_t anchor{ position };
	if (anchor == source.size()) {
		anchor = PreviousRichTextGraphemeBoundary(source, anchor);
	} else if (PreviousRichTextGraphemeBoundary(
			source, NextRichTextGraphemeBoundary(source, anchor)
		) != anchor) {
		anchor = PreviousRichTextGraphemeBoundary(source, anchor);
	}

	if (!IsRichTextSourceWordGrapheme(source, anchor)) {
		if (anchor > 0) {
			const std::size_t previous{ PreviousRichTextGraphemeBoundary(source, anchor) };
			if (IsRichTextSourceWordGrapheme(source, previous)) {
				anchor = previous;
			} else {
				const std::size_t end{ NextRichTextGraphemeBoundary(source, anchor) };
				RequestRichTextSelection(selection, end, anchor, end);
				return;
			}
		} else {
			const std::size_t end{ NextRichTextGraphemeBoundary(source, anchor) };
			RequestRichTextSelection(selection, end, anchor, end);
			return;
		}
	}

	std::size_t begin{ anchor };
	while (begin > 0) {
		const std::size_t previous{ PreviousRichTextGraphemeBoundary(source, begin) };
		if (!IsRichTextSourceWordGrapheme(source, previous)) {
			break;
		}
		begin = previous;
	}

	std::size_t end{ NextRichTextGraphemeBoundary(source, anchor) };
	while (end < source.size() && IsRichTextSourceWordGrapheme(source, end)) {
		end = NextRichTextGraphemeBoundary(source, end);
	}

	RequestRichTextSelection(selection, end, begin, end);
}

bool DrawRichTextSourceInput(
	EditorContext& ctx,
	std::string& source,
	RichTextEditorState& editor_state,
	RichTextSelectionState& selection,
	const TextRunDefaults& defaults,
	const RichTextEditorOptions& options,
	float& editor_height,
	float default_editor_height,
	float maximum_editor_height
) {
	const bool word_wrap{ editor_state.word_wrap };
	const bool page_preview_mode{
		options.show_page_numbers_button && editor_state.show_page_numbers
	};
	std::string page_preview_source;
	if (page_preview_mode) {
		page_preview_source = options.page_number_preview_source.empty()
			? source
			: std::string{ options.page_number_preview_source };
	}
	std::string& displayed_source{
		page_preview_mode ? page_preview_source : source
	};
	const float source_width{ RichTextSourceWidth(displayed_source) };
	const float minimum_editor_height{
		ImGui::GetTextLineHeightWithSpacing() * 3.0f +
		ImGui::GetStyle().FramePadding.y * 2.0f
	};
	if (editor_height <= 0.0f) {
		editor_height = default_editor_height;
	}
	editor_height = std::clamp(
		editor_height,
		minimum_editor_height,
		std::max(minimum_editor_height, maximum_editor_height)
	);

	// Formatting and local history are applied from inside ImGui's input callback so the
	// active edit buffer and the std::string can never diverge. Reserve enough room for
	// both tag insertion and restoring the largest currently reachable undo state.
	if (!page_preview_mode) {
		std::size_t history_capacity{ source.size() };
		for (const auto& entry : editor_state.source_history) {
			history_capacity = std::max(history_capacity, entry.source.size());
		}
		source.reserve(history_capacity + 4096);
	}

	ImGuiWindowFlags child_window_flags{ ImGuiWindowFlags_None };
	if (!word_wrap) {
		child_window_flags |= ImGuiWindowFlags_HorizontalScrollbar;
	}

	// Keep the actual source box opaque. InputTextMultiline's own frame stays transparent below
	// so the syntax layer drawn into this outer child remains visible.
	ImVec4 opaque_frame_color{ ImGui::GetStyleColorVec4(ImGuiCol_FrameBg) };
	opaque_frame_color.w = 1.0f;
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_ChildBg, opaque_frame_color);
	ImGui::SetNextWindowSizeConstraints(
		ImVec2{ 0.0f, minimum_editor_height },
		ImVec2{ FLT_MAX, maximum_editor_height }
	);
	ImGui::BeginChild(
		"##RichTextSourceScroll",
		ImVec2{ -FLT_MIN, editor_height },
		ImGuiChildFlags_ResizeY,
		child_window_flags
	);
	ImGui::PopStyleColor();

	const float inner_width{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
	const bool show_page_numbers{
		page_preview_mode && !options.line_page_numbers.empty()
	};
	std::size_t maximum_page_number{ 0 };
	for (const std::size_t page : options.line_page_numbers) {
		maximum_page_number = std::max(maximum_page_number, page);
	}
	const std::string widest_page_label{
		"[" + std::to_string(std::max<std::size_t>(maximum_page_number, 1)) + "]"
	};
	const float page_number_gutter{
		show_page_numbers
			? ImGui::CalcTextSize(widest_page_label.c_str()).x +
				ImGui::GetStyle().ItemInnerSpacing.x * 2.0f
			: 0.0f
	};
	const float source_view_width{
		std::max(1.0f, inner_width - page_number_gutter)
	};
	const bool horizontal_overflow{
		!word_wrap && source_width > source_view_width
	};
	const float input_width{
		word_wrap
			? source_view_width
			: std::max(source_view_width, source_width)
	};
	const float text_width{
		std::max(1.0f, input_width - ImGui::GetStyle().FramePadding.x * 2.0f)
	};
	const auto visual_lines{ BuildRichTextSourceVisualLines(displayed_source, word_wrap, text_width) };
	const float content_height{
		static_cast<float>(visual_lines.size()) * ImGui::GetTextLineHeight() +
		ImGui::GetStyle().FramePadding.y * 2.0f + 2.0f
	};

	// Fill the visible source box when content is short, but do not make the child content a few
	// pixels taller than its viewport. Reserving the horizontal scrollbar height explicitly avoids
	// the phantom vertical scrollbar that appeared whenever wrapping was disabled.
	float visible_content_height{ std::max(1.0f, ImGui::GetContentRegionAvail().y) };
	if (horizontal_overflow) {
		visible_content_height = std::max(
			1.0f,
			visible_content_height - ImGui::GetStyle().ScrollbarSize
		);
	}
	// The outer resizable child is the sole scroll owner. When content overflows vertically, give
	// InputTextMultiline a small amount of extra height so its private multiline child never needs
	// to establish a second vertical scroll range because of padding/rounding differences.
	const bool vertical_overflow{ content_height > visible_content_height };
	const float input_height{
		vertical_overflow
			? content_height + ImGui::GetStyle().FramePadding.y + 2.0f
			: visible_content_height
	};

	if (selection.focus_source && !page_preview_mode) {
		ImGui::SetKeyboardFocusHere();
		selection.focus_source = false;
	} else if (page_preview_mode) {
		selection.focus_source = false;
	}

	RichTextInputCallbackContext callback_context{
		.selection = &selection,
		.editor_state = &editor_state,
		.defaults = &defaults,
	};

	editor_state.source_history_applied = false;

	constexpr ImGuiInputTextFlags kHistoryFlags{ ImGuiInputTextFlags_NoUndoRedo };
	ImGuiInputTextFlags input_flags{
		ImGuiInputTextFlags_AllowTabInput |
		kHistoryFlags | ImGuiInputTextFlags_NoHorizontalScroll
	};
	if (page_preview_mode) {
		input_flags |= ImGuiInputTextFlags_ReadOnly;
	} else {
		input_flags |= ImGuiInputTextFlags_CallbackAlways;
	}
#if IMGUI_VERSION_NUM >= 19230
	if (word_wrap) {
		input_flags |= ImGuiInputTextFlags_WordWrap;
	}
#endif

	constexpr ImVec4 transparent_input_frame{ 0.0f, 0.0f, 0.0f, 0.0f };
	ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent_input_frame);
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, transparent_input_frame);
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, transparent_input_frame);
#if IMGUI_VERSION_NUM >= 19104
	ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
#else
	ImGui::PushStyleColor(ImGuiCol_NavHighlight, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
#endif
	const ImVec4 original_text_color{ ImGui::GetStyleColorVec4(ImGuiCol_Text) };
	ImGui::PushStyleColor(
		ImGuiCol_Text,
		ImVec4{ original_text_color.x, original_text_color.y, original_text_color.z, 0.0f }
	);
	ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });

	int pushed_source_colors{ 6 };
#if IMGUI_VERSION_NUM >= 19230
	ImGui::PushStyleColor(
		ImGuiCol_InputTextCursor, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f }
	);
	++pushed_source_colors;
#endif

	// InputTextMultiline internally owns a child window with its own vertical scrollbar. The outer
	// RichTextSourceScroll child already owns scrolling for the syntax overlay, selection and caret,
	// so suppress the inner scrollbar visually. input_height above is sized to the full text content,
	// which keeps that private child from needing to scroll during normal editing.
	if (show_page_numbers) {
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + page_number_gutter);
	}

	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
	const char* source_input_id{
		page_preview_mode
			? "##RichTextPagePreview"
			: "##RichTextSource"
	};
	const bool input_changed{ ImGui::InputTextMultiline(
		source_input_id,
		&displayed_source,
		ImVec2{ input_width, input_height },
		input_flags,
		page_preview_mode ? nullptr : &RichTextInputCallback,
		page_preview_mode ? nullptr : &callback_context
	) };
	ImGui::PopStyleVar();
	const bool source_clicked{ ImGui::IsItemClicked(ImGuiMouseButton_Left) };
	const bool source_double_clicked{
		ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
	};
	const bool source_active{ ImGui::IsItemActive() };
	const bool editable_source_active{ source_active && !page_preview_mode };
	const ImVec2 source_min{ ImGui::GetItemRectMin() };
	const ImVec2 source_max{ ImGui::GetItemRectMax() };
	ImGui::PopStyleColor(pushed_source_colors);

	if (source_clicked && !page_preview_mode) {
		selection.keep_selection_highlight = false;
	}

	if (editable_source_active && !selection.pending_action.has_value()) {
		const auto& io{ ImGui::GetIO() };
		const bool command_modifier{ io.KeyCtrl || io.KeySuper };
		std::optional<std::tuple<std::string_view, std::string_view, std::string_view>> shortcut;
		if (command_modifier && ImGui::IsKeyPressed(ImGuiKey_B, false)) {
			shortcut = std::tuple{ std::string_view{ "b" }, std::string_view{ "<b>" }, std::string_view{ "</b>" } };
		} else if (command_modifier && ImGui::IsKeyPressed(ImGuiKey_I, false)) {
			shortcut = std::tuple{ std::string_view{ "i" }, std::string_view{ "<i>" }, std::string_view{ "</i>" } };
		} else if (command_modifier && ImGui::IsKeyPressed(ImGuiKey_U, false)) {
			shortcut = std::tuple{ std::string_view{ "u" }, std::string_view{ "<u>" }, std::string_view{ "</u>" } };
		}

		if (shortcut.has_value()) {
			if (const auto error{ RichTextFormattingTargetError(source, selection) }) {
				editor_state.selection_warning = *error;
				editor_state.selection_warning_until = ImGui::GetTime() + 4.0;
			} else {
				const auto [tag, open, close]{ *shortcut };
				const bool has_range{
					RichTextSelectionHasRange(source, selection)
				};
				const std::size_t cursor{
					std::min(selection.cursor, source.size())
				};
				const bool inside_existing_tag{
					FindRichTextSurroundingTag(
						source, cursor, cursor, tag
					).has_value()
				};

				if (!has_range && !inside_existing_tag) {
					editor_state.selection_warning =
						"Select text before applying this style.";
					editor_state.selection_warning_until =
						ImGui::GetTime() + 4.0;
				} else {
					ctx.undo.CommitActiveEdit();
					selection.pending_action = RichTextPendingAction{
						.type = RichTextPendingActionType::ToggleTag,
						.cursor = selection.cursor,
						.selection_start = selection.selection_start,
						.selection_end = selection.selection_end,
						.tag = std::string{ tag },
						.open = std::string{ open },
						.close = std::string{ close },
					};
					selection.apply_selection = true;
					selection.keep_selection_highlight = true;
					editor_state.selection_warning.clear();
					editor_state.selection_warning_until = 0.0;
				}
			}
		}
	}

	bool changed{
		!page_preview_mode &&
		(input_changed || callback_context.action_applied)
	};
	if (!page_preview_mode &&
		selection.pending_action.has_value() &&
		!editable_source_active) {
		changed |= ApplyPendingRichTextActionToInactiveSource(
			source, selection, editor_state, defaults
		);
	}

	if (!page_preview_mode && !editor_state.source_history_applied) {
		if (changed) {
			RecordRichTextSourceHistory(editor_state, source, selection);
		} else if (!editor_state.source_history.empty()) {
			auto& current{ editor_state.source_history[editor_state.source_history_index] };
			if (current.source == source) {
				current.cursor = selection.cursor;
				current.selection_start = selection.selection_start;
				current.selection_end = selection.selection_end;
			}
		}
	}
	editor_state.source_history_applied = false;

	const auto editor_diagnostics{
		BuildRichTextEditorDiagnostics(ctx, displayed_source, defaults, options)
	};
	const auto display_lines{ BuildRichTextSourceVisualLines(displayed_source, word_wrap, text_width) };
	const ImVec2 text_origin{
		source_min.x + ImGui::GetStyle().FramePadding.x,
		source_min.y + ImGui::GetStyle().FramePadding.y,
	};

	const auto& io{ ImGui::GetIO() };
	const std::size_t mouse_source_position{ RichTextSourcePositionFromMouse(
		displayed_source, display_lines, text_origin, io.MousePos
	) };

	if (!page_preview_mode && source_double_clicked) {
		selection.drag_selection_anchor.reset();
		SelectRichTextSourceWordAt(source, mouse_source_position, selection);
		selection.drag_word_selection = std::pair{
			std::min(selection.selection_start, selection.selection_end),
			std::max(selection.selection_start, selection.selection_end),
		};
	} else {
		if (!page_preview_mode && source_clicked && !io.KeyShift) {
			selection.drag_word_selection.reset();
			selection.drag_selection_anchor = mouse_source_position;
			RequestRichTextSelection(
				selection, mouse_source_position, mouse_source_position, mouse_source_position
			);
		}

		// Dear ImGui's native drag selection uses its own generic wrapping rows. Those rows can
		// differ from our tag-aware layout, so always derive drag endpoints from the same
		// visual map used by our highlight. A double-click drag is word-anchored: dragging
		// left keeps the word's right edge, while dragging right keeps its left edge.
		if (editable_source_active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			if (selection.drag_word_selection.has_value()) {
				const auto [word_begin, word_end]{ *selection.drag_word_selection };
				if (mouse_source_position < word_begin) {
					RequestRichTextSelection(
						selection, mouse_source_position, word_end, mouse_source_position
					);
				} else if (mouse_source_position > word_end) {
					RequestRichTextSelection(
						selection, mouse_source_position, word_begin, mouse_source_position
					);
				} else {
					RequestRichTextSelection(selection, word_end, word_begin, word_end);
				}
			} else if (selection.drag_selection_anchor.has_value()) {
				const std::size_t anchor{ selection.drag_selection_anchor.value() };
				RequestRichTextSelection(
					selection, mouse_source_position, anchor, mouse_source_position
				);
			}
		}
	}

	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		selection.drag_selection_anchor.reset();
		selection.drag_word_selection.reset();
	}

	if (!page_preview_mode &&
		(editable_source_active || selection.keep_selection_highlight) &&
		selection.selection_start != selection.selection_end) {
		DrawRichTextSourceSelectionHighlight(
			source, selection, display_lines, text_origin, source_min, source_max
		);
	}
	DrawRichTextSourceSyntax(
		displayed_source, defaults, display_lines, text_origin, source_min, source_max
	);
	DrawRichTextSourceDiagnostics(
		displayed_source, editor_diagnostics, display_lines, text_origin, source_min, source_max
	);
	if (editable_source_active) {
		DrawRichTextSourceCaret(
			source, selection, display_lines, text_origin, source_min, source_max
		);
	}

	if (show_page_numbers) {
		ImDrawList* draw_list{ ImGui::GetWindowDrawList() };
		draw_list->PushClipRect(
			ImVec2{ source_min.x - page_number_gutter, source_min.y },
			source_max,
			true
		);

		std::size_t logical_line{ 0 };
		std::size_t scan_position{ 0 };
		std::optional<std::size_t> previous_drawn_line;
		for (std::size_t visual_index{ 0 }; visual_index < display_lines.size(); ++visual_index) {
			const auto& line{ display_lines[visual_index] };
			while (scan_position < line.begin && scan_position < displayed_source.size()) {
				if (displayed_source[scan_position] == '\n') {
					++logical_line;
				}
				++scan_position;
			}

			if (previous_drawn_line == logical_line) {
				continue;
			}
			previous_drawn_line = logical_line;

			if (logical_line >= options.line_page_numbers.size()) {
				continue;
			}
			const std::size_t page_number{
				options.line_page_numbers[logical_line]
			};
			if (page_number == 0) {
				continue;
			}

			const std::string label{
				"[" + std::to_string(page_number) + "]"
			};
			const float label_width{ ImGui::CalcTextSize(label.c_str()).x };
			const ImVec2 position{
				text_origin.x - ImGui::GetStyle().ItemInnerSpacing.x - label_width,
				text_origin.y +
					static_cast<float>(visual_index) * ImGui::GetTextLineHeight(),
			};
			draw_list->AddText(
				position,
				ImGui::GetColorU32(ImGuiCol_TextDisabled),
				label.c_str()
			);
		}
		draw_list->PopClipRect();
	}

	ImGui::EndChild();
	const ImVec2 child_min{ ImGui::GetItemRectMin() };
	const ImVec2 child_max{ ImGui::GetItemRectMax() };
	editor_height = std::clamp(
		child_max.y - child_min.y,
		minimum_editor_height,
		maximum_editor_height
	);
	ImGui::PopStyleVar();

	return changed;
}

[[nodiscard]] std::size_t RichTextCharacterPosition(
	std::string_view source, std::size_t byte_position
) {
	byte_position = std::min(byte_position, source.size());
	std::size_t character{ 1 };
	for (std::size_t i{ 0 }; i < byte_position;) {
		const std::size_t next{ std::min(
			byte_position, NextRichTextGraphemeBoundary(source, i)
		) };
		i = std::max(next, i + 1);
		++character;
	}
	return character;
}

[[nodiscard]] Color RichTextPreviewClearColor() {
	// Keep the preview target opaque so the text/effect alpha is composited exactly once by the
	// Protegon renderer. A transparent target would be alpha-blended again when ImGui samples the
	// preview texture, making partially transparent glyphs/effects appear too faint.
	return color::Black;
}

[[nodiscard]] std::optional<std::string> RichTextPreviewMissingFont(
	EditorContext& ctx,
	const StyledText& styled_text
) {
	::ptgn::impl::AssetAccessor assets{ ctx.editor.GetAssetManager() };

	for (const auto& run : styled_text.runs) {
		if (assets.TryGet<Font>(run.font).has_value()) {
			continue;
		}

		return run.font.value.empty()
			? std::optional<std::string>{ "Default font is not loaded." }
			: std::optional<std::string>{
				  "Font '" + run.font.value + "' is not currently loaded."
			  };
	}

	return std::nullopt;
}

void DrawRichTextPreviewImpl(
	EditorContext& ctx,
	const StyledText& styled_text,
	const TextBox* preview_box
) {
	constexpr float kPreviewPadding{ 16.0f };
	constexpr int kMaximumPreviewDimension{ 4096 };

	if (const auto missing_font{ RichTextPreviewMissingFont(ctx, styled_text) }) {
		ImGui::TextDisabled("%s", missing_font->c_str());
		return;
	}

	TextBox box{ preview_box ? *preview_box : TextBox{} };
	// Runtime Text::Draw can derive missing alignment from the entity Origin. The
	// standalone preview has no entity transform, so resolve only missing axes to
	// the neutral top-left fallback while preserving every explicit TextBox setting.
	if (!box.style.alignment.horizontal.has_value()) {
		box.style.alignment.horizontal = HorizontalAlign::Left;
	}
	if (!box.style.alignment.vertical.has_value()) {
		box.style.alignment.vertical = VerticalAlign::Top;
	}

	const TextLayout layout{
		::ptgn::impl::BuildTextLayout(
			ctx.editor.GetAssetManager(),
			styled_text,
			box
		)
	};

	const V2_float box_size{ box.rect.GetSize() };

	// The logical layout may extend above/left of the TextBox when wrapping is disabled
	// (for example centered/right-aligned text that is wider than the box). Size and
	// position the preview from the union of the actual layout bounds and the TextBox,
	// rather than assuming the box top-left is the minimum visible coordinate.
	Rect preview_bounds{ layout.GetBounds() };
	bool has_preview_bounds{ !layout.lines.empty() };
	if (box.HasBox()) {
		if (has_preview_bounds) {
			preview_bounds.min = Min(preview_bounds.min, box.rect.min);
			preview_bounds.max = Max(preview_bounds.max, box.rect.max);
		} else {
			preview_bounds = box.rect;
			has_preview_bounds = true;
		}
	}
	if (!has_preview_bounds) {
		preview_bounds = {};
	}

	const Rect origin_rect{ box.HasBox() ? box.rect : layout.GetBounds() };
	const V2_float origin_point{ origin_rect.GetOriginPoint(Origin::TopLeft) };
	const V2_float relative_min{
		preview_bounds.min.x - origin_point.x,
		preview_bounds.min.y - origin_point.y,
	};
	const V2_float relative_max{
		preview_bounds.max.x - origin_point.x,
		preview_bounds.max.y - origin_point.y,
	};
	const V2_float preview_extent{
		std::max(0.0f, relative_max.x - relative_min.x),
		std::max(0.0f, relative_max.y - relative_min.y),
	};

	const ImVec2 available{ ImGui::GetContentRegionAvail() };
	const float natural_width{
		std::max(1.0f, preview_extent.x + kPreviewPadding * 2.0f)
	};
	const float natural_height{
		std::max(1.0f, preview_extent.y + kPreviewPadding * 2.0f)
	};

	const V2_int framebuffer_size{
		std::clamp(
			static_cast<int>(std::ceil(std::max(available.x, natural_width))),
			1,
			kMaximumPreviewDimension
		),
		std::clamp(
			static_cast<int>(std::ceil(std::max(available.y, natural_height))),
			1,
			kMaximumPreviewDimension
		),
	};

	const V2_float preview_origin{
		-static_cast<float>(framebuffer_size.x) * 0.5f + kPreviewPadding - relative_min.x,
		-static_cast<float>(framebuffer_size.y) * 0.5f + kPreviewPadding - relative_min.y,
	};

	Transform anchor{};
	anchor.position = preview_origin;

	const auto prepared{ ::ptgn::impl::PrepareTextDraw(
		anchor, layout, box, Origin::TopLeft, std::nullopt
	) };

	if (!prepared.drawable) {
		return;
	}

	const auto texture{ ctx.editor.RenderTextPreview(
		static_cast<std::uint64_t>(ImGui::GetFrameCount()),
		framebuffer_size,
		RichTextPreviewClearColor(),
		prepared.transform,
		DrawTextRequest{
			.layout = layout,
			.tint = color::White,
			.depth = {},
			.entity_id = ::ptgn::impl::kNoEntityId,
			.clips = prepared.GetClips(),
			.time = [&ctx]() {
				auto* scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };
				return scene ? scene->ctx().GameTime().count() : 0.0f;
			}(),
		}
	) };

	ImGui::Image(
		static_cast<ImTextureID>(texture),
		ImVec2{
			static_cast<float>(framebuffer_size.x),
			static_cast<float>(framebuffer_size.y),
		},
		ImVec2{ 0.0f, 1.0f },
		ImVec2{ 1.0f, 0.0f }
	);

	const auto& text_debug{ ctx.editor.GetDebugSystem().settings.text };
	if (preview_box && box.HasArea() && text_debug.draw_enabled) {
		const ImVec2 image_min{ ImGui::GetItemRectMin() };
		const V2_float box_relative_min{
			box.rect.min.x - origin_point.x,
			box.rect.min.y - origin_point.y,
		};
		const ImVec2 border_min{
			image_min.x + kPreviewPadding + box_relative_min.x - relative_min.x,
			image_min.y + kPreviewPadding + box_relative_min.y - relative_min.y,
		};
		const ImVec2 border_max{
			border_min.x + box_size.x,
			border_min.y + box_size.y,
		};
		const ImU32 debug_color{
			IM_COL32(
				text_debug.draw_color.r,
				text_debug.draw_color.g,
				text_debug.draw_color.b,
				text_debug.draw_color.a
			)
		};
		ImGui::GetWindowDrawList()->AddRect(
			border_min,
			border_max,
			debug_color,
			0.0f,
			0,
			text_debug.draw_line_width
		);
	}
}

void DrawRichTextTooltipAboveRect(
	std::string_view text, ImVec2 item_min, ImVec2 item_max
) {
	const ImVec2 anchor{
		(item_min.x + item_max.x) * 0.5f,
		item_min.y - ImGui::GetStyle().ItemSpacing.y,
	};
	ImGui::SetNextWindowPos(anchor, ImGuiCond_Always, ImVec2{ 0.5f, 1.0f });
	ImGui::BeginTooltip();
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
	ImGui::EndTooltip();
}

void DrawRichTextToolbarTooltip(std::string_view text) {
	if (!ImGui::IsItemHovered(
			ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_AllowWhenDisabled
		)) {
		return;
	}

	DrawRichTextTooltipAboveRect(
		text, ImGui::GetItemRectMin(), ImGui::GetItemRectMax()
	);
}

void SetNextRichTextPopupSizeConstraints() {
	const ImGuiViewport* viewport{ ImGui::GetMainViewport() };
	constexpr float margin{ 12.0f };
	const ImVec2 maximum_size{
		std::max(120.0f, viewport->WorkSize.x - margin * 2.0f),
		std::max(120.0f, viewport->WorkSize.y - margin * 2.0f),
	};
	ImGui::SetNextWindowSizeConstraints(
		ImVec2{ 0.0f, 0.0f },
		maximum_size
	);
}

void SetNextRichTextColorPickerPopupAbove(
	ImVec2 item_min,
	ImVec2 item_max,
	bool align_right = false
) {
	// Only cap the picker to the usable viewport itself. A lack of room directly
	// above the button should reposition the popup, not make it scrollable.
	const ImGuiViewport* viewport{ ImGui::GetWindowViewport() };
	constexpr float margin{ 12.0f };
	const ImVec2 maximum_size{
		std::max(120.0f, viewport->WorkSize.x - margin * 2.0f),
		std::max(120.0f, viewport->WorkSize.y - margin * 2.0f),
	};
	ImGui::SetNextWindowSizeConstraints(ImVec2{ 0.0f, 0.0f }, maximum_size);

	const float spacing{ ImGui::GetStyle().ItemSpacing.y };
	if (align_right) {
		ImGui::SetNextWindowPos(
			ImVec2{ item_max.x, item_min.y - spacing },
			ImGuiCond_Appearing,
			ImVec2{ 1.0f, 1.0f }
		);
	} else {
		ImGui::SetNextWindowPos(
			ImVec2{ item_min.x, item_min.y - spacing },
			ImGuiCond_Appearing,
			ImVec2{ 0.0f, 1.0f }
		);
	}
}

void FitCurrentRichTextColorPickerPopupToWorkArea(
	ImVec2 item_min,
	ImVec2 item_max,
	bool align_right = false
) {
	const ImGuiViewport* viewport{ ImGui::GetWindowViewport() };
	constexpr float margin{ 12.0f };
	const float spacing{ ImGui::GetStyle().ItemSpacing.y };

	const ImVec2 work_min{
		viewport->WorkPos.x + margin,
		viewport->WorkPos.y + margin,
	};
	const ImVec2 work_max{
		viewport->WorkPos.x + viewport->WorkSize.x - margin,
		viewport->WorkPos.y + viewport->WorkSize.y - margin,
	};
	const ImVec2 window_size{ ImGui::GetWindowSize() };

	const float minimum_x{ work_min.x };
	const float maximum_x{ std::max(work_min.x, work_max.x - window_size.x) };
	const float desired_x{
		align_right ? item_max.x - window_size.x : item_min.x
	};

	const float minimum_y{ work_min.y };
	const float maximum_y{ std::max(work_min.y, work_max.y - window_size.y) };
	const float above_y{ item_min.y - spacing - window_size.y };
	const float below_y{ item_max.y + spacing };

	float desired_y{ above_y };
	if (above_y < minimum_y && below_y + window_size.y <= work_max.y) {
		desired_y = below_y;
	}

	const ImVec2 fitted_pos{
		std::clamp(desired_x, minimum_x, maximum_x),
		std::clamp(desired_y, minimum_y, maximum_y),
	};
	ImGui::SetWindowPos(fitted_pos, ImGuiCond_Always);
}

void SetNextRichTextPopupAbove(
	ImVec2 item_min,
	ImVec2 item_max,
	bool align_right = false,
	ImGuiCond condition = ImGuiCond_Appearing
) {
	const ImGuiViewport* viewport{ ImGui::GetMainViewport() };
	const float spacing{ ImGui::GetStyle().ItemSpacing.y };
	constexpr float margin{ 12.0f };

	// Constrain the popup to the actual space above its control. When the contents
	// are taller/wider than that available work area, ImGui can expose scrollbars
	// instead of letting the popup disappear beyond a monitor edge.
	const float available_width{
		align_right
			? item_max.x - viewport->WorkPos.x - margin
			: viewport->WorkPos.x + viewport->WorkSize.x -
				item_min.x - margin
	};
	const float available_height{
		item_min.y - viewport->WorkPos.y - spacing - margin
	};
	const ImVec2 maximum_size{
		std::max(1.0f, available_width),
		std::max(1.0f, available_height),
	};
	ImGui::SetNextWindowSizeConstraints(
		ImVec2{ 0.0f, 0.0f },
		maximum_size
	);

	if (align_right) {
		ImGui::SetNextWindowPos(
			ImVec2{ item_max.x, item_min.y - spacing },
			condition,
			ImVec2{ 1.0f, 1.0f }
		);
	} else {
		ImGui::SetNextWindowPos(
			ImVec2{ item_min.x, item_min.y - spacing },
			condition,
			ImVec2{ 0.0f, 1.0f }
		);
	}
}

void DrawRichTextPreviewSeparator() {
	constexpr std::string_view label{ "Preview" };
	ImGui::SeparatorText(label.data());

	const ImVec2 item_min{ ImGui::GetItemRectMin() };
	const ImVec2 item_max{ ImGui::GetItemRectMax() };
	const ImVec2 label_size{ ImGui::CalcTextSize(label.data()) };
	const ImGuiStyle& style{ ImGui::GetStyle() };
	const float inner_width{ std::max(
		0.0f, item_max.x - item_min.x - style.SeparatorTextPadding.x * 2.0f - label_size.x
	) };
	const float label_x{
		item_min.x + style.SeparatorTextPadding.x + inner_width * style.SeparatorTextAlign.x
	};
	const float label_y{
		item_min.y + (item_max.y - item_min.y - label_size.y) * style.SeparatorTextAlign.y
	};
	const ImVec2 label_min{ label_x, label_y };
	const ImVec2 label_max{ label_x + label_size.x, label_y + label_size.y };

	if (ImGui::IsMouseHoveringRect(label_min, label_max, false)) {
		DrawRichTextTooltipAboveRect(
			"Live preview of size, color, BIUS, spacing, SDF layers and glyph effects.",
			label_min, label_max
		);
	}
}

[[nodiscard]] std::string FormatRichTextEditorFloat(float value) {
	char buffer[64]{};
	std::snprintf(buffer, sizeof(buffer), "%.4g", static_cast<double>(value));
	return buffer;
}

[[nodiscard]] std::string_view RichTextEditorGlyphEffectName(GlyphEffectType type) {
	switch (type) {
		case GlyphEffectType::Wobble: return "Wobble";
		case GlyphEffectType::Wave: return "Wave";
		case GlyphEffectType::Shake: return "Shake";
		case GlyphEffectType::Pulse: return "Pulse";
		case GlyphEffectType::None: return "None";
	}
	return "None";
}

[[nodiscard]] bool RichTextLayerActive(const DistanceFieldLayerStyle& layer) {
	return layer != DistanceFieldLayerStyle{};
}

void DrawRichTextToolbar(
	EditorContext& ctx, const TextRunDefaults& defaults,
	const RichTextEditorOptions& options, RichTextEditorState& state,
	RichTextSelectionState& selection, bool allow_detached_window
) {
	const float button_height{ ImGui::GetFrameHeight() };
	const auto format{
		ResolveRichTextEditorSelectionFormat(state.source, selection, defaults)
	};
	const bool page_preview_read_only{
		options.show_page_numbers_button && state.show_page_numbers
	};

	auto begin_text_action = [&]() { ctx.undo.CommitActiveEdit(); };
	auto preserve_source_selection = [&]() {
		selection.apply_selection = true;
		selection.keep_selection_highlight = true;
	};

	auto set_selection_warning = [&](std::string warning) {
		state.selection_warning = std::move(warning);
		state.selection_warning_until = ImGui::GetTime() + 4.0;
	};

	auto validate_format_selection = [&]() {
		if (const auto error{ RichTextFormattingTargetError(state.source, selection) }) {
			set_selection_warning(*error);
			return false;
		}
		state.selection_warning.clear();
		state.selection_warning_until = 0.0;
		return true;
	};

	auto validate_insert_selection = [&]() {
		if (const auto error{
				RichTextFormattingTargetError(state.source, selection, true)
			}) {
			set_selection_warning(*error);
			return false;
		}
		state.selection_warning.clear();
		state.selection_warning_until = 0.0;
		return true;
	};

	auto selection_has_range = [&]() {
		return RichTextSelectionHasRange(state.source, selection);
	};

	auto caret_inside_tag = [&](std::string_view tag) {
		if (selection_has_range()) {
			return false;
		}
		const std::size_t cursor{ std::min(selection.cursor, state.source.size()) };
		return FindRichTextSurroundingTag(
			state.source, cursor, cursor, tag
		).has_value();
	};

	auto can_edit_or_add_tag = [&](std::string_view tag, std::string_view description) {
		if (!validate_format_selection()) {
			return false;
		}
		if (selection_has_range() || caret_inside_tag(tag)) {
			return true;
		}

		set_selection_warning(
			"Select text before applying " + std::string{ description } + "."
		);
		return false;
	};

	auto queue_toggle_tag = [&](
		std::string_view tag, std::string_view open, std::string_view close
	) {
		if (page_preview_read_only || !validate_format_selection()) {
			return false;
		}
		selection.pending_action = RichTextPendingAction{
			.type = RichTextPendingActionType::ToggleTag,
			.cursor = selection.cursor,
			.selection_start = selection.selection_start,
			.selection_end = selection.selection_end,
			.tag = std::string{ tag },
			.open = std::string{ open },
			.close = std::string{ close },
		};
		selection.apply_selection = true;
		selection.keep_selection_highlight = true;
		selection.focus_source = true;
		return true;
	};

	auto queue_set_tag = [&](
		std::string_view tag, std::string open, std::string_view close
	) {
		if (page_preview_read_only || !validate_format_selection()) {
			return false;
		}
		selection.pending_action = RichTextPendingAction{
			.type = RichTextPendingActionType::SetTag,
			.cursor = selection.cursor,
			.selection_start = selection.selection_start,
			.selection_end = selection.selection_end,
			.tag = std::string{ tag },
			.open = std::move(open),
			.close = std::string{ close },
		};
		selection.apply_selection = true;
		selection.keep_selection_highlight = true;
		return true;
	};

	auto queue_remove_tag = [&](std::string_view tag) {
		if (page_preview_read_only || !validate_format_selection()) {
			return false;
		}
		selection.pending_action = RichTextPendingAction{
			.type = RichTextPendingActionType::RemoveTag,
			.cursor = selection.cursor,
			.selection_start = selection.selection_start,
			.selection_end = selection.selection_end,
			.tag = std::string{ tag },
		};
		selection.apply_selection = true;
		selection.keep_selection_highlight = true;
		return true;
	};

	auto queue_remove_effects = [&]() {
		if (page_preview_read_only || !validate_format_selection()) {
			return false;
		}
		selection.pending_action = RichTextPendingAction{
			.type = RichTextPendingActionType::RemoveEffects,
			.cursor = selection.cursor,
			.selection_start = selection.selection_start,
			.selection_end = selection.selection_end,
		};
		selection.apply_selection = true;
		selection.keep_selection_highlight = true;
		return true;
	};

	auto queue_insert_token = [&](std::string token) {
		if (page_preview_read_only || !validate_insert_selection()) {
			return false;
		}
		selection.pending_action = RichTextPendingAction{
			.type = RichTextPendingActionType::InsertToken,
			.cursor = selection.cursor,
			.selection_start = selection.selection_start,
			.selection_end = selection.selection_end,
			.token = std::move(token),
		};
		selection.apply_selection = true;
		selection.keep_selection_highlight = true;
		return true;
	};

	auto queue_toggle_standalone_line_token = [&](std::string token) {
		if (page_preview_read_only) {
			return false;
		}
		selection.pending_action = RichTextPendingAction{
			.type = RichTextPendingActionType::ToggleStandaloneLineToken,
			.cursor = selection.cursor,
			.selection_start = selection.selection_start,
			.selection_end = selection.selection_end,
			.token = std::move(token),
		};
		selection.apply_selection = true;
		selection.keep_selection_highlight = true;
		selection.focus_source = true;
		return true;
	};

	auto tag_button = [&](
		const char* label, std::string_view tag, std::string_view open,
		std::string_view close, std::string_view tooltip,
		std::optional<bool> active, std::string_view shortcut
	) {
		const bool mixed{ format.valid && !active.has_value() };
		if (active == true) {
			ImGui::PushStyleColor(
				ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)
			);
		} else if (mixed) {
			ImGui::PushStyleColor(
				ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered)
			);
		}

		if (ImGui::Button(label, ImVec2{ 0.0f, button_height })) {
			if (selection_has_range() || caret_inside_tag(tag)) {
				begin_text_action();
				queue_toggle_tag(tag, open, close);
			} else {
				set_selection_warning("Select text before applying this style.");
			}
		}
		if (active == true || mixed) {
			ImGui::PopStyleColor();
		}

		std::string full_tooltip{ tooltip };
		if (!shortcut.empty()) {
			full_tooltip += "\nShortcut: ";
			full_tooltip += shortcut;
		}
		DrawRichTextToolbarTooltip(full_tooltip);
	};

	tag_button(
		"B", "b", "<b>", "</b>", "Bold.\n<b>...</b>\n<b=0.2>...</b>",
		RichTextSelectionFlag(format, FontStyle::Bold), "Ctrl/Cmd+B"
	);
	ImGui::SameLine();
	tag_button(
		"I", "i", "<i>", "</i>", "Italic.\n<i>...</i>",
		RichTextSelectionFlag(format, FontStyle::Italic), "Ctrl/Cmd+I"
	);
	ImGui::SameLine();
	tag_button(
		"U", "u", "<u>", "</u>", "Underline.\n<u>...</u>",
		RichTextSelectionFlag(format, FontStyle::Underline), "Ctrl/Cmd+U"
	);
	ImGui::SameLine();
	tag_button(
		"S", "s", "<s>", "</s>", "Strikethrough.\n<s>...</s>",
		RichTextSelectionFlag(format, FontStyle::Strikethrough), {}
	);

	const auto selection_color{ RichTextSelectionColor(format) };
	ImGui::SameLine();
	std::array<float, 4> toolbar_color{};
	SetRichTextEditorColor(
		toolbar_color, selection_color.value_or(defaults.style.color)
	);
	const float color_button_size{ button_height };
	if (ImGui::ColorButton(
			"##RichTextColor",
			ImVec4{ toolbar_color[0], toolbar_color[1], toolbar_color[2], toolbar_color[3] },
			ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip,
			ImVec2{ color_button_size, color_button_size }
		)) {
		if (can_edit_or_add_tag("c", "a text color")) {
			state.color = toolbar_color;
			preserve_source_selection();
			ImGui::OpenPopup("RichTextColorPopup");
		}
	}
	const ImVec2 color_button_min{ ImGui::GetItemRectMin() };
	const ImVec2 color_button_max{ ImGui::GetItemRectMax() };
	DrawRichTextToolbarTooltip(
		selection_color.has_value()
			? "Text color.\n<c=red>...</c>\n<c=#RRGGBB>...</c>\n<c=#RRGGBBAA>...</c>"
			: "Text color: Mixed.\nChoose a color to apply it to the full selection."
	);
	if (ImGui::IsPopupOpen("RichTextColorPopup")) {
		SetNextRichTextColorPickerPopupAbove(color_button_min, color_button_max);
	}
	if (ImGui::BeginPopup(
			"RichTextColorPopup",
			ImGuiWindowFlags_HorizontalScrollbar
		)) {
		FitCurrentRichTextColorPickerPopupToWorkArea(
			color_button_min,
			color_button_max
		);

		Color selected_color{ V4_float{
			state.color[0],
			state.color[1],
			state.color[2],
			state.color[3],
		} };

		const auto color_result{ DrawColorPickerContents(
			ctx,
			"RichTextColorPicker",
			selected_color,
			ImGuiColorEditFlags_AlphaBar |
				ImGuiColorEditFlags_AlphaPreviewHalf |
				ImGuiColorEditFlags_DisplayRGB |
				ImGuiColorEditFlags_InputRGB |
				ImGuiColorEditFlags_Uint8,
			std::addressof(defaults.style.color)
		) };
		if (color_result.interaction_started) {
			begin_text_action();
		}
		if (color_result.use_default) {
			SetRichTextEditorColor(state.color, defaults.style.color);
			queue_remove_tag("c");
		} else if (color_result.changed) {
			SetRichTextEditorColor(state.color, selected_color);
			queue_set_tag(
				"c", "<c=" + RichTextEditorColorTag(state.color) + ">", "</c>"
			);
		}
		ImGui::EndPopup();
	}

	const auto selection_font{ RichTextSelectionFont(format) };
	std::string font_display;
	if (format.valid && !selection_font.has_value()) {
		font_display = "Mixed";
	} else {
		font_display = selection_font.value_or(defaults.font).value;
		if (font_display.empty()) {
			font_display = "Default";
		}
		if (font_display.size() > 18) {
			font_display.resize(15);
			font_display += "...";
		}
	}

	ImGui::SameLine();
	const std::string font_button{ "Font: " + font_display + "##RichTextFontButton" };
	if (ImGui::Button(font_button.c_str(), ImVec2{ 0.0f, button_height })) {
		if (can_edit_or_add_tag("font", "a font")) {
			state.font = selection_font.value_or(defaults.font).value;
			preserve_source_selection();
			ImGui::OpenPopup("RichTextFontPopup");
		}
	}
	const ImVec2 font_button_min{ ImGui::GetItemRectMin() };
	const ImVec2 font_button_max{ ImGui::GetItemRectMax() };
	DrawRichTextToolbarTooltip(
		selection_font.has_value()
			? "Font key.\n<font=key>...</font>"
			: "Font: Mixed.\nChoose a font to apply it to the full selection."
	);
	if (ImGui::IsPopupOpen("RichTextFontPopup")) {
		SetNextRichTextPopupAbove(font_button_min, font_button_max);
	}
	if (ImGui::BeginPopup(
			"RichTextFontPopup",
			ImGuiWindowFlags_HorizontalScrollbar
		)) {
		auto apply_font = [&]() {
			if (FontKey{ state.font } == defaults.font) {
				queue_remove_tag("font");
			} else {
				queue_set_tag("font", "<font=" + state.font + ">", "</font>");
			}
		};

		const auto loaded_fonts{ GetLoadedRichTextFontKeys(ctx) };
		const bool custom_font_key{ !std::ranges::contains(loaded_fonts, state.font) };
		const char* preview{
			custom_font_key ? "Custom" : (state.font.empty() ? "Default" : state.font.c_str())
		};
		const ImVec2 loaded_font_combo_min{ ImGui::GetCursorScreenPos() };
		const ImVec2 loaded_font_combo_max{
			loaded_font_combo_min.x + 260.0f,
			loaded_font_combo_min.y + ImGui::GetFrameHeight(),
		};
		ImGui::SetNextItemWidth(260.0f);
		SetNextRichTextPopupAbove(
			loaded_font_combo_min,
			loaded_font_combo_max
		);
		if (ImGui::BeginCombo(
				"Loaded##RichTextFont",
				preview,
				ImGuiComboFlags_HeightLarge
			)) {
			for (const auto& font : loaded_fonts) {
				const bool selected{ !custom_font_key && state.font == font };
				const char* label{ font.empty() ? "Default" : font.c_str() };
				if (ImGui::Selectable(label, selected)) {
					begin_text_action();
					state.font = font;
					apply_font();
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SetNextItemWidth(260.0f);
		const bool font_changed{ ImGui::InputTextWithHint(
			"##RichTextCustomFont", "Custom font key", &state.font
		) };
		const ImVec2 custom_font_min{ ImGui::GetItemRectMin() };
		const ImVec2 custom_font_max{ ImGui::GetItemRectMax() };
		const bool custom_font_hovered{ ImGui::IsItemHovered() };
		if (ImGui::IsItemActivated()) {
			begin_text_action();
		}
		if (font_changed) {
			apply_font();
		}

		const auto availability{ GetRichTextFontAvailability(ctx, FontKey{ state.font }) };
		const bool invalid_font_key{ !state.font.empty() && !availability.IsAvailable() };
		if (invalid_font_key) {
			ImGui::GetWindowDrawList()->AddRect(
				custom_font_min,
				custom_font_max,
				IM_COL32(230, 50, 50, 255),
				ImGui::GetStyle().FrameRounding,
				0,
				1.0f
			);
			if (custom_font_hovered) {
				if (const char* tooltip{ RichTextFontAvailabilityTooltip(availability) }) {
					ImGui::SetTooltip("%s", tooltip);
				}
			}
		}

		if (ImGui::Button("Use Default Font", ImVec2{ -FLT_MIN, 0.0f })) {
			begin_text_action();
			state.font = defaults.font.value;
			queue_remove_tag("font");
		}
		DrawRichTextToolbarTooltip(
			"Remove the explicit font tag and use the rich text Defaults font."
		);
		ImGui::EndPopup();
	}

	const auto selection_size{ RichTextSelectionSize(format) };
	std::string size_display{
		(format.valid && !selection_size.has_value())
			? "Mixed"
			: FormatRichTextEditorFloat(selection_size.value_or(defaults.style.size))
	};
	ImGui::SameLine();
	const std::string size_button{ "Size: " + size_display + "##RichTextSizeButton" };
	if (ImGui::Button(size_button.c_str(), ImVec2{ 0.0f, button_height })) {
		if (can_edit_or_add_tag("size", "a font size")) {
			state.size = selection_size.value_or(defaults.style.size);
			preserve_source_selection();
			ImGui::OpenPopup("RichTextSizePopup");
		}
	}
	const ImVec2 size_button_min{ ImGui::GetItemRectMin() };
	const ImVec2 size_button_max{ ImGui::GetItemRectMax() };
	DrawRichTextToolbarTooltip(
		selection_size.has_value()
			? "Font size.\n<size=32>...</size>"
			: "Font size: Mixed.\nChoose a size to apply it to the full selection."
	);
	if (ImGui::IsPopupOpen("RichTextSizePopup")) {
		SetNextRichTextPopupAbove(size_button_min, size_button_max);
	}
	if (ImGui::BeginPopup(
			"RichTextSizePopup",
			ImGuiWindowFlags_HorizontalScrollbar
		)) {
		auto apply_size = [&]() {
			if (std::abs(state.size - defaults.style.size) <= 0.0001f) {
				queue_remove_tag("size");
			} else {
				queue_set_tag(
					"size", "<size=" + FormatRichTextEditorFloat(state.size) + ">", "</size>"
				);
			}
		};

		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		if (ImGui::Button("-##RichTextSize", ImVec2{ button_height, button_height })) {
			begin_text_action();
			StepRichTextCommonFontSize(state.size, -1);
			apply_size();
		}
		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(120.0f);
		const bool size_changed{ ImGui::DragFloat(
			"##RichTextSize", &state.size, 0.5f, 1.0f, 1000.0f, "%.1f",
			ImGuiSliderFlags_AlwaysClamp
		) };
		if (ImGui::IsItemActivated()) {
			begin_text_action();
		}
		if (size_changed) {
			apply_size();
		}
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::Button("+##RichTextSize", ImVec2{ button_height, button_height })) {
			begin_text_action();
			StepRichTextCommonFontSize(state.size, 1);
			apply_size();
		}

		if (ImGui::Button("Use Default Size", ImVec2{ -FLT_MIN, 0.0f })) {
			begin_text_action();
			state.size = defaults.style.size;
			queue_remove_tag("size");
		}
		DrawRichTextToolbarTooltip(
			"Remove the explicit size tag and use the rich text Defaults size."
		);
		ImGui::EndPopup();
	}

	const auto glyph_effect{ RichTextSelectionGlyphEffect(format) };
	const auto outline{ RichTextSelectionLayer(format, RichTextEffectEditorKind::Outline) };
	const auto shadow{ RichTextSelectionLayer(format, RichTextEffectEditorKind::Shadow) };
	const auto outer_glow{ RichTextSelectionLayer(format, RichTextEffectEditorKind::OuterGlow) };
	const auto inner_glow{ RichTextSelectionLayer(format, RichTextEffectEditorKind::InnerGlow) };
	const auto shadow_offset{ RichTextSelectionShadowOffset(format) };

	const bool effects_resolved{
		format.valid && glyph_effect.has_value() && outline.has_value() &&
		shadow.has_value() && outer_glow.has_value() && inner_glow.has_value() &&
		shadow_offset.has_value()
	};
	const bool effects_mixed{ format.valid && !effects_resolved };
	std::vector<std::string_view> active_effects;
	if (effects_resolved) {
		if (glyph_effect->type != GlyphEffectType::None) {
			active_effects.push_back(RichTextEditorGlyphEffectName(glyph_effect->type));
		}
		if (RichTextLayerActive(*outline)) active_effects.push_back("Outline");
		if (RichTextLayerActive(*shadow)) active_effects.push_back("Shadow");
		if (RichTextLayerActive(*outer_glow)) active_effects.push_back("Outer Glow");
		if (RichTextLayerActive(*inner_glow)) active_effects.push_back("Inner Glow");
	}

	std::string effects_preview{ "Effects" };
	if (effects_mixed) {
		effects_preview = "Effects: Mixed";
	} else if (active_effects.size() == 1) {
		effects_preview = "Effects: " + std::string{ active_effects.front() };
	} else if (active_effects.size() > 1) {
		effects_preview = "Effects: " + std::to_string(active_effects.size());
	}

	auto effect_kind_tag = [](RichTextEffectEditorKind kind) -> std::string_view {
		switch (kind) {
			case RichTextEffectEditorKind::Glyph: return "fx";
			case RichTextEffectEditorKind::Outline: return "outline";
			case RichTextEffectEditorKind::Shadow: return "shadow";
			case RichTextEffectEditorKind::OuterGlow: return "outerglow";
			case RichTextEffectEditorKind::InnerGlow: return "innerglow";
		}
		return {};
	};

	auto effect_kind_is_active = [&](RichTextEffectEditorKind kind) {
		if (!effects_resolved) {
			return false;
		}

		// A collapsed caret only exposes Edit Effect for an explicitly authored
		// wrapper that contains that caret. Default effects do not create an edit target.
		if (!selection_has_range() && !caret_inside_tag(effect_kind_tag(kind))) {
			return false;
		}
		switch (kind) {
			case RichTextEffectEditorKind::Glyph:
				return glyph_effect->type != GlyphEffectType::None;
			case RichTextEffectEditorKind::Outline:
				return RichTextLayerActive(*outline);
			case RichTextEffectEditorKind::Shadow:
				return RichTextLayerActive(*shadow);
			case RichTextEffectEditorKind::OuterGlow:
				return RichTextLayerActive(*outer_glow);
			case RichTextEffectEditorKind::InnerGlow:
				return RichTextLayerActive(*inner_glow);
		}
		return false;
	};

	std::optional<RichTextEffectEditorKind> editable_effect_kind;
	if (state.effect_editor_kind.has_value() &&
		effect_kind_is_active(*state.effect_editor_kind)) {
		editable_effect_kind = state.effect_editor_kind;
	} else {
		for (const auto kind : {
				RichTextEffectEditorKind::Glyph,
				RichTextEffectEditorKind::Outline,
				RichTextEffectEditorKind::Shadow,
				RichTextEffectEditorKind::OuterGlow,
				RichTextEffectEditorKind::InnerGlow,
			}) {
			if (effect_kind_is_active(kind)) {
				editable_effect_kind = kind;
				break;
			}
		}
	}

	bool open_effect_popup{ false };
	bool effects_cleared_this_frame{ false };
	std::optional<std::pair<ImVec2, ImVec2>> effect_popup_anchor;

	auto load_preset = [&](std::string_view open, std::string_view close) -> TextRunStyle {
		const std::string sample{ std::string{ open } + "x" + std::string{ close } };
		const auto parsed{ ParseRichText(sample, defaults) };
		return parsed.text.runs.empty() ? defaults.style : parsed.text.runs.front().style;
	};

	auto load_effect_editor_state = [&](
		RichTextEffectEditorKind kind,
		const TextRunStyle& fallback
	) {
		state.effect_editor_kind = kind;
		if (kind == RichTextEffectEditorKind::Glyph) {
			const auto current{ RichTextSelectionGlyphEffect(format) };
			const GlyphEffectStyle effect{
				current.has_value() && current->type != GlyphEffectType::None
					? *current
					: fallback.effect
			};
			state.effect_type = effect.type;
			state.effect_amplitude = effect.amplitude;
			state.effect_frequency = effect.frequency;
			state.effect_speed = effect.speed;
			state.effect_phase = effect.phase;
			return;
		}

		const auto current{ RichTextSelectionLayer(format, kind) };
		const DistanceFieldLayerStyle fallback_layer{
			kind == RichTextEffectEditorKind::Outline ? fallback.sdf.outline :
			kind == RichTextEffectEditorKind::Shadow ? fallback.sdf.shadow :
			kind == RichTextEffectEditorKind::OuterGlow ? fallback.sdf.outer_glow :
			fallback.sdf.inner_glow
		};
		const DistanceFieldLayerStyle layer{
			current.has_value() && RichTextLayerActive(*current)
				? *current
				: fallback_layer
		};
		SetRichTextEditorColor(state.effect_color, layer.color);
		state.effect_width = layer.width;
		state.effect_softness = layer.softness;
		if (kind == RichTextEffectEditorKind::Shadow) {
			state.effect_shadow_offset = shadow_offset.value_or(fallback.sdf.shadow_offset);
		}
	};

	auto begin_effect_editor = [&] (
		RichTextEffectEditorKind kind, std::string_view tag,
		std::string_view open, std::string_view close
	) {
		if (page_preview_read_only || !validate_format_selection()) {
			return;
		}
		if (!selection_has_range()) {
			set_selection_warning("Select text before applying an effect.");
			return;
		}

		begin_text_action();
		preserve_source_selection();
		const TextRunStyle preset{ load_preset(open, close) };
		load_effect_editor_state(kind, preset);
		queue_set_tag(tag, std::string{ open }, close);
		open_effect_popup = true;
	};

	auto edit_existing_effect = [&](RichTextEffectEditorKind kind) {
		if (page_preview_read_only ||
			!validate_format_selection() || !effect_kind_is_active(kind)) {
			return;
		}
		preserve_source_selection();
		load_effect_editor_state(kind, defaults.style);
		open_effect_popup = true;
	};

	ImGui::SameLine();
	const ImVec2 effects_combo_min{ ImGui::GetCursorScreenPos() };
	const ImVec2 effects_combo_max{
		effects_combo_min.x + 120.0f,
		effects_combo_min.y + ImGui::GetFrameHeight(),
	};
	ImGui::SetNextItemWidth(120.0f);
	SetNextRichTextPopupAbove(effects_combo_min, effects_combo_max);
	const bool effects_open{
		ImGui::BeginCombo(
			"##RichTextEffects",
			effects_preview.c_str(),
			ImGuiComboFlags_HeightLargest
		)
	};
	if (ImGui::IsItemActivated()) {
		preserve_source_selection();
	}

	if (effects_open) {
		if (ImGui::Selectable("Reset Effects")) {
			begin_text_action();
			queue_remove_effects();
			state.effect_editor_kind.reset();
			effects_cleared_this_frame = true;
		}
		DrawRichTextToolbarTooltip(
			"Remove glyph, outline, shadow and glow effect tags from this selection/caret."
		);
		ImGui::Separator();

		if (ImGui::Selectable("Wave")) {
			begin_effect_editor(
				RichTextEffectEditorKind::Glyph, "fx",
				"<fx=Wave,8,2,1,0>", "</fx>"
			);
		}
		if (ImGui::Selectable("Wobble")) {
			begin_effect_editor(
				RichTextEffectEditorKind::Glyph, "fx",
				"<fx=Wobble,4,2,1,0>", "</fx>"
			);
		}
		if (ImGui::Selectable("Shake")) {
			begin_effect_editor(
				RichTextEffectEditorKind::Glyph, "fx",
				"<fx=Shake,3,20,1,0>", "</fx>"
			);
		}
		if (ImGui::Selectable("Pulse")) {
			begin_effect_editor(
				RichTextEffectEditorKind::Glyph, "fx",
				"<fx=Pulse,0.15,2,1,0>", "</fx>"
			);
		}

		ImGui::Separator();
		if (ImGui::Selectable("Outline")) {
			begin_effect_editor(
				RichTextEffectEditorKind::Outline, "outline",
				"<outline=#000000,2,1>", "</outline>"
			);
		}
		if (ImGui::Selectable("Shadow")) {
			begin_effect_editor(
				RichTextEffectEditorKind::Shadow, "shadow",
				"<shadow=#00000080,3,3,0,1>", "</shadow>"
			);
		}
		if (ImGui::Selectable("Outer Glow")) {
			begin_effect_editor(
				RichTextEffectEditorKind::OuterGlow, "outerglow",
				"<outerglow=#00FFFF,4,1>", "</outerglow>"
			);
		}
		if (ImGui::Selectable("Inner Glow")) {
			begin_effect_editor(
				RichTextEffectEditorKind::InnerGlow, "innerglow",
				"<innerglow=#FFFFFF,2,1>", "</innerglow>"
			);
		}
		ImGui::EndCombo();
	}
	DrawRichTextToolbarTooltip(
		"Effect tags. Selecting an effect opens its live parameter editor."
	);

	if (!options.standalone_line_tag.empty()) {
		ImGui::SameLine();
		const std::string label{
			options.standalone_line_button_label.empty()
				? std::string{ "Line Tag" }
				: std::string{ options.standalone_line_button_label }
		};
		ImGui::BeginDisabled(
			options.show_page_numbers_button && state.show_page_numbers
		);
		if (ImGui::Button(label.c_str(), ImVec2{ 0.0f, button_height })) {
			begin_text_action();
			queue_toggle_standalone_line_token(
				std::string{ options.standalone_line_tag }
			);
		}
		ImGui::EndDisabled();
		DrawRichTextToolbarTooltip(
			options.standalone_line_tooltip.empty()
				? std::string_view{
					"Toggle this control on the selected blank line or above the selected paragraph."
				}
				: options.standalone_line_tooltip
		);
	}

	const bool effect_popup_open{
		ImGui::IsPopupOpen("RichTextEffectEditorPopup")
	};
	std::optional<RichTextEffectEditorKind> edit_button_kind{ editable_effect_kind };
	if (!edit_button_kind.has_value() &&
		(open_effect_popup || effect_popup_open) &&
		state.effect_editor_kind.has_value()) {
		edit_button_kind = state.effect_editor_kind;
	}
	if (!effects_cleared_this_frame && edit_button_kind.has_value()) {
		ImGui::SameLine();
		if (ImGui::Button("Edit Effect", ImVec2{ 0.0f, button_height })) {
			edit_existing_effect(*edit_button_kind);
		}
		const ImVec2 edit_min{ ImGui::GetItemRectMin() };
		const ImVec2 edit_max{ ImGui::GetItemRectMax() };
		DrawRichTextToolbarTooltip("Edit the currently selected effect parameters.");

		effect_popup_anchor = std::pair{ edit_min, edit_max };
	}

	if (open_effect_popup) {
		ImGui::OpenPopup("RichTextEffectEditorPopup");
	}
	if (ImGui::IsPopupOpen("RichTextEffectEditorPopup")) {
		if (effect_popup_anchor.has_value()) {
			// Keep the popup immediately above the Edit Effect button. Its right edge
			// stays aligned with the button, and its maximum height is limited to the
			// available space above so oversized content becomes scrollable.
			SetNextRichTextPopupAbove(
				effect_popup_anchor->first,
				effect_popup_anchor->second,
				true,
				ImGuiCond_Always
			);
		} else {
			SetNextRichTextPopupSizeConstraints();
		}
	}
	if (ImGui::BeginPopup(
			"RichTextEffectEditorPopup",
			ImGuiWindowFlags_HorizontalScrollbar
		)) {
		if (!state.effect_editor_kind.has_value()) {
			ImGui::TextDisabled("No effect selected.");
		} else if (*state.effect_editor_kind == RichTextEffectEditorKind::Glyph) {
			bool commit{ false };
			ImGui::SetNextItemWidth(180.0f);
			const std::string_view current_name{
				RichTextEditorGlyphEffectName(state.effect_type)
			};
			if (ImGui::BeginCombo("Type##RichTextGlyphEffect", current_name.data())) {
				for (const auto type : magic_enum::enum_values<GlyphEffectType>()) {
					const bool selected{ type == state.effect_type };
					const std::string name{ magic_enum::enum_name(type) };
					if (ImGui::Selectable(name.c_str(), selected)) {
						begin_text_action();
						state.effect_type = type;
						commit = true;
					}
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			auto drag = [&](const char* label, float& value, float speed) {
				ImGui::SetNextItemWidth(180.0f);
				ImGui::DragFloat(label, &value, speed, 0.0f, 0.0f, "%.3f");
				if (ImGui::IsItemActivated()) {
					begin_text_action();
				}
				if (ImGui::IsItemDeactivatedAfterEdit()) {
					commit = true;
				}
			};
			drag("Amplitude", state.effect_amplitude, 0.05f);
			drag("Frequency", state.effect_frequency, 0.05f);
			drag("Speed", state.effect_speed, 0.05f);
			drag("Phase", state.effect_phase, 0.05f);

			if (commit) {
				if (state.effect_type == GlyphEffectType::None) {
					queue_remove_tag("fx");
					state.effect_editor_kind.reset();
					ImGui::CloseCurrentPopup();
				} else {
					queue_set_tag(
						"fx",
						"<fx=" + std::string{ RichTextEditorGlyphEffectName(state.effect_type) } +
							"," + FormatRichTextEditorFloat(state.effect_amplitude) +
							"," + FormatRichTextEditorFloat(state.effect_frequency) +
							"," + FormatRichTextEditorFloat(state.effect_speed) +
							"," + FormatRichTextEditorFloat(state.effect_phase) + ">",
						"</fx>"
					);
				}
			}

			if (state.effect_editor_kind.has_value() &&
				ImGui::Button("Remove Effect", ImVec2{ -FLT_MIN, 0.0f })) {
				begin_text_action();
				queue_remove_tag("fx");
				state.effect_editor_kind.reset();
				ImGui::CloseCurrentPopup();
			}
		} else {
			const auto kind{ *state.effect_editor_kind };
			const char* label{
				kind == RichTextEffectEditorKind::Outline ? "Outline" :
				kind == RichTextEffectEditorKind::Shadow ? "Shadow" :
				kind == RichTextEffectEditorKind::OuterGlow ? "Outer Glow" :
				"Inner Glow"
			};
			ImGui::TextUnformatted(label);

			bool changed{ false };
			Color selected_effect_color{ V4_float{
				state.effect_color[0],
				state.effect_color[1],
				state.effect_color[2],
				state.effect_color[3],
			} };

			// Keep the large palette-capable picker in its own popup instead of
			// flattening it into the effect editor. This is especially important on
			// smaller macOS work areas where the full picker would otherwise overflow.
			ImGui::PushID(static_cast<int>(kind));
			ImGui::TextUnformatted("Color");
			ImGui::SameLine();
			const float effect_color_button_size{
				std::max(12.0f, button_height - 5.0f)
			};
			if (ImGui::ColorButton(
					"##RichTextEffectColorButton",
					ImVec4{
						state.effect_color[0],
						state.effect_color[1],
						state.effect_color[2],
						state.effect_color[3],
					},
					ImGuiColorEditFlags_AlphaPreviewHalf |
						ImGuiColorEditFlags_NoTooltip,
					ImVec2{
						effect_color_button_size,
						effect_color_button_size,
					}
				)) {
				ImGui::OpenPopup("RichTextEffectColorPopup");
			}
			const ImVec2 effect_color_button_min{ ImGui::GetItemRectMin() };
			const ImVec2 effect_color_button_max{ ImGui::GetItemRectMax() };

			if (ImGui::IsPopupOpen("RichTextEffectColorPopup")) {
				SetNextRichTextColorPickerPopupAbove(
					effect_color_button_min,
					effect_color_button_max,
					true
				);
			}
			if (ImGui::BeginPopup(
					"RichTextEffectColorPopup",
					ImGuiWindowFlags_HorizontalScrollbar
				)) {
				FitCurrentRichTextColorPickerPopupToWorkArea(
					effect_color_button_min,
					effect_color_button_max,
					true
				);

				const auto effect_color_result{ DrawColorPickerContents(
					ctx,
					"RichTextEffectColorPicker",
					selected_effect_color
				) };
				if (effect_color_result.interaction_started) {
					begin_text_action();
				}
				if (effect_color_result.changed) {
					SetRichTextEditorColor(
						state.effect_color,
						selected_effect_color
					);
					changed = true;
				}
				ImGui::EndPopup();
			}
			ImGui::PopID();

			bool parameter_commit{ false };
			auto drag_parameter = [&](auto&& draw) {
				std::invoke(std::forward<decltype(draw)>(draw));
				if (ImGui::IsItemActivated()) {
					begin_text_action();
				}
				if (ImGui::IsItemDeactivatedAfterEdit()) {
					parameter_commit = true;
				}
			};

			ImGui::SetNextItemWidth(180.0f);
			drag_parameter([&]() {
				ImGui::DragFloat(
					"Width", &state.effect_width, 0.1f, 0.0f, 0.0f, "%.2f"
				);
			});

			ImGui::SetNextItemWidth(180.0f);
			drag_parameter([&]() {
				ImGui::DragFloat(
					"Softness", &state.effect_softness, 0.05f, 0.0f, 0.0f, "%.2f"
				);
			});

			if (kind == RichTextEffectEditorKind::Shadow) {
				ImGui::SetNextItemWidth(180.0f);
				drag_parameter([&]() {
					ImGui::DragFloat2(
						"Offset", &state.effect_shadow_offset.x, 0.1f, 0.0f, 0.0f, "%.2f"
					);
				});
			}

			std::string_view tag;
			std::string_view close;
			switch (kind) {
				case RichTextEffectEditorKind::Outline:
					tag = "outline";
					close = "</outline>";
					break;
				case RichTextEffectEditorKind::Shadow:
					tag = "shadow";
					close = "</shadow>";
					break;
				case RichTextEffectEditorKind::OuterGlow:
					tag = "outerglow";
					close = "</outerglow>";
					break;
				case RichTextEffectEditorKind::InnerGlow:
					tag = "innerglow";
					close = "</innerglow>";
					break;
				case RichTextEffectEditorKind::Glyph:
					break;
			}

			if (changed || parameter_commit) {
				std::string open{
					"<" + std::string{ tag } + "=" +
					RichTextEditorColorTag(state.effect_color) + ","
				};
				if (kind == RichTextEffectEditorKind::Shadow) {
					open += FormatRichTextEditorFloat(state.effect_shadow_offset.x) + ",";
					open += FormatRichTextEditorFloat(state.effect_shadow_offset.y) + ",";
				}
				open += FormatRichTextEditorFloat(state.effect_width) + ",";
				open += FormatRichTextEditorFloat(state.effect_softness) + ">";
				queue_set_tag(tag, std::move(open), close);
			}

			if (ImGui::Button("Remove Effect", ImVec2{ -FLT_MIN, 0.0f })) {
				begin_text_action();
				queue_remove_tag(tag);
				state.effect_editor_kind.reset();
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	const bool word_wrap_button_active{ state.word_wrap };
	if (word_wrap_button_active) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	}
	if (ImGui::Button("WW", ImVec2{ 0.0f, button_height })) {
		state.word_wrap = !state.word_wrap;
	}
	if (word_wrap_button_active) {
		ImGui::PopStyleColor();
	}
	DrawRichTextToolbarTooltip(
		state.word_wrap
			? "Disable word wrapping in the text input."
			: "Enable word wrapping in the text input."
	);

	if (options.show_page_numbers_button) {
		ImGui::SameLine();
		const bool active{ state.show_page_numbers };
		if (active) {
			ImGui::PushStyleColor(
				ImGuiCol_Button,
				ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)
			);
		}
		if (ImGui::Button("[#]", ImVec2{ 0.0f, button_height })) {
			state.show_page_numbers = !state.show_page_numbers;
			if (state.show_page_numbers) {
				selection.pending_action.reset();
				selection.focus_source = false;
				selection.keep_selection_highlight = false;
				state.selection_warning.clear();
				state.selection_warning_until = 0.0;
			}
		}
		if (active) {
			ImGui::PopStyleColor();
		}
		DrawRichTextToolbarTooltip(
			state.show_page_numbers
				? "Hide dialogue page numbers beside source lines."
				: "Show dialogue page numbers beside source lines."
		);
	}

	if (allow_detached_window) {
		ImGui::SameLine();
		if (ImGui::Button("Open", ImVec2{ 0.0f, button_height })) {
			state.window_open = true;
			state.window_selection = selection;
			state.window_selection.apply_selection = true;
		}
		DrawRichTextToolbarTooltip("Open a larger rich text editor.");
	}

	if (!options.variables.empty()) {
		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.0f);
		const bool variables_open{ ImGui::BeginCombo("##RichTextVariables", "Variables") };
		if (ImGui::IsItemActivated()) {
			preserve_source_selection();
		}
		if (variables_open) {
			for (const auto& variable : options.variables) {
				const std::string expression{ "${" + std::string{ variable.variable } + "}" };
				if (ImGui::Selectable(std::string{ variable.label }.c_str())) {
					begin_text_action();
					queue_insert_token(expression);
				}
				if (!variable.preview.empty()) {
					const std::string tooltip{
						expression + "\n" + std::string{ variable.preview }
					};
					DrawRichTextToolbarTooltip(tooltip);
				} else {
					DrawRichTextToolbarTooltip(expression);
				}
			}
			ImGui::EndCombo();
		}
		DrawRichTextToolbarTooltip("Insert a context variable.\n${name}");
	}
}

bool DrawRichTextEditorPanel(
	EditorContext& ctx, std::string& source, TextRunDefaults& defaults,
	const RichTextEditorOptions& options, RichTextEditorState& state,
	RichTextSelectionState& selection, bool detached, bool& defaults_changed
) {
	bool changed{ false };
	DrawRichTextToolbar(ctx, defaults, options, state, selection, !detached);

	if (!state.selection_warning.empty()) {
		if (ImGui::GetTime() <= state.selection_warning_until) {
			ImGui::PushStyleColor(
				ImGuiCol_Text, ImVec4{ 1.0f, 0.45f, 0.2f, 1.0f }
			);
			ImGui::TextWrapped("%s", state.selection_warning.c_str());
			ImGui::PopStyleColor();
		} else {
			state.selection_warning.clear();
			state.selection_warning_until = 0.0;
		}
	}

	const float default_editor_height{
		detached
			? std::max(180.0f, ImGui::GetContentRegionAvail().y * 0.45f)
			: ImGui::GetTextLineHeightWithSpacing() *
				  static_cast<float>(std::max(options.line_count, 3)) +
				  ImGui::GetStyle().FramePadding.y * 2.0f
	};
	const float maximum_editor_height{ std::max(
		260.0f, ImGui::GetWindowSize().y * 0.80f
	) };
	float& editor_height{
		detached ? state.window_source_height : state.inline_source_height
	};
	if (DrawRichTextSourceInput(
			ctx, source, state, selection, defaults, options, editor_height,
			default_editor_height, maximum_editor_height
		)) {
		changed = true;
	}

	if (options.show_defaults) {
		// Keep the normal vertical item spacing here, matching the gap between the rich text
		// toolbar and the source input above.
		const bool defaults_open{ ImGui::TreeNodeEx(
			"Defaults##RichTextDefaults",
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
		) };
		DrawRichTextToolbarTooltip(
			"Base style used outside tags.\nTags temporarily override these values."
		);
		if (defaults_open) {
			ScopedIndent indent;
			bool local_defaults_changed{ false };
			local_defaults_changed |= DrawValue(ctx, "Font", defaults.font);
			local_defaults_changed |= DrawValue(ctx, "Color", defaults.style.color);
			local_defaults_changed |= DrawValue(ctx, "Size", defaults.style.size);
			local_defaults_changed |= DrawValue(ctx, "Bold Weight", defaults.style.bold_weight);
			local_defaults_changed |= DrawValue(ctx, "Kerning", defaults.style.kerning);
			local_defaults_changed |= DrawValue(ctx, "Tracking", defaults.style.tracking);
			local_defaults_changed |= DrawValue(ctx, "Line Spacing", defaults.style.line_spacing);
			local_defaults_changed |= DrawValue(ctx, "Flags", defaults.style.flags);
			local_defaults_changed |= DrawValue(ctx, "Distance Field", defaults.style.sdf);
			local_defaults_changed |= DrawValue(ctx, "Effect", defaults.style.effect);
			defaults_changed |= local_defaults_changed;
			changed |= local_defaults_changed;
			ImGui::TreePop();
		}
	}

	const auto source_diagnostics{
		BuildRichTextEditorDiagnostics(ctx, source, defaults, options)
	};
	for (std::size_t i{ 0 }; i < source_diagnostics.size(); ++i) {
		const auto& diagnostic{ source_diagnostics[i] };
		const std::size_t begin{ std::min(diagnostic.position, source.size()) };
		const std::size_t end{ std::min(
			source.size(), begin + std::max<std::size_t>(diagnostic.length, 1)
		) };
		const std::string label{
			"Character " + std::to_string(RichTextCharacterPosition(source, begin)) +
			": " + diagnostic.message
		};

		ImGui::PushID(static_cast<int>(i));
		ImGui::PushStyleColor(
			ImGuiCol_Text, ImVec4{ 1.0f, 0.45f, 0.2f, 1.0f }
		);
		if (ImGui::Selectable(label.c_str(), false)) {
			RequestRichTextSelection(selection, end, begin, end);
			selection.focus_source = true;
			selection.keep_selection_highlight = true;
		}
		ImGui::PopStyleColor();
		DrawRichTextToolbarTooltip("Click to select the offending source range.");
		ImGui::PopID();
	}

	if (options.show_preview) {
		const std::string expanded{ ExpandRichTextVariables(
			source,
			[&options](std::string_view name) -> std::optional<std::string> {
				for (const auto& variable : options.variables) {
					if (variable.variable == name && !variable.preview.empty()) {
						return std::string{ variable.preview };
					}
				}
				return std::nullopt;
			}
		) };
		const auto parsed{ ParseRichText(expanded, defaults) };

		DrawRichTextPreviewSeparator();

		// Preview height is persistent and independently resizable. Use the same maximum
		// height as the source editor so either panel can grow to roughly 80% of the window.
		constexpr float minimum_preview_height{ 120.0f };
		float& preview_height{
			detached ? state.window_preview_height : state.inline_preview_height
		};
		if (preview_height <= 0.0f) {
			preview_height = detached
				? std::max(minimum_preview_height, ImGui::GetContentRegionAvail().y)
				: minimum_preview_height;
		}
		preview_height = std::clamp(
			preview_height, minimum_preview_height, maximum_editor_height
		);

		ImGui::SetNextWindowSizeConstraints(
			ImVec2{ 0.0f, minimum_preview_height },
			ImVec2{ FLT_MAX, maximum_editor_height }
		);

		TextBox preview_measure_box{
			options.preview_box ? *options.preview_box : TextBox{}
		};
		const TextLayout preview_measure_layout{
			::ptgn::impl::BuildTextLayout(
				ctx.editor.GetAssetManager(),
				parsed.text,
				preview_measure_box
			)
		};
		constexpr float preview_padding{ 32.0f };
		const float preview_available_width{
			std::max(1.0f, ImGui::GetContentRegionAvail().x)
		};
		const float preview_available_height{
			std::max(
				1.0f,
				preview_height - ImGui::GetStyle().WindowPadding.y * 2.0f
			)
		};
		const bool preview_horizontal_overflow{
			preview_measure_layout.size.x + preview_padding >
				preview_available_width
		};
		const bool preview_vertical_overflow{
			preview_measure_layout.size.y + preview_padding >
				preview_available_height
		};
		ImGuiWindowFlags preview_flags{ ImGuiWindowFlags_None };
		if (!preview_horizontal_overflow && !preview_vertical_overflow) {
			preview_flags |=
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoScrollWithMouse;
		} else if (preview_horizontal_overflow) {
			preview_flags |= ImGuiWindowFlags_HorizontalScrollbar;
		}
		ImGui::BeginChild(
			"##RichTextPreview", ImVec2{ -FLT_MIN, preview_height },
			ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY,
			preview_flags
		);
		DrawRichTextPreviewImpl(ctx, parsed.text, options.preview_box);
		ImGui::EndChild();

		const ImVec2 preview_min{ ImGui::GetItemRectMin() };
		const ImVec2 preview_max{ ImGui::GetItemRectMax() };
		preview_height = std::clamp(
			preview_max.y - preview_min.y,
			minimum_preview_height,
			maximum_editor_height
		);
	}

	return changed;
}


} // namespace

void DrawRichTextPreview(
	EditorContext& ctx, const StyledText& styled_text, const TextBox* preview_box
) {
	DrawRichTextPreviewImpl(ctx, styled_text, preview_box);
}

bool DrawRichTextEditor(
	EditorContext& ctx, std::string& source, TextRunDefaults& defaults,
	const RichTextEditorOptions& options
) {
	ImGui::PushID("RichTextEditor");
	const ImGuiID state_id{ ImGui::GetID("##State") };
	static std::unordered_map<ImGuiID, RichTextEditorState> states;
	static int last_prune_frame{ 0 };
	const int current_frame{ ImGui::GetFrameCount() };

	// ImGui IDs are ephemeral as inspector targets appear/disappear. Keep the convenience
	// of per-widget local state without retaining abandoned editor instances forever.
	if (current_frame - last_prune_frame >= 300) {
		for (auto it{ states.begin() }; it != states.end();) {
			if (current_frame - it->second.last_seen_frame > 900) {
				it = states.erase(it);
			} else {
				++it;
			}
		}
		last_prune_frame = current_frame;
	}

	auto& state{ states[state_id] };
	state.last_seen_frame = current_frame;

	auto reset_selection_to_end = [&](RichTextSelectionState& selection) {
		selection.cursor = state.source.size();
		selection.selection_start = state.source.size();
		selection.selection_end = state.source.size();
		selection.apply_selection = false;
		selection.focus_source = false;
		selection.keep_selection_highlight = false;
		selection.drag_selection_anchor.reset();
		selection.drag_word_selection.reset();
		selection.pending_action.reset();
	};

	if (!state.initialized) {
		state.initialized = true;
		state.source = source;
		reset_selection_to_end(state.inline_selection);
		state.window_selection = state.inline_selection;
		ResetRichTextSourceHistory(
			state,
			state.source,
			state.inline_selection
		);
		SetRichTextEditorColor(state.color, defaults.style.color);
		state.font = defaults.font.value;
		state.size = defaults.style.size;
	} else {
		// StyledText/TextData callers reconstruct a canonical source string every frame.
		// Keep the editor's authored source (including empty tags such as <b></b>) when
		// the caller merely hands that canonical representation back. Only replace the
		// authored source when the underlying value genuinely changed externally.
		const std::string canonical_source{ SerializeStyledTextToRichText(
			ParseRichText(state.source, defaults).text, defaults
		) };
		if (source != state.source && source != canonical_source) {
			state.source = source;
			reset_selection_to_end(state.inline_selection);
			reset_selection_to_end(state.window_selection);
			ResetRichTextSourceHistory(
				state,
				state.source,
				state.inline_selection
			);
		}
	}

	ClampRichTextSelection(state.inline_selection, state.source.size());
	ClampRichTextSelection(state.window_selection, state.source.size());

	// Formatting controls are overrides only. Keep a defensive snapshot so no
	// toolbar action can change Defaults; only edits made inside the explicit
	// Defaults tree are allowed to persist.
	const TextRunDefaults defaults_before{ defaults };
	const std::string source_before_draw{ state.source };
	bool defaults_changed{ false };

	bool changed{ DrawRichTextEditorPanel(
		ctx, state.source, defaults, options, state, state.inline_selection, false,
		defaults_changed
	) };

	if (state.window_open) {
		bool open{ true };
		const std::string title{
			"Rich Text Editor###RichTextEditorWindow_" + std::to_string(state_id)
		};
		const ImGuiViewport* viewport{ ImGui::GetWindowViewport() };
		if (viewport) {
			const ImVec2 window_size{
				std::max(600.0f, viewport->WorkSize.x * 0.8f),
				std::max(420.0f, viewport->WorkSize.y * 0.8f),
			};
			const ImVec2 center{
				viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
				viewport->WorkPos.y + viewport->WorkSize.y * 0.5f,
			};
			ImGui::SetNextWindowSize(window_size, ImGuiCond_Appearing);
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });
		}
		ImGui::SetNextWindowSizeConstraints(
			ImVec2{ 600.0f, 420.0f }, ImVec2{ FLT_MAX, FLT_MAX }
		);
		if (ImGui::Begin(title.c_str(), &open)) {
			ImGui::PushID(static_cast<int>(state_id));
			changed |= DrawRichTextEditorPanel(
				ctx, state.source, defaults, options, state, state.window_selection, true,
				defaults_changed
			);
			ImGui::PopID();
		}
		ImGui::End();
		state.window_open = open;
	}

	if (!defaults_changed) {
		defaults = defaults_before;
	}

	// Always expose the exact authored source to the caller. A resolved StyledText caller
	// may discard empty tags in its runtime representation, but the editor keeps them in
	// state.source so typing between them on the next frame still produces tagged text.
	source = state.source;

	if (options.selection) {
		const auto& current_selection{
			state.window_open ? state.window_selection : state.inline_selection
		};
		options.selection->cursor = current_selection.cursor;
		options.selection->selection_start = current_selection.selection_start;
		options.selection->selection_end = current_selection.selection_end;
	}

	ImGui::PopID();
	return changed || state.source != source_before_draw;
}

} // namespace ptgn::editor::inspector
