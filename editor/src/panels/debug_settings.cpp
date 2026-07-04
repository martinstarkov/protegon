#include "panels/debug_settings.h"

#include <imgui.h>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "panels/inspector_fields.h"
#include "panels/settings_fields.h"
#include "tools/debug/debug_system.h"

namespace ptgn::editor::inspector {

namespace {

bool DrawLineDebugSettings(bool& draw_enabled, Color& draw_color, float& draw_line_width) {
	bool changed{ false };

	changed |= DrawValue("Draw Enabled", draw_enabled);
	changed |= DrawValue("Draw Color", draw_color);
	changed |= DrawValue(
		"Draw Line Width", draw_line_width,
		FieldOptions{
			.speed	= 0.1f,
			.min	= kMinLineWidth,
			.max	= 100.0,
			.format = "%.2f",
			.flags	= ImGuiSliderFlags_AlwaysClamp,
		}
	);

	return changed;
}

bool DrawFillDebugSettings(bool& draw_enabled, Color& draw_color, FillStyle& draw_fill_style) {
	bool changed{ false };

	changed |= DrawValue("Draw Enabled", draw_enabled);
	changed |= DrawValue("Draw Color", draw_color);
	changed |= DrawValue("Draw Fill Style", draw_fill_style);

	return changed;
}

} // namespace

template <>
struct Contents<InteractiveDebugSettings> {
	static bool Draw(InteractiveDebugSettings& settings) {
		return DrawLineDebugSettings(
			settings.draw_enabled, settings.draw_color, settings.draw_line_width
		);
	}
};

template <>
struct Contents<CollisionDebugSettings> {
	static bool Draw(CollisionDebugSettings& settings) {
		bool changed{ false };

		changed |= DrawFillDebugSettings(
			settings.draw_enabled, settings.draw_color, settings.draw_fill_style
		);

		changed |= DrawValue("Draw CCD", settings.draw_ccd);

		return changed;
	}
};

template <>
struct Contents<LightVisibilityDebugSettings> {
	static bool Draw(LightVisibilityDebugSettings& settings) {
		bool changed{ false };

		changed |= DrawValue("Draw Enabled", settings.draw_enabled);
		changed |= DrawValue("Draw Interiors", settings.draw_interiors);

		changed |= DrawValue("Polygon Color", settings.polygon_color);
		changed |= DrawValue("Masks Inside Color", settings.masks_inside_color);
		changed |= DrawValue("Does Not Mask Inside Color", settings.does_not_mask_inside_color);

		changed |= DrawValue("Draw Fill Style", settings.draw_fill_style);

		return changed;
	}
};

template <>
struct Contents<TextDebugSettings> {
	static bool Draw(TextDebugSettings& settings) {
		bool changed{ DrawLineDebugSettings(
			settings.draw_enabled, settings.draw_color, settings.draw_line_width
		) };

		changed |= DrawValue("Clip Draw Color", settings.clip_draw_color);

		return changed;
	}
};

} // namespace ptgn::editor::inspector

namespace ptgn::editor {

void DebugSettingsPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Debug Settings");

	ImGui::Checkbox("ImGui Metrics", &show_imgui_metrics_);

	if (show_imgui_metrics_) {
		ImGui::ShowMetricsWindow(&show_imgui_metrics_);
	}

	settings::EditSection(
		"Interaction", [&]() { return ctx.editor.GetDebug().interaction; },
		[&](const InteractiveDebugSettings& value) { ctx.editor.GetDebug().interaction = value; }
	);

	settings::EditSection(
		"Collision", [&]() { return ctx.editor.GetDebug().collision; },
		[&](const CollisionDebugSettings& value) { ctx.editor.GetDebug().collision = value; }
	);

	settings::EditSection(
		"Text", [&]() { return ctx.editor.GetDebug().text; },
		[&](const TextDebugSettings& value) { ctx.editor.GetDebug().text = value; }
	);

	settings::EditSection(
		"Light Visibility", [&]() { return ctx.editor.GetDebug().light; },
		[&](const LightVisibilityDebugSettings& value) { ctx.editor.GetDebug().light = value; }
	);

	ImGui::End();
}

} // namespace ptgn::editor