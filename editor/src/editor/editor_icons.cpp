#include "editor/editor_icons.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>

namespace ptgn::editor {

void DrawEditorIcon(ImDrawList* draw, EditorIcon icon, ImVec2 min, float extent, ImU32 color) {
	const float scale{ extent / 16.0f };
	const float x{ std::floor(min.x) };
	const float y{ std::floor(min.y) };
	const auto point = [&](float px, float py) {
		return ImVec2{
			x + px * scale,
			y + py * scale,
		};
	};
	const float line{ std::max(1.0f, 1.6f * scale) };

	// The hierarchy icons were originally authored in an 18x18 square. Keep that
	// geometry so moving them here does not subtly change their appearance.
	const float hierarchy_scale{ extent / 18.0f };
	const auto hierarchy_point = [&](float px, float py) {
		return ImVec2{
			x + px * hierarchy_scale,
			y + py * hierarchy_scale,
		};
	};
	const float hierarchy_line{ std::max(1.0f, 1.5f * hierarchy_scale) };

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
			draw->PathArcTo(center, radius, -0.15f * IM_PI, 1.55f * IM_PI, 20);
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
			draw->AddCircleFilled(point(3.0f, 8.0f), 1.35f * scale, color, 10);
			draw->AddLine(point(4.2f, 7.2f), point(13.2f, 2.8f), color, line);
			draw->AddLine(point(4.2f, 8.8f), point(13.2f, 13.2f), color, line);
			draw->AddLine(point(13.2f, 2.8f), point(13.2f, 13.2f), color, line);
			break;

		case EditorIcon::Entity:
			draw->AddQuad(
				hierarchy_point(9.0f, 2.0f), hierarchy_point(15.0f, 6.0f),
				hierarchy_point(9.0f, 10.0f), hierarchy_point(3.0f, 6.0f), color,
				hierarchy_line
			);
			draw->AddLine(
				hierarchy_point(3.0f, 6.0f), hierarchy_point(3.0f, 12.0f), color,
				hierarchy_line
			);
			draw->AddLine(
				hierarchy_point(15.0f, 6.0f), hierarchy_point(15.0f, 12.0f), color,
				hierarchy_line
			);
			draw->AddLine(
				hierarchy_point(3.0f, 12.0f), hierarchy_point(9.0f, 16.0f), color,
				hierarchy_line
			);
			draw->AddLine(
				hierarchy_point(15.0f, 12.0f), hierarchy_point(9.0f, 16.0f), color,
				hierarchy_line
			);
			draw->AddLine(
				hierarchy_point(9.0f, 10.0f), hierarchy_point(9.0f, 16.0f), color,
				hierarchy_line
			);
			break;

		case EditorIcon::Tile:
			for (int row{}; row < 2; ++row) {
				for (int column{}; column < 2; ++column) {
					draw->AddRect(
						hierarchy_point(
							3.0f + static_cast<float>(column) * 6.0f,
							3.0f + static_cast<float>(row) * 6.0f
						),
						hierarchy_point(
							8.0f + static_cast<float>(column) * 6.0f,
							8.0f + static_cast<float>(row) * 6.0f
						),
						color
					);
				}
			}
			break;

		case EditorIcon::Visible:
		case EditorIcon::Hidden:
			draw->AddLine(
				hierarchy_point(2.0f, 9.0f), hierarchy_point(6.0f, 5.0f), color,
				hierarchy_line
			);
			draw->AddLine(
				hierarchy_point(6.0f, 5.0f), hierarchy_point(12.0f, 5.0f), color,
				hierarchy_line
			);
			draw->AddLine(
				hierarchy_point(12.0f, 5.0f), hierarchy_point(16.0f, 9.0f), color,
				hierarchy_line
			);
			draw->AddLine(
				hierarchy_point(16.0f, 9.0f), hierarchy_point(12.0f, 13.0f), color,
				hierarchy_line
			);
			draw->AddLine(
				hierarchy_point(12.0f, 13.0f), hierarchy_point(6.0f, 13.0f), color,
				hierarchy_line
			);
			draw->AddLine(
				hierarchy_point(6.0f, 13.0f), hierarchy_point(2.0f, 9.0f), color,
				hierarchy_line
			);
			if (icon == EditorIcon::Visible) {
				draw->AddCircleFilled(
					hierarchy_point(9.0f, 9.0f), 2.3f * hierarchy_scale, color, 8
				);
			} else {
				draw->AddLine(
					hierarchy_point(3.0f, 15.0f), hierarchy_point(15.0f, 3.0f), color,
					hierarchy_line
				);
			}
			break;

		case EditorIcon::Locked:
		case EditorIcon::Unlocked:
			draw->AddRect(
				hierarchy_point(5.0f, 8.0f), hierarchy_point(14.0f, 15.0f), color,
				1.0f * hierarchy_scale, 0, hierarchy_line
			);
			draw->AddLine(
				hierarchy_point(7.0f, 8.0f), hierarchy_point(7.0f, 5.0f), color,
				hierarchy_line
			);
			if (icon == EditorIcon::Locked) {
				draw->AddLine(
					hierarchy_point(7.0f, 5.0f), hierarchy_point(12.0f, 5.0f), color,
					hierarchy_line
				);
				draw->AddLine(
					hierarchy_point(12.0f, 5.0f), hierarchy_point(12.0f, 8.0f), color,
					hierarchy_line
				);
			} else {
				draw->AddLine(
					hierarchy_point(7.0f, 5.0f), hierarchy_point(11.0f, 4.0f), color,
					hierarchy_line
				);
			}
			break;

		case EditorIcon::Select:
			draw->AddTriangleFilled(
				point(2.0f, 1.5f), point(2.0f, 13.5f), point(6.3f, 9.7f), color
			);
			draw->AddLine(point(6.0f, 9.2f), point(10.5f, 14.0f), color, line);
			break;

		case EditorIcon::Move:
			draw->AddLine(point(8.0f, 2.0f), point(8.0f, 14.0f), color, line);
			draw->AddLine(point(2.0f, 8.0f), point(14.0f, 8.0f), color, line);
			draw->AddTriangleFilled(
				point(8.0f, 0.5f), point(5.4f, 4.0f), point(10.6f, 4.0f), color
			);
			draw->AddTriangleFilled(
				point(8.0f, 15.5f), point(5.4f, 12.0f), point(10.6f, 12.0f), color
			);
			draw->AddTriangleFilled(
				point(0.5f, 8.0f), point(4.0f, 5.4f), point(4.0f, 10.6f), color
			);
			draw->AddTriangleFilled(
				point(15.5f, 8.0f), point(12.0f, 5.4f), point(12.0f, 10.6f), color
			);
			break;

		case EditorIcon::Pencil:
			draw->AddLine(point(3.0f, 12.7f), point(11.7f, 4.0f), color, 2.5f * scale);
			draw->AddQuadFilled(
				point(2.0f, 14.0f), point(3.1f, 10.8f), point(5.2f, 12.9f),
				point(2.0f, 14.8f), color
			);
			draw->AddLine(point(10.8f, 3.2f), point(13.0f, 5.4f), color, line);
			break;

		case EditorIcon::Brush:
			draw->AddLine(point(11.8f, 2.2f), point(7.0f, 8.6f), color, 2.5f * scale);
			draw->AddBezierCubic(
				point(6.8f, 8.2f), point(7.0f, 11.2f), point(4.5f, 14.2f),
				point(1.8f, 13.2f), color, line
			);
			draw->AddBezierCubic(
				point(1.8f, 13.2f), point(4.1f, 12.4f), point(2.8f, 9.3f),
				point(6.8f, 8.2f), color, line
			);
			break;

		case EditorIcon::Line:
			draw->AddLine(point(2.5f, 13.5f), point(13.5f, 2.5f), color, 2.0f * scale);
			draw->AddCircleFilled(point(2.5f, 13.5f), 1.35f * scale, color, 10);
			draw->AddCircleFilled(point(13.5f, 2.5f), 1.35f * scale, color, 10);
			break;

		case EditorIcon::Rectangle:
			draw->AddRect(point(2.0f, 3.0f), point(14.0f, 13.0f), color, 0.5f, 0, line);
			break;

		case EditorIcon::Fill:
			draw->AddQuad(
				point(4.0f, 3.0f), point(11.5f, 6.5f), point(7.5f, 13.5f),
				point(1.5f, 10.0f), color, line
			);
			draw->AddLine(point(5.2f, 2.0f), point(12.0f, 8.8f), color, line);
			draw->AddCircleFilled(point(12.8f, 12.5f), 1.7f * scale, color, 10);
			break;

		case EditorIcon::Erase:
			draw->AddQuadFilled(
				point(4.0f, 3.3f), point(13.3f, 8.0f), point(8.2f, 14.0f),
				point(1.2f, 10.3f), color
			);
			draw->AddLine(
				point(4.1f, 11.8f), point(10.0f, 6.2f), ImGui::GetColorU32(ImGuiCol_Button), line
			);
			break;

		case EditorIcon::Eyedropper:
			draw->AddLine(point(4.0f, 12.5f), point(11.2f, 5.3f), color, 2.5f * scale);
			draw->AddCircle(point(12.1f, 4.1f), 2.4f * scale, color, 12, line);
			draw->AddLine(point(2.0f, 14.0f), point(5.2f, 10.8f), color, line);
			break;

		case EditorIcon::Grid: {
			const float x0{ point(3.0f, 3.0f).x };
			const float x1{ point(13.0f, 13.0f).x };
			const float y0{ point(3.0f, 3.0f).y };
			const float y1{ point(13.0f, 13.0f).y };
			for (int i{ 1 }; i <= 2; ++i) {
				const float t{ static_cast<float>(i) / 3.0f };
				draw->AddLine(
					{ x0 + (x1 - x0) * t, y0 }, { x0 + (x1 - x0) * t, y1 }, color,
					std::max(1.0f, scale)
				);
				draw->AddLine(
					{ x0, y0 + (y1 - y0) * t }, { x1, y0 + (y1 - y0) * t }, color,
					std::max(1.0f, scale)
				);
			}
			break;
		}
	}
}

bool DrawEditorIconButton(const char* id, EditorIcon icon, const char* tooltip) {
	return DrawEditorIconButton(
		id, icon,
		EditorIconButtonOptions{
			.tooltip = tooltip,
		}
	);
}

bool DrawEditorIconButton(const char* id, EditorIcon icon, EditorIconButtonOptions options) {
	const auto& style{ ImGui::GetStyle() };
	const float side{ options.compact ? ImGui::GetTextLineHeight() + 2.0f : ImGui::GetFrameHeight() };
	const ImVec2 p0{ ImGui::GetCursorScreenPos() };

	if (options.interactive) {
		if (options.no_navigation) {
			ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
		}
		ImGui::InvisibleButton(id, { side, side });
		if (options.no_navigation) {
			ImGui::PopItemFlag();
		}
	} else {
		ImGui::Dummy({ side, side });
	}

	const bool item_hovered{ ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) };
	const bool hovered{ options.interactive && item_hovered };
	const bool active{ options.interactive && ImGui::IsItemActive() };
	const bool pressed{ options.interactive && ImGui::IsItemClicked(ImGuiMouseButton_Left) };

	auto* draw{ ImGui::GetWindowDrawList() };
	if (options.compact) {
		if (hovered) {
			draw->AddRectFilled(
				p0, { p0.x + side, p0.y + side }, ImGui::GetColorU32(ImGuiCol_HeaderHovered), 3.0f
			);
		}
	} else {
		const bool toggle_icon{
			icon == EditorIcon::Visible || icon == EditorIcon::Hidden ||
			icon == EditorIcon::Locked || icon == EditorIcon::Unlocked
		};

		const ImVec4 background{
			toggle_icon
				? style.Colors[active ? ImGuiCol_FrameBgActive
									: hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg]
				: style.Colors[options.selected ? ImGuiCol_ButtonActive
									: active  ? ImGuiCol_ButtonActive
									: hovered ? ImGuiCol_ButtonHovered
											  : ImGuiCol_Button]
		};
		draw->AddRectFilled(
			p0, { p0.x + side, p0.y + side }, ImGui::GetColorU32(background), style.FrameRounding
		);
		if (options.selected) {
			draw->AddRect(
				p0, { p0.x + side, p0.y + side }, ImGui::GetColorU32(ImGuiCol_Text),
				style.FrameRounding, 0, 1.0f
			);
		}
	}

	const float default_icon_extent{
		options.compact ? side : std::clamp(side - 10.0f, 12.0f, 16.0f)
	};

	const float icon_extent{
		options.icon_extent > 0.0f ? options.icon_extent : default_icon_extent
	};
	DrawEditorIcon(
		draw, icon,
		{
			p0.x + (side - icon_extent) * 0.5f,
			p0.y + (side - icon_extent) * 0.5f,
		},
		icon_extent,
		ImGui::GetColorU32(options.muted ? ImGuiCol_TextDisabled : ImGuiCol_Text)
	);

	if (item_hovered && options.tooltip && *options.tooltip != '\0') {
		ImGui::SetTooltip("%s", options.tooltip);
	}

	return pressed;
}

} // namespace ptgn::editor
