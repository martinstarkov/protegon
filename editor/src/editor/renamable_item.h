#pragma once

#include <imgui.h>
#include <imgui_stdlib.h>

#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace ptgn::editor {

enum class RenameResult {
	None,
	Committed,
	Cancelled,
};

/// @brief Mutable text/focus/error storage used by both inline and modal rename controls.
///
/// This view is useful when an existing panel already owns the three fields separately and you
/// do not want to change that panel's stored state just to use the shared rename helpers.
struct RenameEditStateRef {
	bool& focus_requested;
	std::string& value;
	std::string& error;
};

/// @brief Shared state for an in-place rename control.
///
/// The edited value stays separate from the model until Enter is pressed. Escape and clicking
/// outside the input cancel the edit.
struct InlineRenameState {
	bool active{ false };
	bool focus_requested{ false };
	std::string original{};
	std::string value{};
	std::string error{};

	void Begin(std::string_view current) {
		active = true;
		focus_requested = true;
		original = current;
		value = current;
		error.clear();
	}

	void Cancel() {
		active = false;
		focus_requested = false;
		original.clear();
		value.clear();
		error.clear();
	}
};

/// @brief Shared state for a non-inline rename modal.
///
/// Begin() requests a centered ImGui modal. The edited value remains detached from the model until
/// Rename or Enter confirms a valid value. Cancel, Escape, and clicking outside the dialog cancel
/// without mutating the model. The commit callback owns the actual undoable model mutation.
struct RenameModalState {
	bool active{ false };
	bool open_requested{ false };
	bool focus_requested{ false };
	std::string original{};
	std::string value{};
	std::string error{};

	void Begin(std::string_view current) {
		active = true;
		open_requested = true;
		focus_requested = true;
		original = current;
		value = current;
		error.clear();
	}

	void Cancel() {
		active = false;
		open_requested = false;
		focus_requested = false;
		original.clear();
		value.clear();
		error.clear();
	}
};

[[nodiscard]] inline RenameEditStateRef BindRenameState(InlineRenameState& state) {
	return RenameEditStateRef{ state.focus_requested, state.value, state.error };
}

[[nodiscard]] inline RenameEditStateRef BindRenameState(RenameModalState& state) {
	return RenameEditStateRef{ state.focus_requested, state.value, state.error };
}

// Compatibility name for code written against the first version of the shared rename helper.
using RenamePopupState = RenameModalState;

inline void BeginRename(RenameEditStateRef state, std::string_view current) {
	state.focus_requested = true;
	state.value = current;
	state.error.clear();
}

inline void CancelRename(RenameEditStateRef state) {
	state.focus_requested = false;
	state.value.clear();
	state.error.clear();
}

inline void DrawRenameError(std::string_view error) {
	if (error.empty()) {
		return;
	}

	ImGui::GetWindowDrawList()->AddRect(
		ImGui::GetItemRectMin(),
		ImGui::GetItemRectMax(),
		ImGui::GetColorU32(ImVec4{ 1.0f, 0.20f, 0.20f, 1.0f }),
		ImGui::GetStyle().FrameRounding,
		0,
		1.5f
	);

	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%.*s", static_cast<int>(error.size()), error.data());
	}
}

/// @brief Draw the text input used by rename controls.
///
/// validate(value) returns an empty string for a valid value and an error string otherwise.
/// commit(value) is invoked only for a valid Enter submission. Losing focus never commits: it
/// returns Cancelled, which makes clicking elsewhere safe.
template <typename Validate, typename Commit>
RenameResult DrawRenameInput(
	RenameEditStateRef state,
	const char* id,
	float width,
	Validate&& validate,
	Commit&& commit,
	ImGuiInputTextFlags extra_flags = ImGuiInputTextFlags_None,
	bool cancel_on_deactivate = true
) {
	if (state.focus_requested) {
		ImGui::SetKeyboardFocusHere();
		state.focus_requested = false;
	}

	ImGui::SetNextItemWidth(width);
	const bool submitted{ ImGui::InputText(
		id,
		&state.value,
		extra_flags |
			ImGuiInputTextFlags_EnterReturnsTrue |
			ImGuiInputTextFlags_AutoSelectAll
	) };
	const bool edited{ ImGui::IsItemEdited() };

	if (edited) {
		state.error = std::invoke(validate, std::string_view{ state.value });
	}
	DrawRenameError(state.error);

	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
		return RenameResult::Cancelled;
	}

	if (submitted) {
		state.error = std::invoke(validate, std::string_view{ state.value });
		if (!state.error.empty()) {
			state.focus_requested = true;
			return RenameResult::None;
		}

		std::invoke(std::forward<Commit>(commit), std::string_view{ state.value });
		return RenameResult::Committed;
	}

	// Clicking elsewhere deactivates the input. Renames are intentionally never committed by
	// focus loss; Enter is the only commit gesture.
	if (cancel_on_deactivate && ImGui::IsItemDeactivated()) {
		return RenameResult::Cancelled;
	}

	return RenameResult::None;
}

/// @brief Draw a reusable in-place rename field.
template <typename Validate, typename Commit>
RenameResult DrawInlineRename(
	InlineRenameState& state,
	const char* id,
	float width,
	Validate&& validate,
	Commit&& commit,
	ImGuiInputTextFlags extra_flags = ImGuiInputTextFlags_None
) {
	if (!state.active) {
		return RenameResult::None;
	}

	const RenameResult result{ DrawRenameInput(
		BindRenameState(state),
		id,
		width,
		std::forward<Validate>(validate),
		std::forward<Commit>(commit),
		extra_flags
	) };

	if (result != RenameResult::None) {
		state.Cancel();
	}
	return result;
}

struct RenameModalOptions {
	float width{ 320.0f };
	float button_width{ 92.0f };
	const char* title{ "Rename" };
	const char* rename_label{ "Rename" };
	const char* cancel_label{ "Cancel" };
	ImGuiWindowFlags window_flags{ ImGuiWindowFlags_AlwaysAutoResize };
	ImGuiInputTextFlags input_flags{ ImGuiInputTextFlags_None };
	bool cancel_on_outside_click{ true };

	/// @brief Optional content drawn immediately to the left of the rename input.
	///
	/// The callback should finish by calling ImGui::SameLine() when the following input should
	/// remain on the same row. This is useful for contextual previews such as a color swatch.
	std::function<void()> draw_input_prefix{};
};

/// @brief Draw a centered rename modal suitable for non-inline rename flows.
///
/// Rename/Enter validates then commits. Cancel/Escape dismiss without mutation. When enabled, a
/// click outside the dialog is treated as Cancel even though BeginPopupModal normally blocks it.
/// The commit callback is deliberately the only place the model is changed, allowing callers to
/// route the operation through EditorCommands or UndoStack exactly once.
template <typename Validate, typename Commit>
RenameResult DrawRenameModal(
	RenameModalState& state,
	const char* popup_id,
	const char* input_id,
	Validate&& validate,
	Commit&& commit,
	RenameModalOptions options = {}
) {
	if (!state.active) {
		return RenameResult::None;
	}

	if (state.open_requested) {
		ImGui::OpenPopup(popup_id);
		state.open_requested = false;
	}

	if (!ImGui::IsPopupOpen(popup_id)) {
		state.Cancel();
		return RenameResult::Cancelled;
	}

	if (const ImGuiViewport* viewport{ ImGui::GetMainViewport() }) {
		ImGui::SetNextWindowPos(
			viewport->GetCenter(),
			ImGuiCond_Appearing,
			ImVec2{ 0.5f, 0.5f }
		);
	}

	RenameResult result{ RenameResult::None };
	if (!ImGui::BeginPopupModal(popup_id, nullptr, options.window_flags)) {
		return result;
	}
	const bool window_appearing{ ImGui::IsWindowAppearing() };

	if (options.title && options.title[0] != '\0') {
		ImGui::TextUnformatted(options.title);
		ImGui::Separator();
	}

	if (options.draw_input_prefix) {
		std::invoke(options.draw_input_prefix);
	}

	// Set focus after any prefix content so the rename text field, rather than the prefix item,
	// receives keyboard focus when the modal opens or validation fails.
	if (state.focus_requested) {
		ImGui::SetKeyboardFocusHere();
		state.focus_requested = false;
	}

	ImGui::SetNextItemWidth(options.width);
	const bool enter_pressed{ ImGui::InputText(
		input_id,
		&state.value,
		options.input_flags |
			ImGuiInputTextFlags_EnterReturnsTrue |
			ImGuiInputTextFlags_AutoSelectAll
	) };
	const bool edited{ ImGui::IsItemEdited() };

	if (edited) {
		state.error = std::invoke(validate, std::string_view{ state.value });
	}
	DrawRenameError(state.error);

	if (!state.error.empty()) {
		ImGui::TextColored(
			ImVec4{ 1.0f, 0.35f, 0.35f, 1.0f },
			"%s",
			state.error.c_str()
		);
	}

	ImGui::Spacing();

	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float total_button_width{ options.button_width * 2.0f + spacing };
	const float available{ ImGui::GetContentRegionAvail().x };
	if (available > total_button_width) {
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available - total_button_width);
	}

	const bool cancel_pressed{
		ImGui::Button(options.cancel_label, ImVec2{ options.button_width, 0.0f })
	};
	ImGui::SameLine();
	const bool rename_pressed{
		ImGui::Button(options.rename_label, ImVec2{ options.button_width, 0.0f })
	};

	const bool escape_pressed{ ImGui::IsKeyPressed(ImGuiKey_Escape, false) };

	bool outside_clicked{ false };
	if (options.cancel_on_outside_click && !window_appearing &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		const ImVec2 mouse{ ImGui::GetIO().MousePos };
		const ImVec2 window_min{ ImGui::GetWindowPos() };
		const ImVec2 window_max{
			window_min.x + ImGui::GetWindowSize().x,
			window_min.y + ImGui::GetWindowSize().y
		};
		outside_clicked = mouse.x < window_min.x || mouse.y < window_min.y ||
			mouse.x >= window_max.x || mouse.y >= window_max.y;
	}

	if (cancel_pressed || escape_pressed || outside_clicked) {
		result = RenameResult::Cancelled;
	} else if (rename_pressed || enter_pressed) {
		state.error = std::invoke(validate, std::string_view{ state.value });
		if (state.error.empty()) {
			std::invoke(std::forward<Commit>(commit), std::string_view{ state.value });
			result = RenameResult::Committed;
		} else {
			state.focus_requested = true;
		}
	}

	if (result != RenameResult::None) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();

	if (result != RenameResult::None) {
		state.Cancel();
	}
	return result;
}

// Compatibility aliases for the earlier non-modal API names. They now use modal semantics.
using RenamePopupOptions = RenameModalOptions;

template <typename Validate, typename Commit>
RenameResult DrawRenamePopup(
	RenamePopupState& state,
	const char* popup_id,
	const char* input_id,
	Validate&& validate,
	Commit&& commit,
	RenamePopupOptions options = {}
) {
	return DrawRenameModal(
		state, popup_id, input_id,
		std::forward<Validate>(validate),
		std::forward<Commit>(commit),
		std::move(options)
	);
}

/// @brief Interaction snapshot for a normal item or a renamable tree node.
struct RenamableItemResult {
	ImGuiID item_id{};
	bool hovered{ false };
	bool left_clicked{ false };
	bool right_clicked{ false };
	bool context_requested{ false };

	// Tree-node specific. For a generic item this remains false.
	bool open{ false };
	bool editing{ false };
	RenameResult rename_result{ RenameResult::None };
};

[[nodiscard]] inline RenamableItemResult CaptureRenamableItemResult() {
	RenamableItemResult result;
	result.item_id = ImGui::GetItemID();
	result.hovered = ImGui::IsItemHovered();
	result.left_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
	result.right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
	result.context_requested =
		result.hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right);
	return result;
}

/// @brief Draw any clickable ImGui item and capture enough interaction state to attach a reusable
/// right-click rename/context menu to it.
///
/// draw_item must leave the item to be renamed as the current/last ImGui item when it returns.
template <typename DrawItem>
RenamableItemResult DrawRenamableItem(DrawItem&& draw_item) {
	std::invoke(std::forward<DrawItem>(draw_item));
	return CaptureRenamableItemResult();
}

struct RenamableTreeNodeOptions {
	float rename_width{ -1.0f };
	const char* rename_input_id{ "##Rename" };
	ImGuiInputTextFlags rename_input_flags{ ImGuiInputTextFlags_None };
};

/// @brief Draw a tree node that can swap itself for the shared inline rename input.
///
/// Whether this particular node is currently being renamed is supplied by the caller so a single
/// rename buffer can be shared across a whole hierarchy while the caller keeps its own stable
/// entity/asset identity for the rename target.
template <typename Validate, typename Commit>
RenamableItemResult DrawRenamableTreeNode(
	bool editing,
	RenameEditStateRef rename_state,
	const char* node_id,
	std::string_view label,
	ImGuiTreeNodeFlags flags,
	Validate&& validate,
	Commit&& commit,
	RenamableTreeNodeOptions options = {}
) {
	if (editing) {
		RenamableItemResult result;
		result.editing = true;
		result.rename_result = DrawRenameInput(
			rename_state,
			options.rename_input_id,
			options.rename_width,
			std::forward<Validate>(validate),
			std::forward<Commit>(commit),
			options.rename_input_flags
		);

		const RenamableItemResult interaction{ CaptureRenamableItemResult() };
		result.item_id = interaction.item_id;
		result.hovered = interaction.hovered;
		result.left_clicked = interaction.left_clicked;
		result.right_clicked = interaction.right_clicked;
		return result;
	}

	RenamableItemResult result;
	result.open = ImGui::TreeNodeEx(
		node_id,
		flags,
		"%.*s",
		static_cast<int>(label.size()),
		label.data()
	);

	const RenamableItemResult interaction{ CaptureRenamableItemResult() };
	result.item_id = interaction.item_id;
	result.hovered = interaction.hovered;
	result.left_clicked = interaction.left_clicked;
	result.right_clicked = interaction.right_clicked;
	result.context_requested = interaction.context_requested;
	return result;
}

enum class RenameMenuPlacement {
	First,
	Middle,
	Last,
};

struct RenamableContextMenuOptions {
	const char* rename_label{ "Rename" };
	bool show_rename{ true };
	bool rename_enabled{ true };
	RenameMenuPlacement rename_placement{ RenameMenuPlacement::First };
};

/// @brief Attach a right-click context menu to a result returned by DrawRenamableItem or
/// DrawRenamableTreeNode.
///
/// begin_rename(current_value) chooses the rename presentation: call InlineRenameState::Begin for
/// in-place editing or RenameModalState::Begin for a modal. The before/after callbacks make the
/// menu fully configurable without requiring the caller to reimplement the rename entry.
template <typename BeginRename, typename DrawBeforeRename, typename DrawAfterRename>
bool DrawRenamableContextMenu(
	const RenamableItemResult& item,
	const char* popup_id,
	std::string_view current_value,
	BeginRename&& begin_rename,
	DrawBeforeRename&& draw_before_rename,
	DrawAfterRename&& draw_after_rename,
	RenamableContextMenuOptions options = {}
) {
	if (item.context_requested) {
		ImGui::OpenPopup(popup_id);
	}

	if (!ImGui::BeginPopup(popup_id)) {
		return false;
	}

	auto draw_rename_item = [&]() {
		if (!options.show_rename) {
			return;
		}

		ImGui::BeginDisabled(!options.rename_enabled);
		if (ImGui::MenuItem(options.rename_label)) {
			std::invoke(begin_rename, current_value);
		}
		ImGui::EndDisabled();
	};

	if (options.rename_placement == RenameMenuPlacement::First) {
		draw_rename_item();
	}

	std::invoke(std::forward<DrawBeforeRename>(draw_before_rename));

	if (options.rename_placement == RenameMenuPlacement::Middle) {
		draw_rename_item();
	}

	std::invoke(std::forward<DrawAfterRename>(draw_after_rename));

	if (options.rename_placement == RenameMenuPlacement::Last) {
		draw_rename_item();
	}

	ImGui::EndPopup();
	return true;
}

/// @brief Convenience overload for the common case where all custom menu items come after Rename.
template <typename BeginRename, typename DrawExtraMenu>
bool DrawRenamableContextMenu(
	const RenamableItemResult& item,
	const char* popup_id,
	std::string_view current_value,
	BeginRename&& begin_rename,
	DrawExtraMenu&& draw_extra_menu,
	RenamableContextMenuOptions options = {}
) {
	return DrawRenamableContextMenu(
		item,
		popup_id,
		current_value,
		std::forward<BeginRename>(begin_rename),
		[]() {},
		std::forward<DrawExtraMenu>(draw_extra_menu),
		options
	);
}

} // namespace ptgn::editor
