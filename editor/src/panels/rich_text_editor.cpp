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

struct RichTextEditorState {
	RichTextSelectionState inline_selection{};
	RichTextSelectionState window_selection{};
	bool initialized{ false };
	bool window_open{ false };
	bool window_recenter_requested{ false };
	std::string source{};
	std::array<float, 4> color{ 1.0f, 1.0f, 1.0f, 1.0f };
	std::string font{};
	float size{ kDefaultFontSize };
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

	// The selection may include the matching opening/closing tags themselves.
	// Treat that exactly like selecting only the inner text so toggling removes
	// the existing wrapper rather than nesting another one.
	if (const auto selected{
			FindRichTextSelectedTag(source, selection_begin, selection_end, tag)
		}) {
		return selected;
	}

	std::size_t open_cursor{ selection_begin };
	while (open_cursor > 0 && source[open_cursor - 1] == '>') {
		const std::size_t open_begin{ source.rfind('<', open_cursor - 1) };
		if (open_begin == std::string_view::npos) {
			break;
		}

		std::string_view token{
			source.substr(open_begin + 1, open_cursor - open_begin - 2)
		};
		if (token.empty() || token.starts_with('/')) {
			break;
		}

		const auto equals{ token.find('=') };
		const std::string_view name{ token.substr(0, equals) };
		if (RichTextTagNameMatches(name, tag)) {
			std::size_t close_cursor{ selection_end };
			while (close_cursor < source.size() && source[close_cursor] == '<') {
				const std::size_t close_end{ source.find('>', close_cursor + 1) };
				if (close_end == std::string_view::npos) {
					break;
				}

				std::string_view close_token{
					source.substr(close_cursor + 1, close_end - close_cursor - 1)
				};
				if (!close_token.starts_with('/')) {
					break;
				}
				close_token.remove_prefix(1);
				if (RichTextTagNameMatches(close_token, tag)) {
					return RichTextSurroundingTag{
						.open_begin = open_begin,
						.open_size = open_cursor - open_begin,
						.close_begin = close_cursor,
						.close_size = close_end - close_cursor + 1,
					};
				}

				close_cursor = close_end + 1;
			}
			return std::nullopt;
		}

		open_cursor = open_begin;
	}

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

// Applies a parameterized rich-text tag without nesting the same tag repeatedly.
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

[[nodiscard]] bool RichTextEditorColorEquals(const std::array<float, 4>& value, Color color) {
	auto byte = [](float component) {
		return static_cast<std::uint8_t>(
			std::clamp(std::lround(component * 255.0f), 0l, 255l)
		);
	};
	return byte(value[0]) == color.r && byte(value[1]) == color.g &&
		   byte(value[2]) == color.b && byte(value[3]) == color.a;
}

[[nodiscard]] bool RichTextEditorFloatEquals(float lhs, float rhs) {
	return std::abs(lhs - rhs) <= 0.0001f;
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

void DrawRichTextInactiveSelectionHighlight(
	std::string_view source, const RichTextSelectionState& selection, ImVec2 item_min,
	ImVec2 item_max
) {
	std::size_t begin{ std::min(selection.selection_start, selection.selection_end) };
	std::size_t end{ std::max(selection.selection_start, selection.selection_end) };
	begin = std::min(begin, source.size());
	end = std::min(end, source.size());
	if (begin >= end) {
		return;
	}

	ImDrawList* draw_list{ ImGui::GetWindowDrawList() };
	const ImGuiStyle& style{ ImGui::GetStyle() };
	const float line_height{ ImGui::GetTextLineHeight() };
	const ImU32 selection_color{ ImGui::GetColorU32(ImGuiCol_TextSelectedBg) };
	const ImU32 text_color{ ImGui::GetColorU32(ImGuiCol_Text) };
	const float content_left{ item_min.x + style.FramePadding.x };
	float y{ item_min.y + style.FramePadding.y };

	draw_list->PushClipRect(item_min, item_max, true);

	std::size_t line_begin{ 0 };
	while (line_begin <= source.size()) {
		const std::size_t newline{ source.find('\n', line_begin) };
		const std::size_t line_end{
			newline == std::string_view::npos ? source.size() : newline
		};

		const std::size_t selected_begin{ std::max(begin, line_begin) };
		const std::size_t selected_end{ std::min(end, line_end) };
		const bool newline_selected{
			newline != std::string_view::npos && begin <= newline && end > newline
		};

		if (selected_begin < selected_end || newline_selected) {
			const float x0{
				content_left +
				ImGui::CalcTextSize(
					source.data() + line_begin, source.data() + selected_begin, false
				).x
			};
			float x1{
				content_left +
				ImGui::CalcTextSize(
					source.data() + line_begin, source.data() + selected_end, false
				).x
			};
			if (newline_selected) {
				x1 = std::max(x1, item_max.x - style.FramePadding.x);
			}

			draw_list->AddRectFilled(
				ImVec2{ x0, y }, ImVec2{ std::max(x1, x0 + 1.0f), y + line_height },
				selection_color
			);

			if (selected_begin < selected_end) {
				draw_list->AddText(
					ImVec2{ x0, y }, text_color, source.data() + selected_begin,
					source.data() + selected_end
				);
			}
		}

		if (newline == std::string_view::npos) {
			break;
		}

		line_begin = newline + 1;
		y += line_height;
		if (y > item_max.y) {
			break;
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
	std::string& source, RichTextSelectionState& selection, float editor_height
) {
	const float available_width{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
	const float source_width{ RichTextSourceWidth(source) };
	const float input_width{ std::max(available_width, source_width) };

	// Formatting is applied from inside ImGui's input callback so the active edit buffer
	// and the std::string can never diverge. Give callback-side tag insertion enough room
	// without forcing the stdlib wrapper to resize during the formatter operation.
	source.reserve(source.size() + 4096);

	auto draw_input = [&](float width) {
		if (selection.focus_source) {
			ImGui::SetKeyboardFocusHere();
			selection.focus_source = false;
		}

		RichTextInputCallbackContext callback_context{ .selection = &selection };

		// The rich-text source should never gain a special blue active/nav frame.
		ImGui::PushStyleColor(
			ImGuiCol_FrameBgActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg)
		);
#if IMGUI_VERSION_NUM >= 19104
		ImGui::PushStyleColor(
			ImGuiCol_NavCursor, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f }
		);
#else
		ImGui::PushStyleColor(
			ImGuiCol_NavHighlight, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f }
		);
#endif

		const bool input_changed{ ImGui::InputTextMultiline(
			"##RichTextSource", &source, ImVec2{ width, editor_height },
			ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_AllowTabInput |
				ImGuiInputTextFlags_NoHorizontalScroll,
			&RichTextInputCallback, &callback_context
		) };

		const bool source_clicked{ ImGui::IsItemClicked(ImGuiMouseButton_Left) };
		ImGui::PopStyleColor(2);
		if (source_clicked) {
			selection.keep_selection_highlight = false;
		}

		const bool source_active{ ImGui::IsItemActive() };
		const ImVec2 source_min{ ImGui::GetItemRectMin() };
		const ImVec2 source_max{ ImGui::GetItemRectMax() };

		bool changed{ input_changed || callback_context.action_applied };
		if (selection.pending_action.has_value() && !source_active) {
			changed |= ApplyPendingRichTextActionToInactiveSource(source, selection);
		}

		if (!source_active && selection.keep_selection_highlight) {
			DrawRichTextInactiveSelectionHighlight(
				source, selection, source_min, source_max
			);
		}

		return changed;
	};

	// The child is intentionally present before and after overflow. Previously it was created
	// only when the source became wider than the editor, which changed the active InputText's
	// parent window/ID context and dropped keyboard focus the moment the scrollbar appeared.
	const float child_height{
		editor_height + ImGui::GetStyle().ScrollbarSize +
		ImGui::GetStyle().FramePadding.y * 2.0f + 2.0f
	};
	ImGui::BeginChild(
		"##RichTextSourceHorizontalScroll", ImVec2{ -FLT_MIN, child_height }, false,
		ImGuiWindowFlags_HorizontalScrollbar
	);
	const bool changed{ draw_input(input_width) };
	ImGui::EndChild();
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

[[nodiscard]] ImU32 RichTextPreviewColor(Color color, float alpha_scale = 1.0f) {
	const auto alpha{ static_cast<std::uint8_t>(std::clamp(
		std::lround(static_cast<float>(color.a) * alpha_scale), 0l, 255l
	)) };
	return IM_COL32(color.r, color.g, color.b, alpha);
}

void DrawRichTextPreviewGlyphText(
	ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 position, ImU32 color,
	std::string_view glyph, bool italic
) {
	const int vertex_begin{ draw_list->VtxBuffer.Size };
	draw_list->AddText(
		font, font_size, position, color, glyph.data(), glyph.data() + glyph.size()
	);

	if (!italic) {
		return;
	}

	const float bottom{ position.y + font_size };
	for (int i{ vertex_begin }; i < draw_list->VtxBuffer.Size; ++i) {
		auto& vertex{ draw_list->VtxBuffer[i] };
		vertex.pos.x += (bottom - vertex.pos.y) * 0.18f;
	}
}

void DrawRichTextPreviewLayer(
	ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 position, ImU32 color,
	std::string_view glyph, bool italic, float radius, float softness
) {
	const float clamped_radius{ std::clamp(radius, 0.0f, 10.0f) };
	if (clamped_radius <= 0.01f) {
		DrawRichTextPreviewGlyphText(draw_list, font, font_size, position, color, glyph, italic);
		return;
	}

	const int rings{ std::clamp(static_cast<int>(std::ceil(clamped_radius + softness)), 1, 8) };
	for (int ring{ rings }; ring >= 1; --ring) {
		const float t{ static_cast<float>(ring) / static_cast<float>(rings) };
		const float distance{ clamped_radius * t };
		const float alpha{ 0.18f + 0.52f * (1.0f - t) };
		const ImU32 ring_color{
			(color & 0x00FFFFFFu) |
			(static_cast<ImU32>(std::clamp(alpha * 255.0f, 0.0f, 255.0f)) << IM_COL32_A_SHIFT)
		};

		for (int direction{ 0 }; direction < 8; ++direction) {
			const float angle{ static_cast<float>(direction) * 0.78539816339f };
			DrawRichTextPreviewGlyphText(
				draw_list, font, font_size,
				ImVec2{
					position.x + std::cos(angle) * distance,
					position.y + std::sin(angle) * distance,
				},
				ring_color, glyph, italic
			);
		}
	}
}

struct RichTextPreviewGlyphEffect {
	ImVec2 offset{};
	float scale{ 1.0f };
};

[[nodiscard]] RichTextPreviewGlyphEffect GetRichTextPreviewGlyphEffect(
	const GlyphEffectStyle& effect, std::size_t glyph_index, double time
) {
	constexpr float kGlyphEffectPhaseStep{ 0.35f };

	RichTextPreviewGlyphEffect result;
	const float order{ static_cast<float>(glyph_index) };
	const float phase{ effect.phase + order * kGlyphEffectPhaseStep };
	const float t{ static_cast<float>(time) * effect.speed + phase };

	switch (effect.type) {
		case GlyphEffectType::None:
			break;

		case GlyphEffectType::Wave:
			result.offset.y = std::sin(t * effect.frequency) * effect.amplitude;
			break;

		case GlyphEffectType::Wobble:
			result.offset = ImVec2{
				std::sin(t * effect.frequency) * effect.amplitude,
				std::cos(t * effect.frequency * 1.37f) * effect.amplitude,
			};
			break;

		case GlyphEffectType::Shake:
			result.offset = ImVec2{
				std::sin(t * effect.frequency * 17.0f + order * 12.9898f) * effect.amplitude,
				std::cos(t * effect.frequency * 23.0f + order * 78.233f) * effect.amplitude,
			};
			break;

		case GlyphEffectType::Pulse:
			result.scale = std::max(0.1f, 1.0f + std::sin(t) * effect.amplitude);
			break;
	}

	return result;
}

void DrawRichTextPreviewGlyph(
	ImDrawList* draw_list, ImFont* font, const TextRunStyle& style, std::string_view glyph,
	ImVec2 position, float font_size, std::size_t glyph_index, double time
) {
	const auto effect{ GetRichTextPreviewGlyphEffect(style.effect, glyph_index, time) };
	const float draw_size{ std::max(1.0f, font_size * effect.scale) };
	position.x += effect.offset.x;
	position.y += effect.offset.y + (font_size - draw_size) * 0.5f;

	const bool italic{ HasFontFlag(style.flags, FontStyle::Italic) };
	const bool bold{ HasFontFlag(style.flags, FontStyle::Bold) };
	const auto& sdf{ style.sdf };

	if (sdf.outer_glow.color.a > 0 && sdf.outer_glow.width > 0.0f) {
		DrawRichTextPreviewLayer(
			draw_list, font, draw_size, position,
			RichTextPreviewColor(sdf.outer_glow.color, 0.55f), glyph, italic,
			sdf.outer_glow.width, sdf.outer_glow.softness
		);
	}

	if (sdf.shadow.color.a > 0) {
		const ImVec2 shadow_position{
			position.x + sdf.shadow_offset.x,
			position.y + sdf.shadow_offset.y,
		};
		DrawRichTextPreviewLayer(
			draw_list, font, draw_size, shadow_position,
			RichTextPreviewColor(sdf.shadow.color, 0.85f), glyph, italic,
			sdf.shadow.width, sdf.shadow.softness
		);
	}

	if (sdf.outline.color.a > 0 && sdf.outline.width > 0.0f) {
		DrawRichTextPreviewLayer(
			draw_list, font, draw_size, position,
			RichTextPreviewColor(sdf.outline.color), glyph, italic,
			sdf.outline.width, sdf.outline.softness
		);
	}

	const ImU32 text_color{ RichTextPreviewColor(style.color) };
	DrawRichTextPreviewGlyphText(
		draw_list, font, draw_size, position, text_color, glyph, italic
	);

	if (bold) {
		const float weight{ std::clamp(style.bold_weight * draw_size, 0.0f, 4.0f) };
		if (weight > 0.01f) {
			DrawRichTextPreviewGlyphText(
				draw_list, font, draw_size, ImVec2{ position.x + weight, position.y },
				text_color, glyph, italic
			);
		}
	}

	if (sdf.inner_glow.color.a > 0 && sdf.inner_glow.width > 0.0f) {
		const float strength{
			std::clamp(sdf.inner_glow.width / (sdf.inner_glow.width + sdf.inner_glow.softness + 1.0f),
				0.08f, 0.45f)
		};
		DrawRichTextPreviewGlyphText(
			draw_list, font, draw_size, position,
			RichTextPreviewColor(sdf.inner_glow.color, strength), glyph, italic
		);
	}
}

void DrawRichTextPreview(const StyledText& styled_text) {
	constexpr float padding{ 8.0f };
	ImDrawList* draw_list{ ImGui::GetWindowDrawList() };
	ImFont* font{ ImGui::GetFont() };
	const ImVec2 origin{ ImGui::GetCursorScreenPos() };
	const double time{ ImGui::GetTime() };

	float x{ padding };
	float y{ padding };
	float line_height{ ImGui::GetTextLineHeight() };
	float line_spacing{ 0.0f };
	float max_width{ padding * 2.0f };
	std::size_t glyph_index{ 0 };
	bool drew_anything{ false };

	for (const auto& run : styled_text.runs) {
		const float font_size{ std::max(1.0f, run.style.size) };
		const float tracking{ run.style.tracking * font_size };
		std::string_view remaining{ run.text };

		for (std::size_t i{ 0 }; i < remaining.size();) {
			if (remaining[i] == '\n') {
				max_width = std::max(max_width, x + padding);
				x = padding;
				y += std::max(line_height, font_size) + line_spacing;
				line_height = font_size;
				line_spacing = run.style.line_spacing;
				++i;
				continue;
			}

			const std::size_t length{ std::min(
				RichTextUtf8CharacterLength(static_cast<unsigned char>(remaining[i])),
				remaining.size() - i
			) };
			const std::string_view glyph{ remaining.substr(i, std::max<std::size_t>(1, length)) };
			const float glyph_width{
				font->CalcTextSizeA(
					font_size, FLT_MAX, 0.0f, glyph.data(), glyph.data() + glyph.size()
				).x
			};

			const auto effect{ GetRichTextPreviewGlyphEffect(run.style.effect, glyph_index, time) };
			const ImVec2 glyph_position{
				origin.x + x,
				origin.y + y,
			};
			DrawRichTextPreviewGlyph(
				draw_list, font, run.style, glyph, glyph_position, font_size, glyph_index, time
			);

			const float advance{ glyph_width + tracking };
			const ImU32 decoration_color{ RichTextPreviewColor(run.style.color) };
			if (HasFontFlag(run.style.flags, FontStyle::Underline)) {
				const float underline_y{
					glyph_position.y + effect.offset.y + font_size * 0.92f
				};
				draw_list->AddLine(
					ImVec2{ glyph_position.x + effect.offset.x, underline_y },
					ImVec2{ glyph_position.x + effect.offset.x + advance, underline_y },
					decoration_color, std::max(1.0f, font_size * 0.055f)
				);
			}
			if (HasFontFlag(run.style.flags, FontStyle::Strikethrough)) {
				const float strike_y{
					glyph_position.y + effect.offset.y + font_size * 0.52f
				};
				draw_list->AddLine(
					ImVec2{ glyph_position.x + effect.offset.x, strike_y },
					ImVec2{ glyph_position.x + effect.offset.x + advance, strike_y },
					decoration_color, std::max(1.0f, font_size * 0.05f)
				);
			}

			x += advance;
			line_height = std::max(line_height, font_size * effect.scale);
			line_spacing = std::max(line_spacing, run.style.line_spacing);
			max_width = std::max(max_width, x + padding);
			i += std::max<std::size_t>(1, length);
			++glyph_index;
			drew_anything = true;
		}
	}

	if (!drew_anything) {
		ImGui::TextDisabled("(empty)");
		return;
	}

	const float content_height{ y + line_height + padding * 2.0f };
	ImGui::Dummy(ImVec2{ std::max(max_width, ImGui::GetContentRegionAvail().x), content_height });
}

void DrawRichTextToolbarTooltip(std::string_view text) {
	if (!ImGui::IsItemHovered()) {
		return;
	}

	const ImVec2 item_min{ ImGui::GetItemRectMin() };
	const ImVec2 item_max{ ImGui::GetItemRectMax() };
	const ImVec2 position{
		(item_min.x + item_max.x) * 0.5f,
		item_min.y - ImGui::GetStyle().ItemSpacing.y,
	};

	// BeginTooltip() positions from the mouse. A dedicated tooltip window lets us anchor
	// bottom-center above the hovered control, so toolbar/combo help never covers the source.
	ImGui::SetNextWindowPos(position, ImGuiCond_Always, ImVec2{ 0.5f, 1.0f });
	ImGui::Begin(
		"##RichTextToolbarTooltip", nullptr,
		ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
	);
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
	ImGui::End();
}

void DrawRichTextToolbar(
	EditorContext& ctx, const TextRunDefaults& defaults,
	const RichTextEditorOptions& options, RichTextEditorState& state,
	RichTextSelectionState& selection, bool allow_detached_window
) {
	const float button_height{ ImGui::GetFrameHeight() };
	const TextRunDefaults engine_defaults{};

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

	auto queue_toggle_tag = [&](
		std::string_view tag, std::string_view open, std::string_view close
	) {
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
	};

	auto queue_set_tag = [&](
		std::string_view tag, std::string open, std::string_view close
	) {
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
	};

	auto queue_remove_tag = [&](std::string_view tag) {
		selection.pending_action = RichTextPendingAction{
			.type = RichTextPendingActionType::RemoveTag,
			.cursor = selection.cursor,
			.selection_start = selection.selection_start,
			.selection_end = selection.selection_end,
			.tag = std::string{ tag },
		};
		selection.apply_selection = true;
		selection.keep_selection_highlight = true;
	};

	auto queue_remove_effects = [&]() {
		selection.pending_action = RichTextPendingAction{
			.type = RichTextPendingActionType::RemoveEffects,
			.cursor = selection.cursor,
			.selection_start = selection.selection_start,
			.selection_end = selection.selection_end,
		};
		selection.apply_selection = true;
		selection.keep_selection_highlight = true;
	};

	auto queue_insert_token = [&](std::string token) {
		selection.pending_action = RichTextPendingAction{
			.type = RichTextPendingActionType::InsertToken,
			.cursor = selection.cursor,
			.selection_start = selection.selection_start,
			.selection_end = selection.selection_end,
			.token = std::move(token),
		};
		selection.apply_selection = true;
		selection.keep_selection_highlight = true;
	};

	auto tag_button = [&](const char* label, std::string_view tag, std::string_view open,
						  std::string_view close, std::string_view tooltip) {
		if (ImGui::Button(label, ImVec2{ 0.0f, button_height })) {
			begin_text_action();
			queue_toggle_tag(tag, open, close);
		}
		DrawRichTextToolbarTooltip(tooltip);
	};

	const bool default_bold{ HasFontFlag(defaults.style.flags, FontStyle::Bold) };
	const bool default_italic{ HasFontFlag(defaults.style.flags, FontStyle::Italic) };
	const bool default_underline{ HasFontFlag(defaults.style.flags, FontStyle::Underline) };
	const bool default_strikethrough{
		HasFontFlag(defaults.style.flags, FontStyle::Strikethrough)
	};

	tag_button(
		"B", "b", default_bold ? "<b=off>" : "<b>", "</b>",
		"Bold.\n<b>...</b>\n<b=>...</b> (engine-default weight)\n<b=0.2>...</b>\n<b=off>...</b>"
	);
	ImGui::SameLine();
	tag_button(
		"I", "i", default_italic ? "<i=off>" : "<i>", "</i>",
		"Italic.\n<i>...</i>\n<i=off>...</i>"
	);
	ImGui::SameLine();
	tag_button(
		"U", "u", default_underline ? "<u=off>" : "<u>", "</u>",
		"Underline.\n<u>...</u>\n<u=off>...</u>"
	);
	ImGui::SameLine();
	tag_button(
		"S", "s", default_strikethrough ? "<s=off>" : "<s>", "</s>",
		"Strikethrough.\n<s>...</s>\n<s=off>...</s>"
	);

	ImGui::SameLine();
	if (ImGui::Button("Color", ImVec2{ 0.0f, button_height })) {
		preserve_source_selection();
		ImGui::OpenPopup("RichTextColorPopup");
	}
	DrawRichTextToolbarTooltip(
		"Text color.\n<c=>...</c> uses the engine-default color.\n"
		"<c=red>...</c>\n<c=#RRGGBB>...</c>\n<c=#RRGGBBAA>...</c>"
	);
	if (ImGui::BeginPopup("RichTextColorPopup")) {
		const bool color_changed{ ImGui::ColorPicker4(
			"##RichTextColorPicker", state.color.data(),
			ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB
		) };
		if (ImGui::IsItemActivated()) {
			begin_text_action();
		}
		if (color_changed) {
			if (RichTextEditorColorEquals(state.color, defaults.style.color)) {
				queue_remove_tag("c");
			} else if (RichTextEditorColorEquals(state.color, engine_defaults.style.color)) {
				queue_set_tag("c", "<c=>", "</c>");
			} else {
				queue_set_tag(
					"c", "<c=" + RichTextEditorColorTag(state.color) + ">", "</c>"
				);
			}
		}

		if (ImGui::Button("Reset Color", ImVec2{ -FLT_MIN, 0.0f })) {
			begin_text_action();
			SetRichTextEditorColor(state.color, defaults.style.color);
			queue_remove_tag("c");
		}
		DrawRichTextToolbarTooltip("Remove the explicit color override and use the rich-text default color.");

		ImGui::EndPopup();
	}

	ImGui::SameLine();
	if (ImGui::Button("Font", ImVec2{ 0.0f, button_height })) {
		preserve_source_selection();
		ImGui::OpenPopup("RichTextFontPopup");
	}
	DrawRichTextToolbarTooltip("Font key.\n<font=>...</font> uses the engine-default font.\n<font=key>...</font>");
	if (ImGui::BeginPopup("RichTextFontPopup")) {
		auto apply_font = [&]() {
			if (state.font == defaults.font.value) {
				queue_remove_tag("font");
			} else if (state.font == engine_defaults.font.value) {
				queue_set_tag("font", "<font=>", "</font>");
			} else {
				queue_set_tag("font", "<font=" + state.font + ">", "</font>");
			}
		};

		const auto loaded_fonts{ GetLoadedRichTextFontKeys(ctx) };
		const char* preview{ state.font.empty() ? "Default" : state.font.c_str() };
		ImGui::SetNextItemWidth(260.0f);
		if (ImGui::BeginCombo("Loaded##RichTextFont", preview)) {
			for (const auto& font : loaded_fonts) {
				const bool selected{ state.font == font };
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
			"Custom##RichTextFont", "Custom font key", &state.font
		) };
		if (ImGui::IsItemActivated()) {
			begin_text_action();
		}
		if (font_changed) {
			apply_font();
		}

		if (ImGui::Button("Reset Font", ImVec2{ -FLT_MIN, 0.0f })) {
			begin_text_action();
			state.font = defaults.font.value;
			queue_remove_tag("font");
		}
		DrawRichTextToolbarTooltip("Remove the explicit font override and use the rich-text default font.");

		ImGui::EndPopup();
	}

	ImGui::SameLine();
	if (ImGui::Button("Size", ImVec2{ 0.0f, button_height })) {
		preserve_source_selection();
		ImGui::OpenPopup("RichTextSizePopup");
	}
	DrawRichTextToolbarTooltip("Font size.\n<size=>...</size> uses the engine-default size.\n<size=32>...</size>");
	if (ImGui::BeginPopup("RichTextSizePopup")) {
		auto apply_size = [&]() {
			if (RichTextEditorFloatEquals(state.size, defaults.style.size)) {
				queue_remove_tag("size");
				return;
			}
			if (RichTextEditorFloatEquals(state.size, engine_defaults.style.size)) {
				queue_set_tag("size", "<size=>", "</size>");
				return;
			}
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
		DrawRichTextToolbarTooltip("Remove the explicit size override and use the rich-text default size.");

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
		ImGui::Separator();
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
		ImGui::EndCombo();
	}
	DrawRichTextToolbarTooltip(
		"Effect tags.\n"
		"<outline=color,width[,softness]>\n"
		"<shadow=color,x,y[,width[,softness]]>\n"
		"<outerglow=color,width[,softness]>\n"
		"<innerglow=color,width[,softness]>\n"
		"<fx=type[,amplitude[,frequency[,speed[,phase]]]]>"
	);

	if (allow_detached_window) {
		ImGui::SameLine();
		if (ImGui::Button("Open", ImVec2{ 0.0f, button_height })) {
			state.window_open = true;
			state.window_recenter_requested = true;
			state.window_selection = selection;
			state.window_selection.apply_selection = true;
		}
		DrawRichTextToolbarTooltip("Open a larger rich-text editor.");
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

	const float editor_height{
		detached
			? std::max(
				  300.0f,
				  ImGui::GetTextLineHeightWithSpacing() *
					  static_cast<float>(std::max(options.line_count, 14)) +
					  ImGui::GetStyle().FramePadding.y * 2.0f
			  )
			: ImGui::GetTextLineHeightWithSpacing() *
				  static_cast<float>(std::max(options.line_count, 3)) +
				  ImGui::GetStyle().FramePadding.y * 2.0f
	};
	if (DrawRichTextSourceInput(source, selection, editor_height)) {
		changed = true;
	}

	DrawRichTextToolbarTooltip(
		"Rich-text markup.\n"
		"<b>Bold</b> / <b=off>not bold</b>\n"
		"<i>Italic</i> / <i=off>not italic</i>\n"
		"<u>Underline</u> / <u=off>not underlined</u>\n"
		"<s>Strike</s> / <s=off>not struck</s>\n"
		"Empty assignments such as <font=>, <size=> and <c=> use engine defaults.\n"
		"Escape: \\<  \\>  \\\\"
	);

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

		// Keep diagnostics above the preview so the preview remains the final section
		// in both the inline and detached editors.
		for (const auto& diagnostic : source_diagnostics) {
			ImGui::TextColored(
				ImVec4{ 1.0f, 0.45f, 0.2f, 1.0f }, "Character %zu: %s",
				RichTextCharacterPosition(source, diagnostic.position), diagnostic.message.c_str()
			);
		}

		ImGui::SeparatorText("Preview");
		ImGui::BeginChild(
			"##RichTextPreview", ImVec2{ -FLT_MIN, detached ? 180.0f : 120.0f }, true,
			ImGuiWindowFlags_HorizontalScrollbar
		);
		DrawRichTextPreview(parsed.text);
		ImGui::EndChild();
		DrawRichTextToolbarTooltip(
			"Live preview of size, color, BIUS, spacing, SDF layers and glyph effects."
		);
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
		}
	}

	ClampRichTextSelection(state.inline_selection, state.source.size());
	ClampRichTextSelection(state.window_selection, state.source.size());

	// Formatting controls are overrides only. Keep a defensive snapshot so no
	// toolbar action can change Defaults; only edits made inside the explicit
	// Defaults tree are allowed to persist.
	const TextRunDefaults defaults_before{ defaults };
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

		if (state.window_recenter_requested) {
			auto* viewport{ ImGui::GetMainViewport() };
			const ImVec2 size{ viewport->WorkSize.x * 0.60f, viewport->WorkSize.y * 0.70f };
			const ImVec2 center{
				viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
				viewport->WorkPos.y + viewport->WorkSize.y * 0.5f,
			};

			ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
			ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2{ 0.5f, 0.5f });
			ImGui::SetNextWindowSize(size, ImGuiCond_Always);
			ImGui::SetNextWindowFocus();
			state.window_recenter_requested = false;
		}

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

	if (defaults_changed) {
		// Defaults are authoring data, never inferred from tags. When the user explicitly
		// changes them, reserialize against the new baseline so now-redundant overrides are
		// removed (for example <font=> when Defaults already uses the engine font).
		state.source = SerializeStyledTextToRichText(
			ParseRichText(state.source, defaults).text, defaults
		);
		reset_selection_to_end(state.inline_selection);
		reset_selection_to_end(state.window_selection);
	} else {
		defaults = defaults_before;
	}

	// Always expose the exact authored source to the caller. A resolved StyledText caller
	// may discard empty tags in its runtime representation, but the editor keeps them in
	// state.source so typing between them on the next frame still produces tagged text.
	source = state.source;

	ImGui::PopID();
	return changed;
}

} // namespace ptgn::editor::inspector
