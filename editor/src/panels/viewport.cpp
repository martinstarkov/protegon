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
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });

	constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoScrollbar |
										ImGuiWindowFlags_NoScrollWithMouse |
										ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;

	ImGui::Begin("Game", nullptr, kFlags);
	ImGui::PopStyleVar();

	if (ImGuiWindow* game_window = ImGui::FindWindowByName("Game")) {
		if (game_window->DockNode) {
			game_window->DockNode->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
		}
	}

	const ImVec2 min   = ImGui::GetCursorScreenPos();
	const ImVec2 avail = ImGui::GetContentRegionAvail();
	const ImVec2 max{ min.x + avail.x, min.y + avail.y };
	const ImVec2 center{ min.x + avail.x / 2.0f, min.y + avail.y / 2.0f };

	const Viewport viewport{ .position{ min.x, min.y }, .size{ avail.x, avail.y } };

	ctx.state.viewport.viewport = viewport;
	ctx.state.viewport.focused	= ImGui::IsWindowFocused();
	ctx.state.viewport.hovered	= ImGui::IsWindowHovered();

	ctx.editor.SetPresentationViewport(viewport);

	if (avail.x <= 0.0f || avail.y <= 0.0f) {
		ImGui::End();
		return;
	}

	auto* draw_list = ImGui::GetWindowDrawList();

	auto bg{ ctx.editor.GetWindowBackgroundColor() };

	draw_list->AddRectFilled(min, max, IM_COL32(bg.r, bg.g, bg.b, bg.a));

	const auto display_viewport = ctx.editor.GetDisplayViewport();
	const auto screen_texture	= ctx.editor.GetScreenTargetTexture();

	const ImVec2 img_min{ min.x + static_cast<float>(display_viewport.position.x),
						  min.y + static_cast<float>(display_viewport.position.y) };

	const ImVec2 img_max{
		min.x + static_cast<float>(display_viewport.position.x + display_viewport.size.x),
		min.y + static_cast<float>(display_viewport.position.y + display_viewport.size.y)
	};

	draw_list->AddImage(
		static_cast<ImTextureID>(screen_texture), img_min, img_max, ImVec2{ 0.0f, 1.0f },
		ImVec2{ 1.0f, 0.0f }
	);

	ImGui::End();
}

} // namespace ptgn::editor