#include "panels/viewport.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/editor_state.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/viewport.h"

namespace ptgn::editor {

void ViewportPanel::OnRender(EditorContext& ctx) {
	// Remove padding.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
	ImGui::Begin(
		"Game", nullptr,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
	);
	ImGui::PopStyleVar();

	// Remove docking tab at the top.
	if (ImGuiWindow* game_window = ImGui::FindWindowByName("Game")) {
		if (game_window->DockNode) {
			game_window->DockNode->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
		}
	}

	ImVec2 min{ ImGui::GetCursorScreenPos() };
	ImVec2 size{ ImGui::GetContentRegionAvail() };
	ImVec2 max{ min.x + size.x, min.y + size.y };
	ImVec2 center{ (min.x + max.x) / 2.0f, (min.y + max.y) / 2.0f };

	if (size.x <= 0.0f || size.y <= 0.0f) {
		ImGui::End();
		return;
	}

	ctx.state.viewport.viewport = { { min.x, min.y }, { size.x, size.y } };
	ctx.state.viewport.focused	= ImGui::IsWindowFocused();
	ctx.state.viewport.hovered	= ImGui::IsWindowHovered();

	auto* draw_list = ImGui::GetWindowDrawList();

	draw_list->AddRectFilled(
		min, max,
		// TODO: Use renderer clear color.
		IM_COL32(255, 0, 0, 255)
	);

	auto screen_texture{ ctx.editor.GetScreenTargetTexture() };

	auto display_size{ ctx.editor.GetDisplaySize() };

	auto half_display{ display_size / 2.0f };
	ImVec2 img_min{ center.x - half_display.x, center.y - half_display.y };
	ImVec2 img_max{ center.x + half_display.x, center.y + half_display.y };

	draw_list->AddImage(
		screen_texture, img_min, img_max, ImVec2{ 0.0f, 1.0f }, ImVec2{ 1.0f, 0.0f }
	);

	ImGui::End();
}

} // namespace ptgn::editor