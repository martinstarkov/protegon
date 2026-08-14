#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstddef>
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
	if (!selectable_output.empty() && selectable_output.back() == '\n') {
		selectable_output.pop_back();
	}

	const auto& style{ ImGui::GetStyle() };
	const float line_height{ ImGui::GetTextLineHeight() };

	std::size_t line_count{ 1 };
	float maximum_line_width{ 0.0f };

	const char* line_begin{ selectable_output.data() };
	const char* const text_end{ selectable_output.data() + selectable_output.size() };

	for (const char* cursor{ line_begin };; ++cursor) {
		if (cursor == text_end || *cursor == '\n') {
			maximum_line_width = std::max(
				maximum_line_width,
				ImGui::CalcTextSize(line_begin, cursor, false).x
			);

			if (cursor == text_end) {
				break;
			}

			++line_count;
			line_begin = cursor + 1;
		}
	}

	const ImVec2 available_size{ ImGui::GetContentRegionAvail() };
	const float text_width{
		maximum_line_width + style.FramePadding.x * 2.0f + 1.0f
	};
	const float text_height{
		static_cast<float>(line_count) * line_height + style.FramePadding.y * 2.0f
	};
	const float input_width{ std::max(available_size.x, text_width) };
	const float input_height{
		std::max(line_height + style.FramePadding.y * 2.0f, text_height)
	};

	ImGui::SetNextWindowContentSize(ImVec2{ input_width, input_height });
	ImGui::PushStyleColor(ImGuiCol_ChildBg, style.Colors[ImGuiCol_FrameBg]);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.FrameRounding);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, style.FrameBorderSize);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });

	if (ImGui::BeginChild(
			"OutputRegion",
			ImVec2{ 0.0f, 0.0f },
			ImGuiChildFlags_Borders,
			ImGuiWindowFlags_HorizontalScrollbar
		)) {
		ImGuiWindow* output_window{ ImGui::GetCurrentWindow() };
		ImGuiContext& imgui_context{ *GImGui };
		const auto& io{ ImGui::GetIO() };
		constexpr float kBottomTolerance{ 2.0f };

		const ImGuiID vertical_scrollbar_id{
			ImGui::GetWindowScrollbarID(output_window, ImGuiAxis_Y)
		};
		const bool output_hovered{
			ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)
		};
		const bool user_scrolled_up_with_wheel{
			output_hovered && io.MouseWheel > 0.0f
		};
		const bool user_dragging_vertical_scrollbar{
			imgui_context.ActiveId == vertical_scrollbar_id
		};

		if (!jump_to_bottom_requested &&
			(user_scrolled_up_with_wheel || user_dragging_vertical_scrollbar)) {
			follow_tail = false;
		}

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

		ImGui::InputTextMultiline(
			"##OutputText",
			selectable_output.data(),
			selectable_output.size() + 1,
			ImVec2{ input_width, input_height },
			ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoHorizontalScroll
		);

		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();

		const bool at_bottom{
			output_window->Scroll.y >= output_window->ScrollMax.y - kBottomTolerance
		};

		if (!follow_tail &&
			!user_dragging_vertical_scrollbar &&
			!user_scrolled_up_with_wheel &&
			at_bottom) {
			follow_tail = true;
		}

		if (follow_tail || jump_to_bottom_requested) {
			output_window->Scroll.y = output_window->ScrollMax.y;
			ImGui::SetScrollY(output_window, output_window->ScrollMax.y);
			follow_tail = true;
			jump_to_bottom_requested = false;
		}
	}

	ImGui::EndChild();
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor();
	ImGui::PopID();
	return actions;
}

} // namespace ptgn::editor
