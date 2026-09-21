#pragma once

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ptgn::editor::inspector {

/// Responsive layout policy shared by ordinary inspector rows.
///
/// Labels and controls are deliberately sized from the current content region rather than from
/// previously measured rows. This keeps independently drawn sections aligned as long as they are
/// at the same indentation level and, unlike the previous auto-width scheme, never lets a long
/// label consume the value column.
struct InspectorLayoutStyle {
	float label_fraction{ 0.34f };
	float minimum_label_width{ 64.0f };
	float maximum_label_width{ 220.0f };
	float minimum_value_width{ 72.0f };
	float compact_value_width{ 190.0f };
	float row_spacing{ 3.0f };
};


[[nodiscard]] inline bool& InspectorInlineValueMode() {
	static thread_local bool active{ false };
	return active;
}

class ScopedInspectorInlineValue {
public:
	ScopedInspectorInlineValue() : previous_{ InspectorInlineValueMode() } {
		InspectorInlineValueMode() = true;
	}
	~ScopedInspectorInlineValue() { InspectorInlineValueMode() = previous_; }

private:
	bool previous_{ false };
};

[[nodiscard]] inline InspectorLayoutStyle& GetInspectorLayoutStyle() {
	static InspectorLayoutStyle style;
	return style;
}

[[nodiscard]] inline float GetInspectorLabelColumnWidth(float available_width) {
	const auto& layout{ GetInspectorLayoutStyle() };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float maximum_allowed{
		std::max(1.0f, available_width - layout.minimum_value_width - spacing)
	};
	const float preferred{
		std::clamp(
			available_width * layout.label_fraction,
			layout.minimum_label_width,
			layout.maximum_label_width
		)
	};
	return std::clamp(preferred, 1.0f, maximum_allowed);
}

/// Draws an inspector label at the normal font size and wraps only at whitespace.
///
/// A single word is never split across rows. If that word is wider than the label column it is
/// clipped at the column boundary, which keeps it out of the value column while preserving the
/// requested fixed font size.
inline void DrawFittedInspectorLabel(std::string_view label) {
	const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
	ImFont* font{ ImGui::GetFont() };
	const float font_size{ ImGui::GetFontSize() };
	const float line_height{ ImGui::GetTextLineHeight() };

	std::vector<std::string> lines;
	lines.reserve(2);
	std::string line;
	bool clipped_word{ false };

	auto flush_line = [&]() {
		if (!line.empty()) {
			lines.emplace_back(std::move(line));
			line.clear();
		}
	};

	std::size_t position{};
	while (position < label.size()) {
		if (label[position] == '\n') {
			flush_line();
			if (lines.empty() || (position > 0 && label[position - 1] == '\n')) {
				lines.emplace_back();
			}
			++position;
			continue;
		}

		while (position < label.size() &&
			(label[position] == ' ' || label[position] == '\t' || label[position] == '\r')) {
			++position;
		}
		if (position >= label.size()) {
			break;
		}

		const std::size_t word_begin{ position };
		while (position < label.size() && label[position] != ' ' && label[position] != '\t' &&
			label[position] != '\r' && label[position] != '\n') {
			++position;
		}
		const std::string_view word{ label.substr(word_begin, position - word_begin) };
		const float word_width{
			font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, word.data(), word.data() + word.size()).x
		};

		if (line.empty()) {
			if (word_width > available) {
				lines.emplace_back(word);
				clipped_word = true;
			} else {
				line.assign(word);
			}
			continue;
		}

		std::string candidate{ line };
		candidate.push_back(' ');
		candidate.append(word);
		const float candidate_width{
			font->CalcTextSizeA(
				font_size, FLT_MAX, 0.0f, candidate.data(), candidate.data() + candidate.size()
			).x
		};
		if (candidate_width <= available) {
			line = std::move(candidate);
			continue;
		}

		flush_line();
		if (word_width > available) {
			lines.emplace_back(word);
			clipped_word = true;
		} else {
			line.assign(word);
		}
	}
	flush_line();
	if (lines.empty()) {
		lines.emplace_back();
	}

	const ImGuiStyle& style{ ImGui::GetStyle() };
	const float vertical_padding{ style.FramePadding.y };
	const float text_height{ line_height * static_cast<float>(lines.size()) };
	const float row_height{
		std::max(ImGui::GetFrameHeight(), text_height + vertical_padding * 2.0f)
	};

	const ImVec2 cursor_start{ ImGui::GetCursorScreenPos() };
	const ImVec2 start{ cursor_start.x, cursor_start.y + vertical_padding };
	const ImVec4 clip_rect{
		start.x,
		cursor_start.y,
		start.x + available,
		cursor_start.y + row_height,
	};

	for (std::size_t i{ 0 }; i < lines.size(); ++i) {
		const auto& current{ lines[i] };
		const ImVec2 position_px{
			start.x,
			start.y + static_cast<float>(i) * line_height
		};
		ImGui::GetWindowDrawList()->AddText(
			font,
			font_size,
			position_px,
			ImGui::GetColorU32(ImGuiCol_Text),
			current.data(),
			current.data() + current.size(),
			0.0f,
			&clip_rect
		);
	}

	ImGui::Dummy(ImVec2{ available, row_height });
	if (clipped_word && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%.*s", static_cast<int>(label.size()), label.data());
	}
}

/// Draws one responsive custom label/value row. Each row owns a tiny two-column table so callers
/// do not need an outer layout scope and semantic sections may freely interleave rows, tree nodes
/// and custom controls. Equal indentation produces equal column positions across sections.
template <typename DrawLabel, typename DrawValue>
bool DrawInspectorCustomPropertyRow(
	std::string_view id, DrawLabel&& draw_label, DrawValue&& draw_value
) {
	ImGui::PushID(id.data(), id.data() + id.size());

	const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
	const float label_width{ GetInspectorLabelColumnWidth(available) };
	const ImGuiTableFlags flags{
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings |
		ImGuiTableFlags_NoPadOuterX |
		ImGuiTableFlags_NoBordersInBody
	};

	bool changed{ false };
	const ImGuiStyle& style{ ImGui::GetStyle() };
	const auto& layout{ GetInspectorLayoutStyle() };
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2{ style.CellPadding.x, 0.0f });
	ImGui::PushStyleVar(
		ImGuiStyleVar_ItemSpacing,
		ImVec2{ style.ItemSpacing.x, layout.row_spacing }
	);
	if (ImGui::BeginTable("##InspectorPropertyRow", 2, flags, ImVec2{ available, 0.0f })) {
		ImGui::TableSetupColumn(
			"##Label", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize,
			label_width
		);
		ImGui::TableSetupColumn(
			"##Value", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize,
			1.0f
		);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		std::invoke(std::forward<DrawLabel>(draw_label));
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed = std::invoke(std::forward<DrawValue>(draw_value));
		ImGui::EndTable();
	}
	ImGui::PopStyleVar(2);

	ImGui::PopID();
	return changed;
}

/// Standard fitted-text label/value row.
template <typename Draw>
bool DrawInspectorPropertyRow(std::string_view label, Draw&& draw) {
	if (InspectorInlineValueMode()) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		return std::invoke(std::forward<Draw>(draw));
	}
	return DrawInspectorCustomPropertyRow(
		label, [&]() { DrawFittedInspectorLabel(label); }, std::forward<Draw>(draw)
	);
}

enum class InspectorTreeToggleSide {
	Left,
	Right,
};

struct InspectorTreeToggleResult {
	bool open{ false };
	bool toggle_changed{ false };
};

/// Draw a framed tree-node row with an enable checkbox at one edge. The tree receives all
/// remaining horizontal space instead of being constrained to the ordinary inspector label
/// column. This is intended for optional/nested inspector sections.
inline InspectorTreeToggleResult DrawInspectorTreeToggleRow(
	std::string_view label,
	std::string_view id,
	bool& enabled,
	bool read_only = false,
	InspectorTreeToggleSide toggle_side = InspectorTreeToggleSide::Right,
	bool default_open = false
) {
	ImGui::PushID(id.data(), id.data() + id.size());

	const ImGuiStyle& style{ ImGui::GetStyle() };
	const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
	const float spacing{ style.ItemInnerSpacing.x };
	const float toggle_column_width{ ImGui::GetFrameHeight() + spacing };
	const ImGuiTableFlags table_flags{
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings |
		ImGuiTableFlags_NoPadOuterX |
		ImGuiTableFlags_NoBordersInBody
	};

	InspectorTreeToggleResult result;
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2{ 0.0f, 0.0f });
	if (ImGui::BeginTable(
			"##InspectorTreeToggleRow", 2, table_flags, ImVec2{ available, 0.0f }
		)) {
		const bool toggle_left{ toggle_side == InspectorTreeToggleSide::Left };
		if (toggle_left) {
			ImGui::TableSetupColumn(
				"##Toggle", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize,
				toggle_column_width
			);
			ImGui::TableSetupColumn(
				"##Tree", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize,
				1.0f
			);
		} else {
			ImGui::TableSetupColumn(
				"##Tree", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize,
				1.0f
			);
			ImGui::TableSetupColumn(
				"##Toggle", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize,
				toggle_column_width
			);
		}

		ImGui::TableNextRow();
		auto draw_toggle = [&]() {
			if (!toggle_left) {
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + spacing);
			}
			ImGui::BeginDisabled(read_only);
			result.toggle_changed = ImGui::Checkbox("##Enabled", &enabled);
			ImGui::EndDisabled();
		};

		auto draw_tree = [&]() {
			ImGuiTreeNodeFlags flags{
				ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_FramePadding |
				ImGuiTreeNodeFlags_NoTreePushOnOpen
			};
			if (default_open) {
				flags |= ImGuiTreeNodeFlags_DefaultOpen;
			}

			std::string tree_label{ label };
			tree_label += "##Tree";
			result.open = ImGui::TreeNodeEx(tree_label.c_str(), flags);
		};

		if (toggle_left) {
			ImGui::TableSetColumnIndex(0);
			draw_toggle();
			ImGui::TableSetColumnIndex(1);
			draw_tree();
		} else {
			ImGui::TableSetColumnIndex(0);
			draw_tree();
			ImGui::TableSetColumnIndex(1);
			draw_toggle();
		}

		ImGui::EndTable();
	}
	ImGui::PopStyleVar();

	ImGui::PopID();
	return result;
}

[[nodiscard]] inline bool InspectorValueColumnIsCompact() {
	return ImGui::GetContentRegionAvail().x < GetInspectorLayoutStyle().compact_value_width;
}

[[nodiscard]] inline float InspectorSplitWidth(
	int count,
	float available_width = -1.0f,
	float spacing = -1.0f
) {
	if (count <= 0) {
		return 0.0f;
	}
	if (available_width < 0.0f) {
		available_width = ImGui::GetContentRegionAvail().x;
	}
	if (spacing < 0.0f) {
		spacing = ImGui::GetStyle().ItemInnerSpacing.x;
	}
	return std::max(
		1.0f,
		(available_width - spacing * static_cast<float>(count - 1)) /
			static_cast<float>(count)
	);
}

struct InspectorAction {
	std::string_view label{};
	std::string_view tooltip{};
	bool enabled{ true };
	std::function<void()> invoke{};
};

struct InspectorActionBarOptions {
	std::string_view id{ "##InspectorActionBar" };
	std::string_view overflow_label{ "..." };
	std::string_view overflow_tooltip{ "More actions" };

	/// @brief When true, all visible actions share the full available row width equally.
	bool equal_width{ false };
};

/// Draws as many actions inline as fit. Remaining actions are placed in one overflow menu instead
/// of extending beyond the inspector edge.
inline bool DrawInspectorActionBar(
	std::span<const InspectorAction> actions,
	InspectorActionBarOptions options = {}
) {
	if (actions.empty()) {
		return false;
	}

	ImGui::PushID(options.id.data(), options.id.data() + options.id.size());
	const auto& style{ ImGui::GetStyle() };
	const float spacing{ style.ItemSpacing.x };
	const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
	auto button_width = [&](std::string_view label) {
		return ImGui::CalcTextSize(label.data(), label.data() + label.size()).x +
			style.FramePadding.x * 2.0f;
	};

	const float equal_button_width{
		options.equal_width
			? InspectorSplitWidth(
				  static_cast<int>(actions.size()), available, spacing
			  )
			: 0.0f
	};

	auto resolved_button_width = [&](std::string_view label) {
		return options.equal_width ? equal_button_width : button_width(label);
	};

	float full_width{};
	for (std::size_t i{ 0 }; i < actions.size(); ++i) {
		if (i > 0) {
			full_width += spacing;
		}
		full_width += resolved_button_width(actions[i].label);
	}

	std::size_t visible_count{ actions.size() };
	const float overflow_width{
		std::min(
			available,
			std::max(ImGui::GetFrameHeight(), button_width(options.overflow_label))
		)
	};
	if (!options.equal_width && full_width > available) {
		visible_count = 0;
		float used{ overflow_width };
		for (std::size_t i{ 0 }; i < actions.size(); ++i) {
			const float candidate{ resolved_button_width(actions[i].label) + spacing };
			if (used + candidate > available) {
				break;
			}
			used += candidate;
			++visible_count;
		}
	}

	bool invoked{ false };
	for (std::size_t i{ 0 }; i < visible_count; ++i) {
		if (i > 0) {
			ImGui::SameLine(0.0f, spacing);
		}
		ImGui::PushID(static_cast<int>(i));
		ImGui::BeginDisabled(!actions[i].enabled);
		const std::string action_label{ actions[i].label };
		if (ImGui::Button(
				action_label.c_str(),
				ImVec2{ resolved_button_width(actions[i].label), 0.0f }
			) && actions[i].invoke) {
			actions[i].invoke();
			invoked = true;
		}
		ImGui::EndDisabled();
		if (!actions[i].tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip(
				"%.*s", static_cast<int>(actions[i].tooltip.size()), actions[i].tooltip.data()
			);
		}
		ImGui::PopID();
	}

	if (visible_count < actions.size()) {
		if (visible_count > 0) {
			ImGui::SameLine(0.0f, spacing);
		}
		const std::string overflow_label{ options.overflow_label };
		if (ImGui::Button(overflow_label.c_str(), ImVec2{ overflow_width, 0.0f })) {
			ImGui::OpenPopup("##InspectorActionOverflow");
		}
		if (!options.overflow_tooltip.empty() && ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"%.*s", static_cast<int>(options.overflow_tooltip.size()),
				options.overflow_tooltip.data()
			);
		}
		if (ImGui::BeginPopup("##InspectorActionOverflow")) {
			for (std::size_t i{ visible_count }; i < actions.size(); ++i) {
				const std::string action_label{ actions[i].label };
				if (ImGui::MenuItem(
					action_label.c_str(), nullptr, false, actions[i].enabled
				) && actions[i].invoke) {
					actions[i].invoke();
					invoked = true;
				}
				if (!actions[i].tooltip.empty() && ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"%.*s", static_cast<int>(actions[i].tooltip.size()),
						actions[i].tooltip.data()
					);
				}
			}
			ImGui::EndPopup();
		}
	}

	ImGui::PopID();
	return invoked;
}


struct InspectorChoice {
	std::string_view label{};
	std::string_view tooltip{};
	bool selected{ false };
	bool enabled{ true };
	std::function<void()> invoke{};
};

/// Draw a radio-style choice row while there is room, then collapse to a combo at narrow widths.
/// This is intended for short mutually-exclusive inspector modes such as interaction mode, button
/// preview state, movement type, etc.
inline bool DrawInspectorChoiceBar(
	std::span<const InspectorChoice> choices,
	std::string_view combo_id = "##InspectorChoices"
) {
	if (choices.empty()) {
		return false;
	}

	const auto& style{ ImGui::GetStyle() };
	const float spacing{ style.ItemSpacing.x };
	const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
	auto radio_width = [&](std::string_view label) {
		return ImGui::GetFrameHeight() + style.ItemInnerSpacing.x +
			ImGui::CalcTextSize(label.data(), label.data() + label.size()).x;
	};

	float full_width{};
	for (std::size_t i{ 0 }; i < choices.size(); ++i) {
		if (i > 0) {
			full_width += spacing;
		}
		full_width += radio_width(choices[i].label);
	}

	bool invoked{ false };
	if (full_width <= available) {
		for (std::size_t i{ 0 }; i < choices.size(); ++i) {
			if (i > 0) {
				ImGui::SameLine(0.0f, spacing);
			}
			ImGui::PushID(static_cast<int>(i));
			ImGui::BeginDisabled(!choices[i].enabled);
			const std::string label{ choices[i].label };
			if (ImGui::RadioButton(label.c_str(), choices[i].selected) &&
				!choices[i].selected && choices[i].invoke) {
				choices[i].invoke();
				invoked = true;
			}
			ImGui::EndDisabled();
			if (!choices[i].tooltip.empty() &&
				ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip(
					"%.*s", static_cast<int>(choices[i].tooltip.size()), choices[i].tooltip.data()
				);
			}
			ImGui::PopID();
		}
		return invoked;
	}

	std::string preview{ "Select" };
	for (const auto& choice : choices) {
		if (choice.selected) {
			preview.assign(choice.label);
			break;
		}
	}

	const std::string combo_name{ combo_id };
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (!ImGui::BeginCombo(combo_name.c_str(), preview.c_str())) {
		return false;
	}
	for (std::size_t i{ 0 }; i < choices.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		const std::string label{ choices[i].label };
		if (ImGui::Selectable(label.c_str(), choices[i].selected, choices[i].enabled ? 0 : ImGuiSelectableFlags_Disabled) &&
			!choices[i].selected && choices[i].enabled && choices[i].invoke) {
			choices[i].invoke();
			invoked = true;
		}
		if (!choices[i].tooltip.empty() && ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"%.*s", static_cast<int>(choices[i].tooltip.size()), choices[i].tooltip.data()
			);
		}
		ImGui::PopID();
	}
	ImGui::EndCombo();
	return invoked;
}

struct InspectorSectionOptions {
	bool default_open{ false };
	bool removable{ false };
	bool resettable{ false };
	bool renamable{ false };
};

struct InspectorSectionResult {
	bool open{ false };
	bool remove_requested{ false };
	bool reset_requested{ false };
	bool rename_requested{ false };
};

/// Consistent top-level section header/context menu. It intentionally only reports structural
/// requests; the caller owns model-specific undoable rename/reset/remove behavior.
inline InspectorSectionResult DrawInspectorSectionHeader(
	std::string_view label,
	std::string_view id,
	InspectorSectionOptions options = {}
) {
	ImGui::PushID(id.data(), id.data() + id.size());
	ImGuiTreeNodeFlags flags{};
	if (options.default_open) {
		flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}

	std::string visible_label{ label };
	visible_label += "##Section";
	InspectorSectionResult result{
		.open = ImGui::CollapsingHeader(visible_label.c_str(), flags),
	};

	if ((options.removable || options.resettable || options.renamable) &&
		ImGui::BeginPopupContextItem("##SectionContext")) {
		if (options.renamable && ImGui::MenuItem("Rename")) {
			result.rename_requested = true;
		}
		if (options.resettable && ImGui::MenuItem("Reset")) {
			result.reset_requested = true;
		}
		if ((options.renamable || options.resettable) && options.removable) {
			ImGui::Separator();
		}
		if (options.removable && ImGui::MenuItem("Remove")) {
			result.remove_requested = true;
			result.open = false;
		}
		ImGui::EndPopup();
	}

	ImGui::PopID();
	return result;
}


/// Nested counterpart to DrawInspectorSectionHeader. The caller must call ImGui::TreePop() when
/// result.open is true. Structural requests are intentionally returned rather than executed so the
/// same helper works for entity components, managed UI parts, timers, scripts and other authored
/// objects with different undo backends.
inline InspectorSectionResult DrawInspectorTreeNodeHeader(
	std::string_view label,
	std::string_view id,
	InspectorSectionOptions options = {}
) {
	ImGui::PushID(id.data(), id.data() + id.size());
	ImGuiTreeNodeFlags flags{
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_FramePadding
	};
	if (options.default_open) {
		flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}

	std::string visible_label{ label };
	visible_label += "##Tree";
	InspectorSectionResult result{
		.open = ImGui::TreeNodeEx(visible_label.c_str(), flags),
	};

	if ((options.removable || options.resettable || options.renamable) &&
		ImGui::BeginPopupContextItem("##TreeContext")) {
		if (options.renamable && ImGui::MenuItem("Rename")) {
			result.rename_requested = true;
		}
		if (options.resettable && ImGui::MenuItem("Reset")) {
			result.reset_requested = true;
		}
		if ((options.renamable || options.resettable) && options.removable) {
			ImGui::Separator();
		}
		if (options.removable && ImGui::MenuItem("Remove")) {
			result.remove_requested = true;
		}
		ImGui::EndPopup();
	}

	ImGui::PopID();
	return result;
}

} // namespace ptgn::editor::inspector
