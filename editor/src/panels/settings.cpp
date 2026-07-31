#include "panels/settings.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <optional>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/vector2.h"
#include "core/util/span.h"
#include "panels/inspector_fields.h"
#include "panels/settings_fields.h"
#include "platform/window.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "renderer/renderer_settings.h"
#include "tools/debug/debug_system.h"

namespace ptgn::editor::inspector {

namespace {

bool DrawLineDebugSettings(
	EditorContext& ctx,
	bool& draw_enabled,
	Color& draw_color,
	float& draw_line_width
) {
	bool changed{ false };

	changed |= DrawValue(ctx, "Draw Enabled", draw_enabled);
	changed |= DrawValue(ctx, "Draw Color", draw_color);
	changed |= DrawValue(
		ctx, "Draw Line Width", draw_line_width,
		FieldOptions{
			.speed = 0.1f,
			.min = kMinLineWidth,
			.max = 100.0,
			.format = "%.2f",
			.flags = ImGuiSliderFlags_AlwaysClamp,
		}
	);

	return changed;
}

bool DrawFillDebugSettings(
	EditorContext& ctx,
	bool& draw_enabled,
	Color& draw_color,
	FillStyle& draw_fill_style
) {
	bool changed{ false };

	changed |= DrawValue(ctx, "Draw Enabled", draw_enabled);
	changed |= DrawValue(ctx, "Draw Color", draw_color);
	changed |= DrawValue(ctx, "Draw Fill Style", draw_fill_style);

	return changed;
}

} // namespace

template <>
struct Contents<InteractiveDebugSettings> {
	static bool Draw(EditorContext& ctx, InteractiveDebugSettings& settings) {
		return DrawLineDebugSettings(
			ctx,
			settings.draw_enabled,
			settings.draw_color,
			settings.draw_line_width
		);
	}
};

template <>
struct Contents<CollisionDebugSettings> {
	static bool Draw(EditorContext& ctx, CollisionDebugSettings& settings) {
		bool changed{ false };

		changed |= DrawFillDebugSettings(
			ctx,
			settings.draw_enabled,
			settings.draw_color,
			settings.draw_fill_style
		);

		changed |= DrawValue(ctx, "Draw CCD", settings.draw_ccd);

		return changed;
	}
};

template <>
struct Contents<LightVisibilityDebugSettings> {
	static bool Draw(EditorContext& ctx, LightVisibilityDebugSettings& settings) {
		bool changed{ false };

		changed |= DrawValue(ctx, "Draw Enabled", settings.draw_enabled);
		changed |= DrawValue(ctx, "Draw Interiors", settings.draw_interiors);

		changed |= DrawValue(ctx, "Polygon Color", settings.polygon_color);
		changed |= DrawValue(ctx, "Masks Inside Color", settings.masks_inside_color);
		changed |= DrawValue(
			ctx,
			"Does Not Mask Inside Color",
			settings.does_not_mask_inside_color
		);

		changed |= DrawValue(ctx, "Draw Fill Style", settings.draw_fill_style);

		return changed;
	}
};

template <>
struct Contents<TextDebugSettings> {
	static bool Draw(EditorContext& ctx, TextDebugSettings& settings) {
		bool changed{ DrawLineDebugSettings(
			ctx,
			settings.draw_enabled,
			settings.draw_color,
			settings.draw_line_width
		) };

		changed |= DrawValue(ctx, "Clip Draw Color", settings.clip_draw_color);

		return changed;
	}
};

template <>
struct Contents<RendererSettings> {
	static bool Draw(EditorContext& ctx, RendererSettings& settings) {
		bool changed{ false };

		changed |= DrawValue(ctx, "Tone Mapping", settings.tone_mapping.op);

		if (settings.tone_mapping.op == ToneMappingOperator::Exposure ||
			settings.tone_mapping.op == ToneMappingOperator::ACES) {
			changed |= DrawValue(
				ctx,
				"Exposure",
				settings.tone_mapping.exposure,
				FieldOptions{
					.speed = 0.05f,
					.min = 0.0,
					.max = 20.0,
					.format = "%.2f",
					.flags = ImGuiSliderFlags_AlwaysClamp,
				}
			);
		}

		changed |= DrawValue(
			ctx,
			"Gamma",
			settings.gamma,
			FieldOptions{
				.speed = 0.05f,
				.min = 0.01f,
				.max = 5.0,
				.format = "%.2f",
				.flags = ImGuiSliderFlags_AlwaysClamp,
			}
		);

		return changed;
	}
};

} // namespace ptgn::editor::inspector

namespace ptgn::editor {

namespace {

using namespace inspector;

struct ResolutionPreset {
	const char* label{ "" };
	V2_int size;
};

constexpr std::array<ResolutionPreset, 12> kResolutionPresets{
	{ { "320 x 180 (16:9)", { 320, 180 } },
	  { "640 x 360 (16:9)", { 640, 360 } },
	  { "800 x 450 (16:9)", { 800, 450 } },
	  { "960 x 540 (16:9)", { 960, 540 } },
	  { "1280 x 720 (HD)", { 1280, 720 } },
	  { "1600 x 900", { 1600, 900 } },
	  { "1920 x 1080 (Full HD)", { 1920, 1080 } },
	  { "256 x 224 (SNES)", { 256, 224 } },
	  { "320 x 240 (4:3)", { 320, 240 } },
	  { "640 x 480 (VGA)", { 640, 480 } },
	  { "800 x 600 (SVGA)", { 800, 600 } },
	  { "1024 x 768 (XGA)", { 1024, 768 } } }
};

static_assert(!ContainsDuplicates(kResolutionPresets, &ResolutionPreset::size));
static_assert(!ContainsDuplicates(kResolutionPresets, &ResolutionPreset::label));

bool DrawResolutionMode(EditorContext& ctx) {
	constexpr std::array names{
		"Use Window Size",
		"Use Logical Size",
	};

	auto& renderer{ ctx.editor.GetRenderer() };

	int mode{ renderer.GetSettings().logical_size.has_value() ? 1 : 0 };

	bool changed{ DrawPropertyRow("Resolution Source", [&]() {
		return ImGui::Combo(
			"##value",
			&mode,
			names.data(),
			static_cast<int>(names.size())
		);
	}) };

	if (!changed) {
		return false;
	}

	if (mode == 0) {
		renderer.SetLogicalSize(std::nullopt);
	} else {
		renderer.SetLogicalSize(renderer.GetDisplayViewport().size);
	}

	return true;
}

bool DrawResolutionPreset(EditorContext& ctx) {
	auto& renderer{ ctx.editor.GetRenderer() };

	auto logical_size{ renderer.GetLogicalSize() };

	auto selected{ std::ranges::find_if(
		kResolutionPresets,
		[&](const ResolutionPreset& preset) {
			return preset.size == logical_size;
		}
	) };

	auto preview{ selected != kResolutionPresets.end() ? selected->label : "Custom" };

	return DrawPropertyRow("Preset", [&]() {
		bool changed{ false };

		if (ImGui::BeginCombo("##value", preview)) {
			for (const auto& preset : kResolutionPresets) {
				bool is_selected{ preset.size == logical_size };

				if (ImGui::Selectable(preset.label, is_selected)) {
					renderer.SetLogicalSize(preset.size);
					changed = true;
				}

				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}

		return changed;
	});
}

bool DrawDisplaySettings(EditorContext& ctx) {
	if (!ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
		return false;
	}

	bool changed{ false };

	ImGui::Indent();

	changed |= DrawResolutionMode(ctx);

	auto& renderer{ ctx.editor.GetRenderer() };

	if (renderer.GetSettings().logical_size.has_value()) {
		changed |= DrawResolutionPreset(ctx);

		auto logical_size{ renderer.GetLogicalSize() };

		if (DrawValue(
				ctx,
				"Logical Size",
				logical_size,
				FieldOptions{
					.speed = 1.0f,
					.min = 1.0,
					.max = 4096.0,
					.format = "%d",
					.flags = ImGuiSliderFlags_AlwaysClamp,
				}
			)) {
			renderer.SetLogicalSize(logical_size);
			changed = true;
		}

		auto scaling_mode{ renderer.GetSettings().scaling_mode };

		if (DrawValue(ctx, "Scaling Mode", scaling_mode)) {
			renderer.SetScalingMode(scaling_mode);
			changed = true;
		}
	} else {
		auto window_size{ renderer.GetDisplayViewport().size };

		ImGui::BeginDisabled();
		DrawValue(ctx, "Window Size", window_size);
		ImGui::EndDisabled();
	}

	auto window_settings{ ctx.editor.GetWindow().GetSettings() };
	bool window_changed{ false };

	DrawPropertyRow("Default Window Size", [&]() {
		ImGui::Text(
			"%d x %d",
			window_settings.size.x,
			window_settings.size.y
		);
		return false;
	});
	window_changed |= DrawValue(ctx, "Resizable", window_settings.resizable);
	window_changed |= DrawValue(ctx, "Start Maximized", window_settings.maximized);
	window_changed |= DrawValue(ctx, "Window Background", window_settings.background_color);

	if (window_changed) {
		ctx.editor.GetWindow().SetSettings(window_settings);
		changed = true;
	}

	changed |= settings::EditValue(
		ctx,
		"Renderer Background",
		[&]() { return ctx.editor.GetRenderer().GetSettings().background_color; },
		[&](Color color) { ctx.editor.GetRenderer().SetBackgroundColor(color); }
	);

	ImGui::Unindent();
	ImGui::Spacing();

	return changed;
}

} // namespace

void EngineSettingsPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Engine Settings");

	bool changed{ false };

	{
		AutoLabelWidthScope label_width{ "DisplaySettings" };
		changed |= DrawDisplaySettings(ctx);
	}

	changed |= settings::EditSection(
		ctx,
		"Rendering",
		[&]() { return ctx.editor.GetRenderer().GetSettings(); },
		[&](const RendererSettings& value) {
			ctx.editor.GetRenderer().SetSettings(value);
		}
	);

	if (changed) {
		ctx.local.state.is_dirty = true;
	}

	ImGui::End();
}

void DebugSettingsPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Debug Settings");

	ImGui::Checkbox("ImGui Metrics", &ctx.local.settings.show_imgui_metrics);

	if (ctx.local.settings.show_imgui_metrics) {
		ImGui::ShowMetricsWindow(&ctx.local.settings.show_imgui_metrics);
	}

	bool changed{ false };

	changed |= settings::EditSection(
		ctx,
		"Interaction",
		[&]() { return ctx.editor.GetDebugSystem().settings.interaction; },
		[&](const InteractiveDebugSettings& value) {
			ctx.editor.GetDebugSystem().settings.interaction = value;
		}
	);

	changed |= settings::EditSection(
		ctx,
		"Collision",
		[&]() { return ctx.editor.GetDebugSystem().settings.collision; },
		[&](const CollisionDebugSettings& value) {
			ctx.editor.GetDebugSystem().settings.collision = value;
		}
	);

	changed |= settings::EditSection(
		ctx,
		"Text",
		[&]() { return ctx.editor.GetDebugSystem().settings.text; },
		[&](const TextDebugSettings& value) {
			ctx.editor.GetDebugSystem().settings.text = value;
		}
	);

	changed |= settings::EditSection(
		ctx,
		"Light Visibility",
		[&]() { return ctx.editor.GetDebugSystem().settings.light; },
		[&](const LightVisibilityDebugSettings& value) {
			ctx.editor.GetDebugSystem().settings.light = value;
		}
	);

	if (changed) {
		ctx.local.state.is_dirty = true;
	}

	ImGui::End();
}

void EditorSettingsPanel::OnRender(
	EditorContext& ctx
) {
	ImGui::Begin(
		"Editor Settings"
	);

	auto entity_picking{
		ctx.editor
			.GetSettings()
			.entity_picking
	};

	if (ImGui::Checkbox(
			"Entity Picking",
			&entity_picking
		)) {
		ctx.editor.SetEntityPickingMode(
			entity_picking
		);
	}

	auto render_only_selected_scene{
		ctx.editor
			.GetSettings()
			.render_only_selected_scene
	};

	if (ImGui::Checkbox(
			"Render Only Selected Scene",
			&render_only_selected_scene
		)) {
		ctx.editor
			.SetRenderOnlySelectedScene(
				render_only_selected_scene
			);
	}

	ImGui::TextDisabled(
		"Excludes unselected scenes from the editor presentation."
	);

	auto gizmo_uses_local_orientation{
		ctx.editor
			.GetSettings()
			.gizmo_uses_local_orientation
	};

	if (ImGui::Checkbox(
			"Local Gizmo Orientation",
			&gizmo_uses_local_orientation
		)) {
		ctx.editor
			.SetGizmoUsesLocalOrientation(
				gizmo_uses_local_orientation
			);
	}

	ImGui::TextDisabled(
		"Local orientation rotates the translate and scale axes with the entity."
	);

	ImGui::Spacing();

	ImGui::Checkbox(
		"Show Read-Only Inspector Data",
		&ctx.local.settings.show_read_only_inspector_data
	);

	ImGui::TextDisabled(
		"Shows read-only components and reflected read-only component members."
	);

	ImGui::End();
}

} // namespace ptgn::editor
