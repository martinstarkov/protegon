#pragma once

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/math/angle.h"
#include "core/math/transform.h"
#include "editor/editor_context.h"
#include "editor/editor_icons.h"
#include "panels/inspector_fields.h"
#include "panels/inspector_helpers.h"
#include "panels/inspector_layout.h"

namespace ptgn::editor::inspector {

// An inspector position is stored in the coordinate space of the owning property.
// Converting the viewport point is separate from writing the property, so the same
// picker can edit entity transforms, nested visuals, shapes, and UI configuration.
struct InspectorPositionBinding {
	PositionPicker::Convert from_world{};
	PositionPicker::Apply apply{};
	PositionPicker::Validate validate{};
	std::optional<V2_float> reference_world{};
	bool show_relative{ false };
	bool available{ true };
	bool round_to_pixels{ true };
	bool display_frame{ false };
};

inline void DrawInspectorPositionPickButton(
	EditorContext& ctx, std::string_view label, V2_float initial, InspectorPositionBinding binding
) {
	ImGuiID source_id{ ImGui::GetID("##PickPosition") };
	ImGui::BeginDisabled(!binding.available || !binding.apply);
	bool pressed{ DrawEditorIconButton(
		"##PickPosition", EditorIcon::Target,
		EditorIconButtonOptions{
			.tooltip	 = "Pick position in the viewport.",
			.selected	 = ctx.local.position_picker.IsActive() &&
						   ctx.local.position_picker.SourceId() == source_id,
			.icon_extent = ImGui::GetTextLineHeight() + 2.0f,
		}
	) };
	ImGui::EndDisabled();

	if (!pressed) {
		return;
	}

	ctx.undo.CommitActiveEdit();
	ctx.local.position_picker.Cancel();
	ctx.local.position_picker.Begin(
		std::string{ "Pick " } + std::string{ label }, initial, std::move(binding.from_world),
		std::move(binding.apply), binding.reference_world, binding.show_relative, {},
		binding.round_to_pixels, binding.display_frame, static_cast<std::uint32_t>(source_id),
		std::move(binding.validate)
	);
}

struct SharedTransformFieldOptions {
	float* depth{ nullptr };
	bool* ignore_position{ nullptr };
	bool* ignore_rotation{ nullptr };
	bool* ignore_scale{ nullptr };
	bool* ignore_depth{ nullptr };
	bool* scale_ratio_locked{ nullptr };
	InspectorPositionBinding position{};
	float position_speed{ 1.0f };
};

// All transform rows reserve identical space for their trailing action controls.
// Pinning those controls to the content edge also avoids fractional width drift.
struct SharedTransformRowLayout {
	float spacing{ 0.0f };
	float side{ 0.0f };
	float right_x{ 0.0f };
	float fields_width{ 0.0f };
	int action_count{ 0 };
	bool inline_actions{ false };
};

[[nodiscard]] inline SharedTransformRowLayout MakeSharedTransformRowLayout(
	int action_count, float minimum_fields_width
) {
	float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
	float side{ ImGui::GetFrameHeight() };
	float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
	bool inline_actions{ available >= minimum_fields_width +
										  static_cast<float>(action_count) * (side + spacing) };
	float fields_width{ inline_actions
							? available - static_cast<float>(action_count) * (side + spacing)
							: available };
	return SharedTransformRowLayout{
		.spacing		= spacing,
		.side			= side,
		.right_x		= ImGui::GetCursorScreenPos().x + available,
		.fields_width	= std::max(1.0f, fields_width),
		.action_count	= action_count,
		.inline_actions = inline_actions,
	};
}

inline void PlaceSharedTransformRowAction(const SharedTransformRowLayout& row, int index) {
	if (row.inline_actions || index > 0) {
		ImGui::SameLine(0.0f, row.spacing);
	}
	ImVec2 cursor{ ImGui::GetCursorScreenPos() };
	int remaining{ row.action_count - index };
	cursor.x = row.right_x - static_cast<float>(remaining) * row.side -
			   static_cast<float>(remaining - 1) * row.spacing;
	ImGui::SetCursorScreenPos(cursor);
}

inline bool DrawSharedPositionField(
	EditorContext& ctx, std::string_view label, V2_float& position,
	InspectorPositionBinding binding, bool* ignore_position = nullptr, float speed = 1.0f
) {
	return DrawPropertyRow(label, [&]() {
		SharedTransformRowLayout row{
			MakeSharedTransformRowLayout(ignore_position ? 2 : 1, 104.0f)
		};
		float field_width{ InspectorSplitWidth(2, row.fields_width, row.spacing) };

		ImGui::SetNextItemWidth(field_width);
		bool changed{
			ImGui::DragFloat("##X", &position.x, std::max(speed, 1.0f), 0.0f, 0.0f, "X: %.0f")
		};
		ImGui::SameLine(0.0f, row.spacing);
		ImGui::SetNextItemWidth(field_width);
		changed |=
			ImGui::DragFloat("##Y", &position.y, std::max(speed, 1.0f), 0.0f, 0.0f, "Y: %.0f");

		PlaceSharedTransformRowAction(row, 0);
		DrawInspectorPositionPickButton(ctx, label, position, std::move(binding));

		if (ignore_position) {
			PlaceSharedTransformRowAction(row, 1);
			changed |= ImGui::Checkbox("##IgnoreParent", ignore_position);
			DrawTooltip("Ignore parent position.");
		}
		return changed;
	});
}

inline bool DrawSharedTransformFields(
	EditorContext& ctx, Transform& transform, SharedTransformFieldOptions options
) {
	bool changed{ false };

	auto draw_ignore = [](bool* ignore, const char* tooltip) {
		if (!ignore) {
			return false;
		}
		bool changed{ ImGui::Checkbox("##IgnoreParent", ignore) };
		DrawTooltip(tooltip);
		return changed;
	};

	changed |= DrawSharedPositionField(
		ctx, "Position", transform.position, std::move(options.position), options.ignore_position,
		options.position_speed
	);

	if (options.depth) {
		changed |= DrawPropertyRow("Depth", [&]() {
			SharedTransformRowLayout row{
				MakeSharedTransformRowLayout(options.ignore_depth ? 1 : 0, 72.0f)
			};
			ImGui::SetNextItemWidth(row.fields_width);
			bool local_changed{ ImGui::DragFloat(
				"##Value", options.depth, 1.0f, -1000.0f, 1000.0f, "%.0f",
				ImGuiSliderFlags_AlwaysClamp
			) };
			if (options.ignore_depth) {
				PlaceSharedTransformRowAction(row, 0);
				local_changed |= draw_ignore(options.ignore_depth, "Ignore parent depth.");
			}
			return local_changed;
		});
	}

	changed |= DrawPropertyRow("Rotation", [&]() {
		SharedTransformRowLayout row{
			MakeSharedTransformRowLayout(options.ignore_rotation ? 1 : 0, 72.0f)
		};
		ImGui::SetNextItemWidth(row.fields_width);
		float degrees{ Degrees{ transform.rotation }.value };
		bool local_changed{ ImGui::DragFloat(
			"##Value", &degrees, 1.0f, 0.0f, 360.0f, "%.1f deg", ImGuiSliderFlags_AlwaysClamp
		) };
		if (local_changed) {
			transform.rotation = Radians{ Degrees{ degrees } };
		}
		if (options.ignore_rotation) {
			PlaceSharedTransformRowAction(row, 0);
			local_changed |= draw_ignore(options.ignore_rotation, "Ignore parent rotation.");
		}
		return local_changed;
	});

	changed |= DrawPropertyRow("Scale", [&]() {
		constexpr float kMinimum{ 0.001f };
		constexpr float kEpsilon{ 0.000001f };
		V2_float before{ transform.scale };
		int action_count{ static_cast<int>(options.scale_ratio_locked != nullptr) +
						  static_cast<int>(options.ignore_scale != nullptr) };
		SharedTransformRowLayout row{ MakeSharedTransformRowLayout(action_count, 104.0f) };
		float field_width{ InspectorSplitWidth(2, row.fields_width, row.spacing) };

		ImGui::SetNextItemWidth(field_width);
		bool x_changed{ ImGui::DragFloat(
			"##X", &transform.scale.x, 0.01f, -1000.0f, 1000.0f, "X: %.2f",
			ImGuiSliderFlags_AlwaysClamp
		) };
		ImGui::SameLine(0.0f, row.spacing);
		ImGui::SetNextItemWidth(field_width);
		bool y_changed{ ImGui::DragFloat(
			"##Y", &transform.scale.y, 0.01f, -1000.0f, 1000.0f, "Y: %.2f",
			ImGuiSliderFlags_AlwaysClamp
		) };

		auto clamp = [](float value, float previous) {
			if (std::abs(value) >= kMinimum) {
				return value;
			}
			return (value < 0.0f || (value == 0.0f && previous < 0.0f)) ? -kMinimum : kMinimum;
		};
		if (x_changed) {
			transform.scale.x = clamp(transform.scale.x, before.x);
		}
		if (y_changed) {
			transform.scale.y = clamp(transform.scale.y, before.y);
		}
		if (options.scale_ratio_locked && *options.scale_ratio_locked) {
			if (x_changed && !y_changed && std::abs(before.x) > kEpsilon) {
				transform.scale.y = clamp(before.y * transform.scale.x / before.x, before.y);
			} else if (y_changed && !x_changed && std::abs(before.y) > kEpsilon) {
				transform.scale.x = clamp(before.x * transform.scale.y / before.y, before.x);
			}
		}
		int action_index{ 0 };
		if (options.scale_ratio_locked) {
			PlaceSharedTransformRowAction(row, action_index++);
			bool& locked{ *options.scale_ratio_locked };
			if (DrawEditorIconButton(
					"##LockRatio", locked ? EditorIcon::Locked : EditorIcon::Unlocked,
					EditorIconButtonOptions{
						.tooltip	 = "Lock the scale ratio.",
						.muted		 = !locked,
						.icon_extent = ImGui::GetTextLineHeight() + 2.0f,
					}
				)) {
				locked = !locked;
			}
		}
		if (options.ignore_scale) {
			PlaceSharedTransformRowAction(row, action_index);
			bool ignore_changed{ draw_ignore(options.ignore_scale, "Ignore parent scale.") };
			return x_changed || y_changed || ignore_changed;
		}
		return x_changed || y_changed;
	});
	return changed;
}

} // namespace ptgn::editor::inspector
