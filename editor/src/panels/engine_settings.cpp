#include "panels/engine_settings.h"

#include <imgui.h>

#include <array>
#include <optional>
#include <string>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/math/vector2.h"
#include "core/util/span.h"
#include "panels/inspector_fields.h"
#include "panels/settings_fields.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/render_settings.h"

namespace ptgn::editor::inspector {

template <>
struct Contents<RenderSettings> {
	static bool Draw(RenderSettings& settings) {
		bool changed{ false };

		changed |= DrawValue("Tone Mapping", settings.tone_mapping.op);

		if (settings.tone_mapping.op == ToneMappingOperator::Exposure ||
			settings.tone_mapping.op == ToneMappingOperator::ACES) {
			changed |= DrawValue(
				"Exposure", settings.tone_mapping.exposure,
				FieldOptions{
					.speed	= 0.05f,
					.min	= 0.0,
					.max	= 20.0,
					.format = "%.2f",
					.flags	= ImGuiSliderFlags_AlwaysClamp,
				}
			);
		}

		changed |= DrawValue(
			"Gamma", settings.gamma,
			FieldOptions{
				.speed	= 0.05f,
				.min	= 0.01,
				.max	= 5.0,
				.format = "%.2f",
				.flags	= ImGuiSliderFlags_AlwaysClamp,
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

bool DrawResolutionMode(Editor& editor) {
	constexpr std::array names{
		"Use Window Size",
		"Use Logical Size",
	};

	int mode{ editor.HasLogicalSize() ? 1 : 0 };

	bool changed{ DrawPropertyRow("Resolution Source", [&]() {
		return ImGui::Combo("##value", &mode, names.data(), static_cast<int>(names.size()));
	}) };

	if (!changed) {
		return false;
	}

	if (mode == 0) {
		editor.SetLogicalSize(std::nullopt);
	} else {
		editor.SetLogicalSize(editor.GetDisplayViewport().size);
	}

	return true;
}

bool DrawResolutionPreset(Editor& editor) {
	auto logical_size{ editor.GetLogicalSize() };

	auto selected{ std::ranges::find(kResolutionPresets, logical_size, &ResolutionPreset::size) };

	auto preview{ selected != kResolutionPresets.end() ? selected->label : "Custom" };

	return DrawPropertyRow("Preset", [&]() {
		bool changed{ false };

		if (ImGui::BeginCombo("##value", preview)) {
			for (const auto& preset : kResolutionPresets) {
				bool is_selected{ preset.size == logical_size };

				if (ImGui::Selectable(preset.label, is_selected)) {
					editor.SetLogicalSize(preset.size);
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

void DrawDisplaySettings(Editor& editor) {
	if (!ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	ImGui::Indent();

	DrawResolutionMode(editor);

	if (editor.HasLogicalSize()) {
		DrawResolutionPreset(editor);

		auto logical_size{ editor.GetLogicalSize() };

		if (DrawValue(
				"Logical Size", logical_size,
				FieldOptions{
					.speed	= 1.0f,
					.min	= 1.0,
					.max	= 4096.0,
					.format = "%d",
					.flags	= ImGuiSliderFlags_AlwaysClamp,
				}
			)) {
			editor.SetLogicalSize(logical_size);
		}

		auto scaling_mode{ editor.GetScalingMode() };

		if (DrawValue("Scaling Mode", scaling_mode)) {
			editor.SetScalingMode(scaling_mode);
		}
	} else {
		auto window_size{ editor.GetDisplayViewport().size };

		ImGui::BeginDisabled();
		DrawValue("Window Size", window_size);
		ImGui::EndDisabled();
	}

	settings::EditValue(
		"Window Background", [&]() { return editor.GetWindowBackgroundColor(); },
		[&](Color color) { editor.SetWindowBackgroundColor(color); }
	);

	settings::EditValue(
		"Renderer Background", [&]() { return editor.GetRendererBackgroundColor(); },
		[&](Color color) { editor.SetRendererBackgroundColor(color); }
	);

	ImGui::Unindent();
	ImGui::Spacing();
}

} // namespace

void EngineSettingsPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Engine Settings");

	DrawDisplaySettings(ctx.editor);

	settings::EditSection(
		"Rendering", [&]() { return ctx.editor.GetRenderSettings(); },
		[&](const RenderSettings& value) { ctx.editor.SetRenderSettings(value); }
	);

	ImGui::End();
}

} // namespace ptgn::editor