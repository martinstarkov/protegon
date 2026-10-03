#include "editor/editor_icons.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>

namespace ptgn::editor {

namespace {

[[nodiscard]] float SnapStrokeCenter(float value, int thickness) {
	return (thickness % 2) == 0 ? std::round(value) : std::round(value - 0.5f) + 0.5f;
}

} // namespace

void DrawEditorIcon(ImDrawList* draw, EditorIcon icon, ImVec2 min, float extent, ImU32 color) {
	int extent_pixels{ std::max(1, static_cast<int>(std::lround(extent))) };
	bool even_extent{ (extent_pixels % 2) == 0 };
	int line_pixels{ even_extent ? 2 : 1 };
	float line{ static_cast<float>(line_pixels) };

	ImVec2 requested_center{
		min.x + extent * 0.5f,
		min.y + extent * 0.5f,
	};

	ImVec2 center{
		SnapStrokeCenter(requested_center.x, extent_pixels),
		SnapStrokeCenter(requested_center.y, extent_pixels),
	};

	float pixel_extent{ static_cast<float>(extent_pixels) };

	ImVec2 origin{
		center.x - pixel_extent * 0.5f,
		center.y - pixel_extent * 0.5f,
	};

	float scale{ pixel_extent / 16.0f };

	auto point = [&](float px, float py) {
		return ImVec2{
			origin.x + px * scale,
			origin.y + py * scale,
		};
	};

	auto draw_vertical_band = [&](float cx, float y0, float y1, int thickness) {
		cx = SnapStrokeCenter(cx, thickness);
		y0 = std::round(y0);
		y1 = std::round(y1);
		if (y1 < y0) {
			std::swap(y0, y1);
		}
		float half{ static_cast<float>(thickness) * 0.5f };
		draw->AddRectFilled({ cx - half, y0 }, { cx + half, y1 }, color);
	};

	auto draw_horizontal_band = [&](float cy, float x0, float x1, int thickness) {
		cy = SnapStrokeCenter(cy, thickness);
		x0 = std::round(x0);
		x1 = std::round(x1);
		if (x1 < x0) {
			std::swap(x0, x1);
		}
		float half{ static_cast<float>(thickness) * 0.5f };
		draw->AddRectFilled({ x0, cy - half }, { x1, cy + half }, color);
	};

	auto draw_rect_outline = [&](ImVec2 a, ImVec2 b, int thickness) {
		float left{ std::min(a.x, b.x) };
		float right{ std::max(a.x, b.x) };
		float top{ std::min(a.y, b.y) };
		float bottom{ std::max(a.y, b.y) };

		left   = SnapStrokeCenter(left, thickness);
		right  = SnapStrokeCenter(right, thickness);
		top	   = SnapStrokeCenter(top, thickness);
		bottom = SnapStrokeCenter(bottom, thickness);

		draw_horizontal_band(top, left, right, thickness);
		draw_horizontal_band(bottom, left, right, thickness);
		draw_vertical_band(left, top, bottom, thickness);
		draw_vertical_band(right, top, bottom, thickness);
	};

	float hierarchy_scale{ pixel_extent / 18.0f };

	auto hierarchy_point = [&](float px, float py) {
		return ImVec2{
			origin.x + px * hierarchy_scale,
			origin.y + py * hierarchy_scale,
		};
	};

	auto hierarchy_line_point = [&](float px, float py) {
		ImVec2 p{ hierarchy_point(px, py) };
		return ImVec2{
			SnapStrokeCenter(p.x, line_pixels),
			SnapStrokeCenter(p.y, line_pixels),
		};
	};

	bool runtime_icon{ icon == EditorIcon::Play || icon == EditorIcon::Stop ||
					   icon == EditorIcon::Pause || icon == EditorIcon::Reset ||
					   icon == EditorIcon::StepForward || icon == EditorIcon::StepBackward ||
					   icon == EditorIcon::Camera };

	ImDrawListFlags prev_flags{ draw->Flags };

	if (runtime_icon) {
		draw->Flags &= ~(ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines);
	}

	switch (icon) {
		case EditorIcon::Play:
			draw->AddTriangleFilled(
				point(3.0f, 1.0f), point(3.0f, 15.0f), point(14.0f, 8.0f), color
			);
			break;

		case EditorIcon::Stop: {
			float half_side{ std::round(5.0f * scale) };
			float left{ center.x - half_side };
			float right{ center.x + half_side };
			float top{ center.y - half_side };
			float bottom{ center.y + half_side };
			draw->AddRectFilled({ left, top }, { right, bottom }, color);
			break;
		}

		case EditorIcon::Pause: {
			float left_outer{ std::round(point(3.0f, 8.0f).x) };
			float left_inner{ std::round(point(6.5f, 8.0f).x) };
			float right_inner{ center.x * 2.0f - left_inner };
			float right_outer{ center.x * 2.0f - left_outer };
			float top{ std::round(point(8.0f, 2.5f).y) };
			float bottom{ center.y * 2.0f - top };

			draw->AddRectFilled({ left_outer, top }, { left_inner, bottom }, color, 0.75f);
			draw->AddRectFilled({ right_inner, top }, { right_outer, bottom }, color, 0.75f);
			break;
		}

		case EditorIcon::Reset: {
			float radius{ 5.0f * scale };
			draw->PathArcTo(center, radius, -0.15f * IM_PI, 1.55f * IM_PI, 20);
			draw->PathStroke(color, 0, line);
			draw->AddTriangleFilled(point(3.1f, 2.7f), point(7.0f, 2.9f), point(4.4f, 6.0f), color);
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

		case EditorIcon::Camera: {
			float bar_offset{ 6.0f * scale };
			float bar_top{ SnapStrokeCenter(center.y - bar_offset, line_pixels) };
			float bar_bottom{ center.y * 2.0f - bar_top };
			float bar_left{ center.x - 5.0f * scale };
			float bar_right{ center.x + 5.0f * scale };
			draw_horizontal_band(bar_top, bar_left, bar_right, line_pixels);
			draw_horizontal_band(bar_bottom, bar_left, bar_right, line_pixels);

			ImDrawListFlags previous_flags{ draw->Flags };
			draw->Flags &= ~ImDrawListFlags_AntiAliasedLines;
			draw->AddCircle(center, 3.0f * scale, color, 28, line);
			draw->Flags = previous_flags;

			float tab_width{ std::max(2.0f, std::round(2.0f * scale)) };
			float tab_height{ std::max(2.0f, std::round(3.0f * scale)) + 1.0f };
			float tab_right{ std::round(bar_right - 1.0f * scale) };
			float tab_left{ tab_right - tab_width };
			float tab_bottom{ bar_top + line * 0.5f };
			float tab_top{ tab_bottom - tab_height };
			draw->AddRectFilled({ tab_left, tab_top }, { tab_right, tab_bottom }, color);
			break;
		}

		case EditorIcon::Entity:
			draw->AddQuad(
				hierarchy_line_point(9.0f, 2.0f), hierarchy_line_point(15.0f, 6.0f),
				hierarchy_line_point(9.0f, 10.0f), hierarchy_line_point(3.0f, 6.0f), color, line
			);
			draw->AddLine(
				hierarchy_line_point(3.0f, 6.0f), hierarchy_line_point(3.0f, 12.0f), color, line
			);
			draw->AddLine(
				hierarchy_line_point(15.0f, 6.0f), hierarchy_line_point(15.0f, 12.0f), color, line
			);
			draw->AddLine(
				hierarchy_line_point(3.0f, 12.0f), hierarchy_line_point(9.0f, 16.0f), color, line
			);
			draw->AddLine(
				hierarchy_line_point(15.0f, 12.0f), hierarchy_line_point(9.0f, 16.0f), color, line
			);
			draw->AddLine(
				hierarchy_line_point(9.0f, 10.0f), hierarchy_line_point(9.0f, 16.0f), color, line
			);
			break;

		case EditorIcon::Tile:
			for (int row{}; row < 2; ++row) {
				for (int column{}; column < 2; ++column) {
					float x0{ 3.0f + static_cast<float>(column) * 6.0f };
					float y0{ 3.0f + static_cast<float>(row) * 6.0f };
					float x1{ 8.0f + static_cast<float>(column) * 6.0f };
					float y1{ 8.0f + static_cast<float>(row) * 6.0f };
					draw_rect_outline(
						hierarchy_point(x0, y0), hierarchy_point(x1, y1), line_pixels
					);
				}
			}
			break;

		case EditorIcon::Visible:
		case EditorIcon::Hidden:  {
			ImVec2 eye_outline[]{
				hierarchy_line_point(2.0f, 9.0f),	hierarchy_line_point(6.0f, 5.0f),
				hierarchy_line_point(12.0f, 5.0f),	hierarchy_line_point(16.0f, 9.0f),
				hierarchy_line_point(12.0f, 13.0f), hierarchy_line_point(6.0f, 13.0f),
			};

			draw->AddPolyline(eye_outline, 6, color, ImDrawFlags_Closed, line);

			if (icon == EditorIcon::Visible) {
				draw->AddCircleFilled(center, 2.3f * hierarchy_scale, color, 8);
			} else {
				float slash_offset{ 6.0f * hierarchy_scale };
				float half_width{ line * 0.5f };
				float normal_offset{ half_width * 0.70710678f };

				ImVec2 start{
					center.x - slash_offset,
					center.y + slash_offset,
				};
				ImVec2 end{
					center.x + slash_offset,
					center.y - slash_offset,
				};

				ImVec2 normal{ normal_offset, normal_offset };

				draw->AddQuadFilled(
					{ start.x - normal.x, start.y - normal.y },
					{ end.x - normal.x, end.y - normal.y }, { end.x + normal.x, end.y + normal.y },
					{ start.x + normal.x, start.y + normal.y }, color
				);
			}
			break;
		}

		case EditorIcon::Locked:
		case EditorIcon::Unlocked: {
			float body_half_width{ 4.5f * hierarchy_scale };
			float body_top{ origin.y + 8.0f * hierarchy_scale };
			float body_bottom{ origin.y + 15.0f * hierarchy_scale };
			float body_left{ SnapStrokeCenter(center.x - body_half_width, line_pixels) };
			float body_right{ center.x * 2.0f - body_left };

			draw_rect_outline({ body_left, body_top }, { body_right, body_bottom }, line_pixels);

			float shackle_half_width{ 2.5f * hierarchy_scale };
			float shackle_left{ SnapStrokeCenter(center.x - shackle_half_width, line_pixels) };
			float shackle_right{ center.x * 2.0f - shackle_left };
			float shackle_top{ SnapStrokeCenter(origin.y + 5.0f * hierarchy_scale, line_pixels) };
			float shackle_bottom{ SnapStrokeCenter(body_top, line_pixels) };

			draw_vertical_band(shackle_left, shackle_top, shackle_bottom, line_pixels);

			if (icon == EditorIcon::Locked) {
				draw_vertical_band(shackle_right, shackle_top, shackle_bottom, line_pixels);
				draw_horizontal_band(shackle_top, shackle_left, shackle_right, line_pixels);
			} else {
				draw->AddLine(
					{ shackle_left, shackle_top },
					{ center.x + 2.0f * hierarchy_scale, shackle_top - 1.0f * hierarchy_scale },
					color, line
				);
			}
			break;
		}

		case EditorIcon::Select: {
			draw->AddTriangleFilled(
				point(2.0f, 2.0f), point(4.5f, 12.0f), point(12.0f, 4.5f), color
			);

			ImDrawListFlags previous_flags{ draw->Flags };

			draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
			draw->AddQuadFilled(
				point(6.8f, 8.6f), point(8.6f, 6.8f), point(14.0f, 12.0f), point(12.0f, 14.0f),
				color
			);
			draw->Flags = previous_flags;

			break;
		}

		case EditorIcon::Move: {
			float arm_offset{ std::max(1.0f, std::round(6.0f * scale) + 1.0f) };
			float cap_offset{ std::max(1.0f, arm_offset - 2.0f) };
			float cap_half_extent{ std::max(1.0f, std::round(2.5f * scale)) };

			float top{ SnapStrokeCenter(center.y - arm_offset, line_pixels) };
			float bottom{ center.y * 2.0f - top };
			float left{ SnapStrokeCenter(center.x - arm_offset, line_pixels) };
			float right{ center.x * 2.0f - left };

			float cap_top{ SnapStrokeCenter(center.y - cap_offset, line_pixels) };
			float cap_bottom{ center.y * 2.0f - cap_top };
			float cap_left{ SnapStrokeCenter(center.x - cap_offset, line_pixels) };
			float cap_right{ center.x * 2.0f - cap_left };

			draw_vertical_band(center.x, top, bottom, line_pixels);
			draw_horizontal_band(center.y, left, right, line_pixels);
			draw_horizontal_band(
				cap_top, center.x - cap_half_extent, center.x + cap_half_extent, line_pixels
			);
			draw_horizontal_band(
				cap_bottom, center.x - cap_half_extent, center.x + cap_half_extent, line_pixels
			);
			draw_vertical_band(
				cap_left, center.y - cap_half_extent, center.y + cap_half_extent, line_pixels
			);
			draw_vertical_band(
				cap_right, center.y - cap_half_extent, center.y + cap_half_extent, line_pixels
			);
			break;
		}

		case EditorIcon::Pencil: {
			float half_side{ std::max(2.0f, std::round(3.0f * scale)) };
			float left{ std::round(center.x - half_side) };
			float right{ center.x * 2.0f - left };
			float top{ std::round(center.y - half_side) };
			float bottom{ center.y * 2.0f - top };

			draw->AddRectFilled({ left, top }, { right, bottom }, color);

			break;
		}

		case EditorIcon::Brush: {
			float radius{ std::max(2.0f, std::round(5.0f * scale)) };

			ImDrawListFlags previous_flags{ draw->Flags };

			draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
			draw->AddCircleFilled(center, radius, color, 16);
			draw->Flags = previous_flags;

			break;
		}

		case EditorIcon::Line: {
			float offset{ 6.0f * scale };
			float half_normal{ line * 0.35355339f };

			ImVec2 start{ center.x - offset, center.y + offset };
			ImVec2 end{ center.x + offset, center.y - offset };
			ImVec2 normal{ half_normal, half_normal };

			ImDrawListFlags previous_flags{ draw->Flags };

			draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
			draw->AddQuadFilled(
				{ start.x - normal.x, start.y - normal.y }, { end.x - normal.x, end.y - normal.y },
				{ end.x + normal.x, end.y + normal.y }, { start.x + normal.x, start.y + normal.y },
				color
			);
			draw->Flags = previous_flags;

			break;
		}

		case EditorIcon::Rectangle:
			draw_rect_outline(point(2.0f, 3.0f), point(14.0f, 13.0f), line_pixels);
			break;

		case EditorIcon::Fill: {
			ImDrawListFlags previous_flags{ draw->Flags };
			draw->Flags &= ~(ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines);

			ImVec2 square_center{ point(5.25f, 5.25f) };
			float half_extent{ std::max(2.0f, std::round(5.0f * scale)) };

			draw->AddQuadFilled(
				{ square_center.x, square_center.y - half_extent },
				{ square_center.x + half_extent, square_center.y },
				{ square_center.x, square_center.y + half_extent },
				{ square_center.x - half_extent, square_center.y }, color
			);

			ImVec2 drop_center{ point(11.75f, 11.75f) };
			float drop_radius{ std::max(1.0f, std::round(3.0f * scale)) };

			draw->AddCircleFilled(drop_center, drop_radius, color, 12);

			draw->Flags = previous_flags;

			break;
		}

		case EditorIcon::Erase: {
			ImDrawListFlags previous_flags{ draw->Flags };
			draw->Flags &= ~(ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines);

			auto rotate = [&](float x, float y) {
				ImVec2 p{ point(x, y) };
				float dx{ p.x - center.x };
				float dy{ p.y - center.y };

				return ImVec2{
					center.x + (dx - dy) * 0.70710678f,
					center.y + (dx + dy) * 0.70710678f,
				};
			};

			ImVec2 a{ rotate(1.0f, 5.0f) };
			ImVec2 b{ rotate(15.0f, 5.0f) };
			ImVec2 c{ rotate(15.0f, 11.0f) };
			ImVec2 d{ rotate(1.0f, 11.0f) };
			ImVec2 top_mid{ rotate(8.0f, 5.0f) };
			ImVec2 bottom_mid{ rotate(8.0f, 11.0f) };

			draw->AddQuadFilled(top_mid, b, c, bottom_mid, color);
			draw->AddQuad(a, b, c, d, color, line);
			draw->AddLine(top_mid, bottom_mid, color, line);

			draw->Flags = previous_flags;
			break;
		}

		case EditorIcon::Eyedropper: {
			ImDrawListFlags previous_flags{ draw->Flags };
			draw->Flags &= ~(ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines);

			float offset{ 6.0f * scale };
			float half_normal{ line * 0.35355339f };

			ImVec2 start{ center.x - offset, center.y + offset };
			ImVec2 end{ center.x + offset + 1.0f, center.y - offset - 1.0f };
			ImVec2 normal{ half_normal, half_normal };

			draw->AddQuadFilled(
				{ start.x - normal.x, start.y - normal.y }, { end.x - normal.x, end.y - normal.y },
				{ end.x + normal.x, end.y + normal.y }, { start.x + normal.x, start.y + normal.y },
				color
			);

			ImVec2 bulb_center{
				center.x + 3.5f * scale - 1.0f,
				center.y - 3.5f * scale + 1.0f,
			};
			float bulb_radius{ std::max(2.0f, std::round(4.0f * scale)) };

			draw->AddCircleFilled(bulb_center, bulb_radius, color, 20);

			draw->Flags = previous_flags;
			break;
		}

		case EditorIcon::Target: {
			float radius{ 4.5f * scale };
			draw_vertical_band(center.x, origin.y, origin.y + pixel_extent, line_pixels);
			draw_horizontal_band(center.y, origin.x, origin.x + pixel_extent, line_pixels);
			draw->AddCircle(center, radius, color, 24, line);
			break;
		}

		case EditorIcon::Grid: {
			float offset{ 3.0f * scale };
			float v0{ SnapStrokeCenter(center.x - offset, line_pixels) };
			float v1{ center.x * 2.0f - v0 };
			float h0{ SnapStrokeCenter(center.y - offset, line_pixels) };
			float h1{ center.y * 2.0f - h0 };

			float left{ std::round(origin.x + 1.0f * scale) };
			float right{ center.x * 2.0f - left };
			float top{ std::round(origin.y + 1.0f * scale) };
			float bottom{ center.y * 2.0f - top };

			draw_vertical_band(v0, top, bottom, line_pixels);
			draw_vertical_band(v1, top, bottom, line_pixels);
			draw_horizontal_band(h0, left, right, line_pixels);
			draw_horizontal_band(h1, left, right, line_pixels);
			break;
		}
	}

	if (runtime_icon) {
		draw->Flags = prev_flags;
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
	float side{ options.compact ? ImGui::GetTextLineHeight() + 2.0f : ImGui::GetFrameHeight() };
	ImVec2 p0{ ImGui::GetCursorScreenPos() };

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

	bool item_hovered{ ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) };
	bool hovered{ options.interactive && item_hovered };
	bool active{ options.interactive && ImGui::IsItemActive() };
	bool pressed{ options.interactive && ImGui::IsItemClicked(ImGuiMouseButton_Left) };

	auto* draw{ ImGui::GetWindowDrawList() };
	if (options.compact) {
		if (hovered) {
			draw->AddRectFilled(
				p0, { p0.x + side, p0.y + side }, ImGui::GetColorU32(ImGuiCol_HeaderHovered), 3.0f
			);
		}
	} else {
		bool toggle_icon{ icon == EditorIcon::Visible || icon == EditorIcon::Hidden ||
						  icon == EditorIcon::Locked || icon == EditorIcon::Unlocked };

		ImVec4 background{ toggle_icon ? style.Colors
											 [active	? ImGuiCol_FrameBgActive
											  : hovered ? ImGuiCol_FrameBgHovered
														: ImGuiCol_FrameBg]
									   : style.Colors
											 [options.selected ? ImGuiCol_ButtonActive
											  : active		   ? ImGuiCol_ButtonActive
											  : hovered		   ? ImGuiCol_ButtonHovered
															   : ImGuiCol_Button] };
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

	float default_icon_extent{ options.compact ? side : std::clamp(side - 10.0f, 12.0f, 16.0f) };
	float icon_extent{ options.icon_extent > 0.0f ? options.icon_extent : default_icon_extent };

	DrawEditorIcon(
		draw, icon,
		{
			p0.x + (side - icon_extent) * 0.5f,
			p0.y + (side - icon_extent) * 0.5f,
		},
		icon_extent, ImGui::GetColorU32(options.muted ? ImGuiCol_TextDisabled : ImGuiCol_Text)
	);

	if (item_hovered && options.tooltip && *options.tooltip != '\0') {
		ImGui::SetTooltip("%s", options.tooltip);
	}

	return pressed;
}

} // namespace ptgn::editor
