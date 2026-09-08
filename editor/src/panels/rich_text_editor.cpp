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

namespace ptgn::editor::inspector {

namespace {

enum class RichTextPendingActionType : std::uint8_t {
	ToggleTag,
	SetTag,
	RemoveTag,
	RemoveEffects,
	InsertToken,
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

	std::vector<RichTextSourceHistoryEntry> source_history{};
	std::size_t source_history_index{ 0 };
	bool source_history_applied{ false };

	// Editor-only display preference. This never modifies the authored rich text source.
	bool word_wrap{ false };

	std::string selection_warning{};
	double selection_warning_until{ 0.0 };
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
		const std::string_view name{ token.substr(0, equals) };
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

bool ToggleRichTextSelectionTag(
	std::string& source, RichTextSelectionState& state, std::string_view tag,
	std::string_view default_open, std::string_view close
) {
	ClampRichTextSelection(state, source.size());

	const std::size_t begin{ std::min(state.selection_start, state.selection_end) };
	const std::size_t end{ std::max(state.selection_start, state.selection_end) };

	// Search the complete contiguous wrapper stack, not just the innermost tag.
	// The matcher also accepts a selection that includes the matching tags.
	if (const auto wrapper{ FindRichTextSurroundingTag(source, begin, end, tag) }) {
		return RemoveRichTextLocatedTag(source, state, *wrapper, begin, end);
	}

	return WrapRichTextSelection(source, state, default_open, close);
}

// Applies a parameterized rich text tag without nesting the same tag repeatedly.
// This is used by Color/Font/Size and effect controls so changing a value while
// their popup remains open simply replaces the existing opening tag.
bool RemoveRichTextSelectionTag(
	std::string& source, RichTextSelectionState& state, std::string_view tag
) {
	ClampRichTextSelection(state, source.size());

	const std::size_t begin{ std::min(state.selection_start, state.selection_end) };
	const std::size_t end{ std::max(state.selection_start, state.selection_end) };
	const auto wrapper{ FindRichTextSurroundingTag(source, begin, end, tag) };
	if (!wrapper) {
		return false;
	}

	return RemoveRichTextLocatedTag(source, state, *wrapper, begin, end);
}

bool RemoveRichTextSelectionEffects(
	std::string& source, RichTextSelectionState& state
) {
	static constexpr std::array<std::string_view, 5> kEffectTags{
		"outline", "shadow", "outerglow", "innerglow", "fx",
	};

	bool changed{ false };
	while (true) {
		bool removed{ false };
		for (const auto tag : kEffectTags) {
			if (RemoveRichTextSelectionTag(source, state, tag)) {
				changed = true;
				removed = true;
				break;
			}
		}

		if (!removed) {
			break;
		}
	}

	return changed;
}

bool SetRichTextSelectionTag(
	std::string& source, RichTextSelectionState& state, std::string_view tag,
	std::string_view open, std::string_view close
) {
	ClampRichTextSelection(state, source.size());

	std::size_t begin{ std::min(state.selection_start, state.selection_end) };
	std::size_t end{ std::max(state.selection_start, state.selection_end) };
	if (begin == end) {
		begin = state.cursor;
		end = state.cursor;
	}

	if (const auto wrapper{ FindRichTextSurroundingTag(source, begin, end, tag) }) {
		const std::string_view current_open{
			source.data() + wrapper->open_begin, wrapper->open_size
		};
		if (current_open == open) {
			return false;
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
		return true;
	}

	return WrapRichTextSelection(source, state, open, close);
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

struct RichTextInputCallbackContext {
	RichTextSelectionState* selection{ nullptr };
	RichTextEditorState* editor_state{ nullptr };
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

		bool changed{ false };
		switch (action.type) {
			case RichTextPendingActionType::ToggleTag:
				changed = ToggleRichTextSelectionTag(
					edited_source, action_selection, action.tag, action.open, action.close
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
	std::string& source, RichTextSelectionState& state
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

	bool changed{ false };
	switch (action.type) {
		case RichTextPendingActionType::ToggleTag:
			changed = ToggleRichTextSelectionTag(
				source, action_selection, action.tag, action.open, action.close
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

[[nodiscard]] Color GetRichTextSelectionColor(
	std::string_view source, const RichTextSelectionState& selection,
	const TextRunDefaults& defaults
) {
	std::size_t begin{ std::min(selection.selection_start, selection.selection_end) };
	std::size_t end{ std::max(selection.selection_start, selection.selection_end) };
	begin = std::min(begin, source.size());
	end = std::min(end, source.size());
	if (begin == end) {
		begin = std::min(selection.cursor, source.size());
		end = begin;
	}

	const auto wrapper{ FindRichTextSurroundingTag(source, begin, end, "c") };
	if (!wrapper.has_value()) {
		return defaults.style.color;
	}

	std::string sample{ source.substr(wrapper->open_begin, wrapper->open_size) };
	sample += "x</c>";
	const auto parsed{ ParseRichText(sample, defaults) };
	if (!parsed.diagnostics.empty() || parsed.text.runs.empty()) {
		return defaults.style.color;
	}

	return parsed.text.runs.front().style.color;
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

struct RichTextSourceVisualLine {
	std::size_t begin{ 0 };
	std::size_t end{ 0 };
};

[[nodiscard]] ImU32 RichTextSourceColor(Color color) {
	// Ordinary source text follows the authored text color, including its alpha. Markup itself is
	// handled separately and is always fully opaque for editor readability.
	return IM_COL32(color.r, color.g, color.b, color.a);
}

[[nodiscard]] bool IsRichTextDefaultColorBlack(Color color) {
	return color.r == 0 && color.g == 0 && color.b == 0;
}

[[nodiscard]] Color RichTextSourceTagColor(const TextRunDefaults& defaults) {
	return IsRichTextDefaultColorBlack(defaults.style.color)
		? Color{ 255, 255, 255, 255 }
		: Color{ 0, 0, 0, 255 };
}

[[nodiscard]] std::optional<Color> ParseRichTextSourceColor(
	std::string_view value, const TextRunDefaults& defaults
) {
	if (value.empty()) {
		return std::nullopt;
	}

	std::string sample{ "<c=" };
	sample.append(value);
	sample += ">x</c>";

	const auto parsed{ ParseRichText(sample, defaults) };
	if (!parsed.diagnostics.empty() || parsed.text.runs.empty()) {
		return std::nullopt;
	}

	return parsed.text.runs.front().style.color;
}

[[nodiscard]] std::vector<ImU32> BuildRichTextSourceColors(
	std::string_view source, const TextRunDefaults& defaults
) {
	const ImU32 default_color{ RichTextSourceColor(defaults.style.color) };
	const Color source_tag_color{ RichTextSourceTagColor(defaults) };
	const ImU32 tag_color{
		IM_COL32(source_tag_color.r, source_tag_color.g, source_tag_color.b, 255)
	};
	std::vector<ImU32> colors(source.size(), default_color);

	std::vector<ImU32> color_stack;
	ImU32 current_color{ default_color };

	for (std::size_t i{ 0 }; i < source.size();) {
		// Escaped markup is ordinary source text, not a tag.
		if (source[i] == '\\' && i + 1 < source.size() &&
			(source[i + 1] == '<' || source[i + 1] == '>' || source[i + 1] == '\\')) {
			colors[i] = current_color;
			colors[i + 1] = current_color;
			i += 2;
			continue;
		}

		if (source[i] != '<') {
			colors[i] = current_color;
			++i;
			continue;
		}

		const std::size_t close{ source.find('>', i + 1) };
		if (close == std::string_view::npos) {
			// Keep an unterminated partial tag visually distinct as markup.
			std::fill(colors.begin() + static_cast<std::ptrdiff_t>(i), colors.end(), tag_color);
			break;
		}

		std::fill(
			colors.begin() + static_cast<std::ptrdiff_t>(i),
			colors.begin() + static_cast<std::ptrdiff_t>(close + 1),
			tag_color
		);

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

		bool closing{ source[token_begin] == '/' };
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

		const bool color_tag{ RichTextTagNameMatches(name, "c") };
		if (!color_tag) {
			i = close + 1;
			continue;
		}

		if (closing) {
			if (!color_stack.empty()) {
				current_color = color_stack.back();
				color_stack.pop_back();
			}
			i = close + 1;
			continue;
		}

		if (equals == std::string_view::npos || equals >= token_end) {
			i = close + 1;
			continue;
		}

		std::size_t value_begin{ equals + 1 };
		std::size_t value_end{ token_end };
		while (value_begin < value_end &&
			std::isspace(static_cast<unsigned char>(source[value_begin])) != 0) {
			++value_begin;
		}
		while (value_end > value_begin &&
			std::isspace(static_cast<unsigned char>(source[value_end - 1])) != 0) {
			--value_end;
		}

		const std::string_view value{ source.substr(value_begin, value_end - value_begin) };
		if (const auto parsed_color{ ParseRichTextSourceColor(value, defaults) }) {
			const ImU32 value_color{ RichTextSourceColor(parsed_color.value()) };
			std::fill(
				colors.begin() + static_cast<std::ptrdiff_t>(value_begin),
				colors.begin() + static_cast<std::ptrdiff_t>(value_end),
				value_color
			);
			color_stack.push_back(current_color);
			current_color = value_color;
		}

		i = close + 1;
	}

	return colors;
}

[[nodiscard]] const char* RichTextWordWrapPosition(
	const char* begin, const char* end, float width
) {
#if IMGUI_VERSION_NUM >= 19230
	return ImGui::GetFont()->CalcWordWrapPosition(
		ImGui::GetFontSize(), begin, end, width
	);
#else
	(void)width;
	return end;
#endif
}

[[nodiscard]] std::vector<RichTextSourceVisualLine> BuildRichTextSourceVisualLines(
	std::string_view source, bool word_wrap, float wrap_width
) {
	std::vector<RichTextSourceVisualLine> lines;

	for (std::size_t paragraph_begin{ 0 }; paragraph_begin <= source.size();) {
		const std::size_t newline{ source.find('\n', paragraph_begin) };
		const std::size_t paragraph_end{
			newline == std::string_view::npos ? source.size() : newline
		};

		if (!word_wrap || paragraph_begin == paragraph_end) {
			lines.push_back({ paragraph_begin, paragraph_end });
		} else {
			std::size_t line_begin{ paragraph_begin };
			while (line_begin < paragraph_end) {
				const char* begin_ptr{ source.data() + line_begin };
				const char* end_ptr{ source.data() + paragraph_end };
				const char* wrap_ptr{ RichTextWordWrapPosition(
					begin_ptr, end_ptr, std::max(1.0f, wrap_width)
				) };

				std::size_t line_end{
					static_cast<std::size_t>(wrap_ptr - source.data())
				};
				line_end = std::clamp(line_end, line_begin + 1, paragraph_end);
				lines.push_back({ line_begin, line_end });

				line_begin = line_end;
				while (line_begin < paragraph_end &&
					(source[line_begin] == ' ' || source[line_begin] == '\t')) {
					++line_begin;
				}
			}
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

bool DrawRichTextSourceInput(
	std::string& source,
	RichTextEditorState& editor_state,
	RichTextSelectionState& selection,
	const TextRunDefaults& defaults,
	float editor_height
) {
	const bool word_wrap{ editor_state.word_wrap };
	const float source_width{ RichTextSourceWidth(source) };

	// Formatting and local history are applied from inside ImGui's input callback so the
	// active edit buffer and the std::string can never diverge. Reserve enough room for
	// both tag insertion and restoring the largest currently reachable undo state.
	std::size_t history_capacity{ source.size() };
	for (const auto& entry : editor_state.source_history) {
		history_capacity = std::max(history_capacity, entry.source.size());
	}
	source.reserve(history_capacity + 4096);

	ImGuiWindowFlags child_flags{ ImGuiWindowFlags_None };
	if (!word_wrap) {
		child_flags |= ImGuiWindowFlags_HorizontalScrollbar;
	}

	// Keep the source editor background fully opaque regardless of the surrounding editor theme.
	// The authored text itself may still be translucent when its Color alpha is below 255.
	ImVec4 opaque_frame_color{ ImGui::GetStyleColorVec4(ImGuiCol_FrameBg) };
	opaque_frame_color.w = 1.0f;
	// GetColorU32() also multiplies style colors by ImGuiStyle::Alpha, so force this editor
	// region to full UI opacity as well. Authored text alpha is applied explicitly by our syntax
	// draw colors and therefore remains unaffected.
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_ChildBg, opaque_frame_color);
	ImGui::BeginChild(
		"##RichTextSourceScroll", ImVec2{ -FLT_MIN, editor_height }, false, child_flags
	);
	ImGui::PopStyleColor();

	const float inner_width{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
	const float input_width{ word_wrap ? inner_width : std::max(inner_width, source_width) };
	const float text_width{
		std::max(1.0f, input_width - ImGui::GetStyle().FramePadding.x * 2.0f)
	};
	const auto visual_lines{ BuildRichTextSourceVisualLines(source, word_wrap, text_width) };
	const float content_height{
		static_cast<float>(visual_lines.size()) * ImGui::GetTextLineHeight() +
		ImGui::GetStyle().FramePadding.y * 2.0f + 4.0f
	};
	const float input_height{ std::max(editor_height, content_height) };

	if (selection.focus_source) {
		ImGui::SetKeyboardFocusHere();
		selection.focus_source = false;
	}

	RichTextInputCallbackContext callback_context{
		.selection = &selection,
		.editor_state = &editor_state,
	};

	editor_state.source_history_applied = false;

	constexpr ImGuiInputTextFlags kHistoryFlags{ ImGuiInputTextFlags_NoUndoRedo };
	ImGuiInputTextFlags input_flags{
		ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_AllowTabInput |
		kHistoryFlags | ImGuiInputTextFlags_NoHorizontalScroll
	};
#if IMGUI_VERSION_NUM >= 19230
	if (word_wrap) {
		input_flags |= ImGuiInputTextFlags_WordWrap;
	}
#endif

	// InputTextMultiline creates its own internal child window. The syntax-colored source is
	// drawn into our outer source child after InputTextMultiline returns, so that internal frame
	// must stay transparent or it will cover the syntax layer. The outer source child already
	// provides the fully opaque editor background.
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

	int pushed_source_colors{ 5 };
#if IMGUI_VERSION_NUM >= 19230
	// InputText uses its own cursor color in 1.92.3+, independently of ImGuiCol_Text. Keep
	// Dear ImGui's one authoritative caret fully opaque while the built-in glyphs are hidden.
	ImVec4 input_cursor_color{ ImGui::GetStyleColorVec4(ImGuiCol_InputTextCursor) };
	input_cursor_color.w = 1.0f;
	ImGui::PushStyleColor(ImGuiCol_InputTextCursor, input_cursor_color);
	++pushed_source_colors;
#endif

	const bool input_changed{ ImGui::InputTextMultiline(
		"##RichTextSource", &source, ImVec2{ input_width, input_height }, input_flags,
		&RichTextInputCallback, &callback_context
	) };
	const bool source_clicked{ ImGui::IsItemClicked(ImGuiMouseButton_Left) };
	const bool source_active{ ImGui::IsItemActive() };
	const ImVec2 source_min{ ImGui::GetItemRectMin() };
	const ImVec2 source_max{ ImGui::GetItemRectMax() };
	ImGui::PopStyleColor(pushed_source_colors);

	if (source_clicked) {
		selection.keep_selection_highlight = false;
	}

	bool changed{ input_changed || callback_context.action_applied };
	if (selection.pending_action.has_value() && !source_active) {
		changed |= ApplyPendingRichTextActionToInactiveSource(source, selection);
	}

	if (!editor_state.source_history_applied) {
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

	// The string may have changed in the callback, so rebuild the display rows/colors from the
	// authoritative value before drawing the syntax layer.
	const auto display_lines{ BuildRichTextSourceVisualLines(source, word_wrap, text_width) };
	const ImVec2 text_origin{
		source_min.x + ImGui::GetStyle().FramePadding.x,
		source_min.y + ImGui::GetStyle().FramePadding.y,
	};

	if (!source_active && selection.keep_selection_highlight) {
		DrawRichTextSourceSelectionHighlight(
			source, selection, display_lines, text_origin, source_min, source_max
		);
	}
	DrawRichTextSourceSyntax(
		source, defaults, display_lines, text_origin, source_min, source_max
	);

	// Do not draw a second editor-owned caret here. Dear ImGui already owns the active text
	// cursor and its blink/height behavior; drawing another caret caused the doubled cursor at
	// soft-wrap boundaries and the apparent height change while blinking.

	ImGui::EndChild();
	ImGui::PopStyleVar();
	return changed;
}

[[nodiscard]] std::size_t RichTextUtf8CharacterLength(unsigned char first_byte) {
	if ((first_byte & 0x80u) == 0u) return 1;
	if ((first_byte & 0xE0u) == 0xC0u) return 2;
	if ((first_byte & 0xF0u) == 0xE0u) return 3;
	if ((first_byte & 0xF8u) == 0xF0u) return 4;
	return 1;
}

[[nodiscard]] std::size_t RichTextCharacterPosition(
	std::string_view source, std::size_t byte_position
) {
	byte_position = std::min(byte_position, source.size());
	std::size_t character{ 1 };
	for (std::size_t i{ 0 }; i < byte_position;) {
		const auto length{ std::min(
			RichTextUtf8CharacterLength(static_cast<unsigned char>(source[i])),
			byte_position - i
		) };
		i += std::max<std::size_t>(1, length);
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

void DrawRichTextPreview(
	EditorContext& ctx,
	const StyledText& styled_text
) {
	constexpr float kPreviewPadding{ 16.0f };
	constexpr int kMaximumPreviewDimension{ 4096 };

	if (const auto missing_font{ RichTextPreviewMissingFont(ctx, styled_text) }) {
		ImGui::TextDisabled("%s", missing_font->c_str());
		return;
	}

	const TextBox box{};
	const TextLayout layout{
		::ptgn::impl::BuildTextLayout(
			ctx.editor.GetAssetManager(),
			styled_text,
			box
		)
	};

	const ImVec2 available{ ImGui::GetContentRegionAvail() };
	const float natural_width{
		std::max(1.0f, layout.size.x + kPreviewPadding * 2.0f)
	};
	const float natural_height{
		std::max(1.0f, layout.size.y + kPreviewPadding * 2.0f)
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
		-static_cast<float>(framebuffer_size.x) * 0.5f + kPreviewPadding,
		-static_cast<float>(framebuffer_size.y) * 0.5f + kPreviewPadding,
	};

	Transform anchor{};
	anchor.position = preview_origin;

	// Use the same origin adjustment as Text::Draw instead of treating the layout's
	// raw glyph coordinates as if they started at the preview's top-left corner.
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

void DrawRichTextToolbar(
	EditorContext& ctx, const TextRunDefaults& defaults,
	const RichTextEditorOptions& options, RichTextEditorState& state,
	RichTextSelectionState& selection, bool allow_detached_window
) {
	const float button_height{ ImGui::GetFrameHeight() };

	// Toolbar controls operate on the last real source range without forcing keyboard
	// focus back to the multiline input. This preserves the formatting target while
	// avoiding the active/focused input highlight and keeping popups/combos open.
	auto begin_text_action = [&]() { ctx.undo.CommitActiveEdit(); };
	auto preserve_source_selection = [&]() {
		// Keep the last real text range authoritative, but do not return keyboard focus
		// to the source input here. Doing so later in this frame steals focus from
		// Color/Font/Size popups and Effects/Variables combos, closing them immediately.
		selection.apply_selection = true;
		selection.keep_selection_highlight = true;
	};

	auto validate_selection = [&]() {
		if (!RichTextSelectionContainsPartialTag(state.source, selection)) {
			state.selection_warning.clear();
			state.selection_warning_until = 0.0;
			return true;
		}

		state.selection_warning =
			"Selection contains only part of a rich text tag. Select the complete tag or "
			"move the selection outside it.";
		state.selection_warning_until = ImGui::GetTime() + 4.0;
		return false;
	};

	auto queue_toggle_tag = [&](
		std::string_view tag, std::string_view open, std::string_view close
	) {
		if (!validate_selection()) {
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

		// BIUS is an immediate toolbar action rather than a popup interaction.
		// Reactivate the source so its selected range remains visibly highlighted,
		// while DrawRichTextSourceInput suppresses the blue focus border.
		selection.focus_source = true;
		return true;
	};

	auto queue_set_tag = [&](
		std::string_view tag, std::string open, std::string_view close
	) {
		if (!validate_selection()) {
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
		if (!validate_selection()) {
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
		if (!validate_selection()) {
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
		if (!validate_selection()) {
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

	auto tag_button = [&](const char* label, std::string_view tag, std::string_view open,
						  std::string_view close, std::string_view tooltip) {
		if (ImGui::Button(label, ImVec2{ 0.0f, button_height })) {
			if (validate_selection()) {
				begin_text_action();
				queue_toggle_tag(tag, open, close);
			}
		}
		DrawRichTextToolbarTooltip(tooltip);
	};

	tag_button("B", "b", "<b>", "</b>", "Bold.\n<b>...</b>\n<b=0.2>...</b>");
	ImGui::SameLine();
	tag_button("I", "i", "<i>", "</i>", "Italic.\n<i>...</i>");
	ImGui::SameLine();
	tag_button("U", "u", "<u>", "</u>", "Underline.\n<u>...</u>");
	ImGui::SameLine();
	tag_button("S", "s", "<s>", "</s>", "Strikethrough.\n<s>...</s>");

	ImGui::SameLine();
	std::array<float, 4> toolbar_color{};
	SetRichTextEditorColor(
		toolbar_color, GetRichTextSelectionColor(state.source, selection, defaults)
	);
	if (ImGui::ColorButton(
			"##RichTextColor",
			ImVec4{ toolbar_color[0], toolbar_color[1], toolbar_color[2], toolbar_color[3] },
			ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip,
			ImVec2{ button_height, button_height }
		)) {
		if (validate_selection()) {
			state.color = toolbar_color;
			preserve_source_selection();
			ImGui::OpenPopup("RichTextColorPopup");
		}
	}
	DrawRichTextToolbarTooltip(
		"Text color.\n<c=red>...</c>\n<c=#RRGGBB>...</c>\n<c=#RRGGBBAA>...</c>"
	);
	if (ImGui::BeginPopup("RichTextColorPopup")) {
		const bool color_changed{ ImGui::ColorPicker4(
			"##RichTextColorPicker", state.color.data(),
			ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf |
				ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB
		) };
		if (ImGui::IsItemActivated()) {
			begin_text_action();
		}
		if (color_changed) {
			queue_set_tag(
				"c", "<c=" + RichTextEditorColorTag(state.color) + ">", "</c>"
			);
		}

		if (ImGui::Button("Reset Color", ImVec2{ -FLT_MIN, 0.0f })) {
			begin_text_action();
			SetRichTextEditorColor(state.color, defaults.style.color);
			queue_remove_tag("c");
		}
		DrawRichTextToolbarTooltip("Remove the explicit color override and use the rich text default color.");

		ImGui::EndPopup();
	}

	ImGui::SameLine();
	if (ImGui::Button("Font", ImVec2{ 0.0f, button_height })) {
		if (validate_selection()) {
			preserve_source_selection();
			ImGui::OpenPopup("RichTextFontPopup");
		}
	}
	DrawRichTextToolbarTooltip("Font key.\n<font=key>...</font>");
	if (ImGui::BeginPopup("RichTextFontPopup")) {
		auto apply_font = [&]() {
			queue_set_tag("font", "<font=" + state.font + ">", "</font>");
		};

		const auto loaded_fonts{ GetLoadedRichTextFontKeys(ctx) };
		const bool custom_font_key{ !std::ranges::contains(loaded_fonts, state.font) };
		const char* preview{
			custom_font_key ? "Custom" : (state.font.empty() ? "Default" : state.font.c_str())
		};
		ImGui::SetNextItemWidth(260.0f);
		if (ImGui::BeginCombo("Loaded##RichTextFont", preview)) {
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

		if (ImGui::Button("Reset Font", ImVec2{ -FLT_MIN, 0.0f })) {
			begin_text_action();
			state.font = defaults.font.value;
			queue_remove_tag("font");
		}
		DrawRichTextToolbarTooltip("Remove the explicit font override and use the rich text default font.");

		ImGui::EndPopup();
	}

	ImGui::SameLine();
	if (ImGui::Button("Size", ImVec2{ 0.0f, button_height })) {
		if (validate_selection()) {
			preserve_source_selection();
			ImGui::OpenPopup("RichTextSizePopup");
		}
	}
	DrawRichTextToolbarTooltip("Font size.\n<size=32>...</size>");
	if (ImGui::BeginPopup("RichTextSizePopup")) {
		auto apply_size = [&]() {
			char value[64]{};
			std::snprintf(value, sizeof(value), "<size=%.3g>", static_cast<double>(state.size));
			queue_set_tag("size", value, "</size>");
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

		if (ImGui::Button("Reset Size", ImVec2{ -FLT_MIN, 0.0f })) {
			begin_text_action();
			state.size = defaults.style.size;
			queue_remove_tag("size");
		}
		DrawRichTextToolbarTooltip("Remove the explicit size override and use the rich text default size.");

		ImGui::EndPopup();
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(100.0f);
	const bool effects_open{ ImGui::BeginCombo("##RichTextEffects", "Effects") };
	if (ImGui::IsItemActivated()) {
		preserve_source_selection();
	}
	if (effects_open) {
		if (ImGui::Selectable("Reset Effects")) {
			begin_text_action();
			queue_remove_effects();
		}
		DrawRichTextToolbarTooltip(
			"Remove surrounding outline, shadow, glow, and glyph-effect override tags."
		);
		ImGui::Separator();

		auto effect_item = [&](const char* label, std::string_view tag, std::string_view open,
						   std::string_view close, std::string_view tooltip) {
			if (ImGui::Selectable(label)) {
				begin_text_action();
				queue_set_tag(tag, std::string{ open }, close);
			}
			DrawRichTextToolbarTooltip(tooltip);
		};

		effect_item(
			"Wave", "fx", "<fx=Wave,8,2,1,0>", "</fx>",
			"<fx=Wave,amplitude,frequency,speed,phase>"
		);
		effect_item(
			"Wobble", "fx", "<fx=Wobble,4,2,1,0>", "</fx>",
			"<fx=Wobble,amplitude,frequency,speed,phase>"
		);
		effect_item(
			"Shake", "fx", "<fx=Shake,3,20,1,0>", "</fx>",
			"<fx=Shake,amplitude,frequency,speed,phase>"
		);
		effect_item(
			"Pulse", "fx", "<fx=Pulse,0.15,2,1,0>", "</fx>",
			"<fx=Pulse,amplitude,frequency,speed,phase>"
		);
		ImGui::Separator();
		effect_item(
			"Outline", "outline", "<outline=#000000,2,1>", "</outline>",
			"<outline=color,width[,softness]>"
		);
		effect_item(
			"Shadow", "shadow", "<shadow=#00000080,3,3,0,1>", "</shadow>",
			"<shadow=color,x,y[,width[,softness]]>"
		);
		effect_item(
			"Outer Glow", "outerglow", "<outerglow=#00FFFF,4,1>", "</outerglow>",
			"<outerglow=color,width[,softness]>"
		);
		effect_item(
			"Inner Glow", "innerglow", "<innerglow=#FFFFFF,2,1>", "</innerglow>",
			"<innerglow=color,width[,softness]>"
		);
		ImGui::EndCombo();
	}
	DrawRichTextToolbarTooltip(
		"Effect tags.\n"
		"<fx=type[,amplitude[,frequency[,speed[,phase]]]]>\n"
		"<outline=color,width[,softness]>\n"
		"<shadow=color,x,y[,width[,softness]]>\n"
		"<outerglow=color,width[,softness]>\n"
		"<innerglow=color,width[,softness]>"
	);

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
			ImGui::TextColored(
				ImVec4{ 1.0f, 0.45f, 0.2f, 1.0f }, "%s",
				state.selection_warning.c_str()
			);
		} else {
			state.selection_warning.clear();
			state.selection_warning_until = 0.0;
		}
	}

	const float editor_height{
		detached
			? std::max(180.0f, ImGui::GetContentRegionAvail().y * 0.45f)
			: ImGui::GetTextLineHeightWithSpacing() *
				  static_cast<float>(std::max(options.line_count, 3)) +
				  ImGui::GetStyle().FramePadding.y * 2.0f
	};
	if (DrawRichTextSourceInput(source, state, selection, defaults, editor_height)) {
		changed = true;
	}


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
		const auto source_diagnostics{ ParseRichText(source, defaults).diagnostics };

		// Keep diagnostics above the preview so the preview can consume the remaining
		// height in the detached editor and finish exactly at the bottom of the window.
		for (const auto& diagnostic : source_diagnostics) {
			ImGui::TextColored(
				ImVec4{ 1.0f, 0.45f, 0.2f, 1.0f }, "Character %zu: %s",
				RichTextCharacterPosition(source, diagnostic.position), diagnostic.message.c_str()
			);
		}

		DrawRichTextPreviewSeparator();
		ImGui::BeginChild(
			"##RichTextPreview", ImVec2{ -FLT_MIN, detached ? 0.0f : 120.0f }, true,
			ImGuiWindowFlags_HorizontalScrollbar
		);
		DrawRichTextPreview(ctx, parsed.text);
		ImGui::EndChild();
	}

	return changed;
}


} // namespace

bool DrawRichTextEditor(
	EditorContext& ctx, std::string& source, TextRunDefaults& defaults,
	const RichTextEditorOptions& options
) {
	ImGui::PushID("RichTextEditor");
	const ImGuiID state_id{ ImGui::GetID("##State") };
	static std::unordered_map<ImGuiID, RichTextEditorState> states;
	auto& state{ states[state_id] };

	auto reset_selection_to_end = [&](RichTextSelectionState& selection) {
		selection.cursor = state.source.size();
		selection.selection_start = state.source.size();
		selection.selection_end = state.source.size();
		selection.apply_selection = false;
		selection.focus_source = false;
		selection.keep_selection_highlight = false;
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
		ImGui::SetNextWindowSize(ImVec2{ 900.0f, 700.0f }, ImGuiCond_FirstUseEver);
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

	ImGui::PopID();
	return changed || state.source != source_before_draw;
}

} // namespace ptgn::editor::inspector
