#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <cfloat>
#include <string>

namespace ptgn::editor {

struct OutputConsoleActions {
	bool clear_requested{ false };
	bool save_requested{ false };
};

[[nodiscard]] inline OutputConsoleActions DrawOutputConsole(
	const char* id,
	std::string& output,
	bool& follow_tail,
	bool& jump_to_bottom_requested,
	bool show_save_button = false
) {
	OutputConsoleActions actions;

	ImGui::PushID(id);

	if (ImGui::Button("Clear Output")) {
		output.clear();
		follow_tail = true;
		jump_to_bottom_requested = true;
		actions.clear_requested = true;
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(output.empty());

	if (ImGui::Button("Copy All")) {
		ImGui::SetClipboardText(output.c_str());
	}

	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(output.empty());

	if (ImGui::Button("Jump to Bottom")) {
		follow_tail = true;
		jump_to_bottom_requested = true;
	}

	ImGui::EndDisabled();

	if (show_save_button) {
		ImGui::SameLine();
		ImGui::BeginDisabled(output.empty());

		if (ImGui::Button("Save Log")) {
			actions.save_requested = true;
		}

		ImGui::EndDisabled();
	}

	std::string selectable_output{ output };

	if (!selectable_output.empty() &&
		selectable_output.back() == '\n') {
		selectable_output.pop_back();
	}

	ImGuiWindow* parent_window{
		ImGui::GetCurrentWindow()
	};

	const ImGuiID output_id{
		parent_window->GetID("##OutputText")
	};

	ImGui::InputTextMultiline(
		"##OutputText",
		selectable_output.data(),
		selectable_output.size() + 1,
		ImGui::GetContentRegionAvail(),
		ImGuiInputTextFlags_ReadOnly |
			ImGuiInputTextFlags_NoHorizontalScroll |
			ImGuiInputTextFlags_WordWrap
	);

	ImGuiWindow* output_window{ nullptr };

	for (ImGuiWindow* child : parent_window->DC.ChildWindows) {
		if (child && child->ChildId == output_id) {
			output_window = child;
			break;
		}
	}

	if (output_window) {
		ImGuiContext& imgui_context{ *GImGui };
		const auto& io{ ImGui::GetIO() };

		constexpr float kBottomTolerance{ 2.0f };

		const ImGuiID vertical_scrollbar_id{
			ImGui::GetWindowScrollbarID(
				output_window,
				ImGuiAxis_Y
			)
		};

		const bool output_hovered{
			imgui_context.HoveredWindow == output_window
		};

		const bool user_scrolled_up_with_wheel{
			output_hovered &&
			io.MouseWheel > 0.0f
		};

		const bool user_dragging_vertical_scrollbar{
			imgui_context.ActiveId ==
				vertical_scrollbar_id
		};

		if (
			!jump_to_bottom_requested &&
			(
				user_scrolled_up_with_wheel ||
				user_dragging_vertical_scrollbar
			)
		) {
			follow_tail = false;
		}

		const bool at_bottom{
			output_window->ScrollMax.y <=
				kBottomTolerance ||
			output_window->Scroll.y >=
				output_window->ScrollMax.y -
					kBottomTolerance
		};

		if (
			!follow_tail &&
			!user_dragging_vertical_scrollbar &&
			!user_scrolled_up_with_wheel &&
			at_bottom
		) {
			follow_tail = true;
		}

		if (follow_tail || jump_to_bottom_requested) {
			output_window->Scroll.y =
				output_window->ScrollMax.y;

			ImGui::SetScrollY(
				output_window,
				FLT_MAX
			);

			follow_tail = true;
			jump_to_bottom_requested = false;
		}
	}

	ImGui::PopID();

	return actions;
}

} // namespace ptgn::editor