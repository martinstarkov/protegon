#include "editor/editor_icons.h"

#include <algorithm>

namespace ptgn::editor {

void DrawEditorIcon(ImDrawList* draw, EditorIcon icon, ImVec2 min, float extent, ImU32 color) {
	const float scale{ extent / 16.0f };
	const auto point = [&](float x, float y) {
		return ImVec2{
			min.x + x * scale,
			min.y + y * scale,
		};
	};

	switch (icon) {
		case EditorIcon::Play:
			draw->AddTriangleFilled(
				point(4.0f, 2.0f), point(4.0f, 14.0f), point(13.0f, 8.0f), color
			);
			break;

		case EditorIcon::Stop:
			draw->AddRectFilled(point(3.0f, 3.0f), point(13.0f, 13.0f), color, 1.0f);
			break;

		case EditorIcon::Pause:
			draw->AddRectFilled(point(3.0f, 2.5f), point(6.5f, 13.5f), color, 0.75f);
			draw->AddRectFilled(point(9.5f, 2.5f), point(13.0f, 13.5f), color, 0.75f);
			break;

		case EditorIcon::Reset: {
			const ImVec2 center{ point(8.0f, 8.0f) };
			const float radius{ 5.0f * scale };
			const float thickness{ std::max(1.0f, 1.5f * scale) };
			draw->PathArcTo(
				center, radius, -0.15f * IM_PI, 1.55f * IM_PI, 20
			);
			draw->PathStroke(color, 0, thickness);
			draw->AddTriangleFilled(
				point(3.1f, 2.7f), point(7.0f, 2.9f), point(4.4f, 6.0f), color
			);
			break;
		}

		case EditorIcon::StepForward:
			draw->AddTriangleFilled(
				point(2.5f, 2.5f), point(2.5f, 13.5f), point(10.5f, 8.0f), color
			);
			draw->AddRectFilled(point(11.5f, 2.5f), point(13.5f, 13.5f), color);
			break;

		case EditorIcon::StepBackward:
			draw->AddRectFilled(point(2.5f, 2.5f), point(4.5f, 13.5f), color);
			draw->AddTriangleFilled(
				point(13.5f, 2.5f), point(13.5f, 13.5f), point(5.5f, 8.0f), color
			);
			break;

		case EditorIcon::Camera:
			// View-cone/frustum symbol rather than a literal camera body.
			draw->AddCircleFilled(point(3.0f, 8.0f), 1.35f * scale, color, 10);
			draw->AddLine(
				point(4.2f, 7.2f), point(13.2f, 2.8f), color, std::max(1.0f, 1.5f * scale)
			);
			draw->AddLine(
				point(4.2f, 8.8f), point(13.2f, 13.2f), color, std::max(1.0f, 1.5f * scale)
			);
			draw->AddLine(
				point(13.2f, 2.8f), point(13.2f, 13.2f), color, std::max(1.0f, 1.5f * scale)
			);
			break;

	}
}

bool DrawEditorIconButton(const char* id, EditorIcon icon, const char* tooltip) {
	const auto& style{ ImGui::GetStyle() };
	const float side{ ImGui::GetFrameHeight() };
	const ImVec2 p0{ ImGui::GetCursorScreenPos() };

	ImGui::InvisibleButton(id, { side, side });

	const bool hovered{ ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) };
	const bool active{ ImGui::IsItemActive() };
	const bool pressed{ ImGui::IsItemClicked(ImGuiMouseButton_Left) };

	const ImVec4 background{ active	   ? style.Colors[ImGuiCol_ButtonActive]
							 : hovered ? style.Colors[ImGuiCol_ButtonHovered]
									   : style.Colors[ImGuiCol_Button] };

	auto* draw{ ImGui::GetWindowDrawList() };
	draw->AddRectFilled(
		p0, { p0.x + side, p0.y + side }, ImGui::GetColorU32(background), style.FrameRounding
	);

	const float icon_extent{ std::clamp(side - 10.0f, 12.0f, 16.0f) };
	DrawEditorIcon(
		draw, icon,
		{
			p0.x + (side - icon_extent) * 0.5f,
			p0.y + (side - icon_extent) * 0.5f,
		},
		icon_extent, ImGui::GetColorU32(ImGuiCol_Text)
	);

	if (hovered && tooltip && *tooltip != '\0') {
		ImGui::SetTooltip("%s", tooltip);
	}

	return pressed;
}

} // namespace ptgn::editor
