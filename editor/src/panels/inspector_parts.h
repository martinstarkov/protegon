#pragma once

#include <imgui.h>

#include <cfloat>
#include <string>
#include <string_view>

namespace ptgn::editor::inspector {

/// Shared presentation for bounded UI-owned parts.
///
/// Unlike script/dialogue collections, UI parts have a finite set of possible children. While at
/// least one part is missing, show one full-width Add Parts button above the existing part trees.
inline bool DrawInspectorAddPartsButton(
	bool show,
	std::string_view label = "Add Parts",
	std::string_view tooltip = {}
) {
	if (!show) {
		return false;
	}

	const std::string button_label{ label };
	const bool pressed{
		ImGui::Button(button_label.c_str(), ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })
	};
	if (!tooltip.empty() && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
	}
	return pressed;
}

struct InspectorPartTreeResult {
	bool open{ false };
	bool remove_requested{ false };
};

/// Draw one UI-owned part as a normal inspector tree node. The tree node itself owns the single
/// indentation level for its contents; callers should draw fields directly and call TreePop()
/// when `open` is true rather than adding another ScopedIndent.
inline InspectorPartTreeResult DrawInspectorPartTreeNode(
	std::string_view label,
	std::string_view id,
	bool allow_remove = true,
	bool default_open = false,
	std::string_view remove_label = "Remove Part"
) {
	std::string tree_label{ label };
	tree_label += "###";
	tree_label.append(id.data(), id.size());

	ImGuiTreeNodeFlags flags{
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_FramePadding
	};
	if (default_open) {
		flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}

	InspectorPartTreeResult result{
		.open = ImGui::TreeNodeEx(tree_label.c_str(), flags),
	};

	if (allow_remove && ImGui::BeginPopupContextItem()) {
		const std::string menu_label{ remove_label };
		if (ImGui::MenuItem(menu_label.c_str())) {
			result.remove_requested = true;
		}
		ImGui::EndPopup();
	}

	return result;
}

} // namespace ptgn::editor::inspector
