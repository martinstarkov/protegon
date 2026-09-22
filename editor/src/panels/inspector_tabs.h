#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>

#include "editor/renamable_item.h"

namespace ptgn::editor::inspector {

/// Keeps an inspector tab bar visually constrained to the inspector's visible content width.
/// Dear ImGui intentionally accounts for WidthAllTabs in the host window's content size when
/// FittingPolicyScroll is used. That is useful for ordinary windows, but it would make a long
/// inspector tab set create a parent horizontal scrollbar. This scope removes only that extra
/// horizontal contribution while leaving the selected tab contents in the normal inspector flow.
class InspectorTabStripScope {
public:
	explicit InspectorTabStripScope(const char*) :
		window_{ ImGui::GetCurrentWindow() },
		previous_tab_bar_border_size_{ ImGui::GetStyle().TabBarBorderSize } {
		if (window_) {
			cursor_max_x_before_ = window_->DC.CursorMaxPos.x;
			ideal_max_x_before_ = window_->DC.IdealMaxPos.x;

			const ImVec2 cursor{ ImGui::GetCursorScreenPos() };
			content_right_x_ = cursor.x + std::max(0.0f, ImGui::GetContentRegionAvail().x);
		}

		ImGui::GetStyle().TabBarBorderSize = 0.0f;
	}

	~InspectorTabStripScope() {
		ImGui::GetStyle().TabBarBorderSize = previous_tab_bar_border_size_;

		if (!window_ || window_ != ImGui::GetCurrentWindow()) {
			return;
		}

		window_->DC.CursorMaxPos.x = std::max(
			cursor_max_x_before_,
			std::min(window_->DC.CursorMaxPos.x, content_right_x_)
		);
		window_->DC.IdealMaxPos.x = std::max(
			ideal_max_x_before_,
			std::min(window_->DC.IdealMaxPos.x, content_right_x_)
		);

		window_->Scroll.x = 0.0f;
		ImGui::SetScrollX(window_, 0.0f);
	}

	InspectorTabStripScope(const InspectorTabStripScope&) = delete;
	InspectorTabStripScope& operator=(const InspectorTabStripScope&) = delete;

private:
	ImGuiWindow* window_{ nullptr };
	float previous_tab_bar_border_size_{ 0.0f };
	float cursor_max_x_before_{ 0.0f };
	float ideal_max_x_before_{ 0.0f };
	float content_right_x_{ 0.0f };
};

[[nodiscard]] inline ImGuiTabBarFlags InspectorTabBarFlags() {
	return ImGuiTabBarFlags_AutoSelectNewTabs |
		ImGuiTabBarFlags_FittingPolicyScroll |
		ImGuiTabBarFlags_NoTabListScrollingButtons;
}

inline bool DrawInspectorAddTabButton(const char* id, std::string_view tooltip = {}) {
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ImGui::GetStyle().TabRounding);
	const bool pressed{
		ImGui::TabItemButton(id, ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)
	};
	ImGui::PopStyleVar();

	if (!tooltip.empty() && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
	}
	return pressed;
}

/// While the pointer is inside the tab rectangle, vertical wheel input navigates the tab bar
/// horizontally. A real horizontal wheel/trackpad gesture remains supported, but vertical input
/// wins whenever both axes are present so tiny trackpad X jitter cannot make the strip bounce
/// left/right at either end.
inline void ApplyInspectorTabBarHorizontalWheel() {
	ImGuiTabBar* tab_bar{ ImGui::GetCurrentTabBar() };
	if (!tab_bar || !ImGui::IsMouseHoveringRect(tab_bar->BarRect.Min, tab_bar->BarRect.Max, true)) {
		return;
	}

	ImGuiIO& io{ ImGui::GetIO() };
	ImGui::SetKeyOwner(ImGuiKey_MouseWheelX, tab_bar->ID);
	ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, tab_bar->ID);

	if (io.MouseWheelRequestAxisSwap) {
		return;
	}

	const float wheel{ io.MouseWheel };
	if (wheel == 0.0f) {
		return;
	}

	const float maximum_scroll{
		std::max(0.0f, tab_bar->WidthAllTabs - tab_bar->BarRect.GetWidth())
	};
	if (maximum_scroll <= 0.0f) {
		tab_bar->ScrollingTarget = 0.0f;
		tab_bar->ScrollingAnim = 0.0f;
		tab_bar->ScrollingSpeed = 0.0f;
		return;
	}

	const float current{
		std::clamp(tab_bar->ScrollingTarget, 0.0f, maximum_scroll)
	};
	constexpr float edge_epsilon{ 0.5f };

	if ((wheel > 0.0f && current <= edge_epsilon) ||
		(wheel < 0.0f && current >= maximum_scroll - edge_epsilon)) {
		const float edge{ wheel > 0.0f ? 0.0f : maximum_scroll };
		tab_bar->ScrollingTarget = edge;
		tab_bar->ScrollingAnim = edge;
		tab_bar->ScrollingSpeed = 0.0f;
		tab_bar->ScrollingTargetDistToVisibility = 0.0f;
		return;
	}

	const float step{ ImGui::GetFontSize() * 5.0f };
	const float target{
		std::clamp(current - wheel * step, 0.0f, maximum_scroll)
	};

	tab_bar->ScrollingTarget = target;
	tab_bar->ScrollingAnim = target;
	tab_bar->ScrollingSpeed = 0.0f;
	tab_bar->ScrollingTargetDistToVisibility = 0.0f;

	if (ImGuiWindow* window{ ImGui::GetCurrentWindow() }) {
		window->Scroll.x = 0.0f;
		ImGui::SetScrollX(window, 0.0f);
	}
}

/// Shared empty-state/tab-strip shell used by all inspector-owned tab collections.
///
/// With no items it renders a full-width add button. Once at least one item exists it renders the
/// supplied tabs plus the trailing + tab. The caller owns the actual add operation/menu so this can
/// be reused by direct-add collections (script sequences) and popup-add collections (UI parts,
/// interaction hit areas, etc.).
struct InspectorTabCollectionOptions {
	const char* scope_id{ "##InspectorTabCollectionScope" };
	const char* tab_bar_id{ "##InspectorTabCollection" };
	const char* add_tab_id{ "+##InspectorTabCollectionAdd" };
	std::string_view empty_add_label{ "Add Item" };
	std::string_view add_tooltip{ "Add item" };
	bool show_add{ true };
};

template <typename DrawTabs>
[[nodiscard]] bool DrawInspectorTabCollection(
	bool empty,
	const InspectorTabCollectionOptions& options,
	DrawTabs&& draw_tabs
) {
	if (empty) {
		if (!options.show_add) {
			return false;
		}

		const std::string label{ options.empty_add_label };
		const bool pressed{
			ImGui::Button(label.c_str(), ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() })
		};
		if (!options.add_tooltip.empty() && ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"%.*s", static_cast<int>(options.add_tooltip.size()), options.add_tooltip.data()
			);
		}
		return pressed;
	}

	bool add_requested{ false };
	InspectorTabStripScope strip{ options.scope_id };
	if (ImGui::BeginTabBar(options.tab_bar_id, InspectorTabBarFlags())) {
		std::forward<DrawTabs>(draw_tabs)();

		if (options.show_add) {
			add_requested = DrawInspectorAddTabButton(options.add_tab_id, options.add_tooltip);
		}

		ApplyInspectorTabBarHorizontalWheel();
		ImGui::EndTabBar();
	}
	return add_requested;
}

struct InspectorTabContextResult {
	bool rename_requested{ false };
	bool duplicate_requested{ false };
	bool remove_requested{ false };
};

inline InspectorTabContextResult DrawInspectorTabContextMenu(
	const char* popup_id,
	bool allow_rename = true,
	bool allow_duplicate = false,
	bool allow_remove = true,
	std::string_view remove_label = "Remove"
) {
	InspectorTabContextResult result;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImGui::GetStyle().WindowPadding);
	if (!ImGui::BeginPopupContextItem(popup_id)) {
		ImGui::PopStyleVar();
		return result;
	}

	if (allow_rename && ImGui::MenuItem("Rename")) {
		result.rename_requested = true;
	}
	if (allow_duplicate && ImGui::MenuItem("Duplicate")) {
		result.duplicate_requested = true;
	}
	if ((allow_rename || allow_duplicate) && allow_remove) {
		ImGui::Separator();
	}
	if (allow_remove) {
		const std::string label{ remove_label };
		if (ImGui::MenuItem(label.c_str())) {
			result.remove_requested = true;
		}
	}
	ImGui::EndPopup();
	ImGui::PopStyleVar();
	return result;
}

template <typename Validate, typename Commit>
RenameResult DrawInspectorTabRenameModal(
	RenameModalState& state,
	const char* popup_id,
	const char* input_id,
	Validate&& validate,
	Commit&& commit,
	std::string_view title = "Rename Tab"
) {
	std::string title_string{ title };
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImGui::GetStyle().WindowPadding);
	const RenameResult result{ DrawRenameModal(
		state,
		popup_id,
		input_id,
		std::forward<Validate>(validate),
		std::forward<Commit>(commit),
		RenameModalOptions{ .title = title_string.c_str() }
	) };
	ImGui::PopStyleVar();
	return result;
}

} // namespace ptgn::editor::inspector
