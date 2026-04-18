#include "panels/engine_settings.h"

#include <imgui.h>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/scaling_mode.h"

namespace ptgn::editor {

struct ResolutionPreset {
	const char* label;
	int width;
	int height;
};

static constexpr ResolutionPreset kResolutionPresets[] = {
	{ "320 x 180 (16:9)", 320, 180 },		 { "640 x 360 (16:9)", 640, 360 },
	{ "800 x 450 (16:9)", 800, 450 },		 { "960 x 540 (16:9)", 960, 540 },
	{ "1280 x 720 (HD)", 1280, 720 },		 { "1600 x 900", 1600, 900 },
	{ "1920 x 1080 (Full HD)", 1920, 1080 }, { "256 x 224 (SNES)", 256, 224 },
	{ "320 x 240 (4:3)", 320, 240 },		 { "640 x 480 (VGA)", 640, 480 },
	{ "800 x 600 (SVGA)", 800, 600 },		 { "1024 x 768 (XGA)", 1024, 768 },
};

static int FindMatchingResolutionPreset(V2_int size) {
	for (int i = 0; i < IM_ARRAYSIZE(kResolutionPresets); ++i) {
		if (kResolutionPresets[i].width == size.x && kResolutionPresets[i].height == size.y) {
			return i;
		}
	}
	return -1;
}

void EngineSettingsPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Engine Settings");

	constexpr float kLabelWidth = 140.0f;
	constexpr float kSpacing	= 6.0f;

	const auto table_row_label = [](const char* text) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(text);
		ImGui::TableSetColumnIndex(1);
	};

	const auto full_width = [] {
		ImGui::SetNextItemWidth(-FLT_MIN);
	};

	const auto drag_int_pair = [](const char* id_w, int* w, int w_min, int w_max, const char* id_h,
								  int* h, int h_min, int h_max) {
		const float total_width = ImGui::GetContentRegionAvail().x;
		const float field_width = (total_width - kSpacing) * 0.5f;

		ImGui::SetNextItemWidth(field_width);
		ImGui::DragInt(id_w, w, 1.0f, w_min, w_max, "W: %d", ImGuiSliderFlags_AlwaysClamp);

		ImGui::SameLine(0.0f, kSpacing);

		ImGui::SetNextItemWidth(field_width);
		ImGui::DragInt(id_h, h, 1.0f, h_min, h_max, "H: %d", ImGuiSliderFlags_AlwaysClamp);
	};

	struct ResolutionPreset {
		const char* label;
		int width;
		int height;
	};

	// TODO: Use magic enum instead of this.
	constexpr std::array scaling_mode_names{
		"Disabled", "Stretch", "Letterbox", "Overscan", "IntegerScale",
	};

	if (ImGui::BeginTable("##EngineSettingsTable", 2, ImGuiTableFlags_SizingFixedFit)) {
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, kLabelWidth);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		// Logical Resolution
		table_row_label("Game Size");

		{
			auto game_size{ ctx.editor.GetGameSize() };

			const int preset_index = FindMatchingResolutionPreset(game_size);

			const char* preview =
				(preset_index >= 0) ? kResolutionPresets[preset_index].label : "Custom";

			full_width();
			if (ImGui::BeginCombo("##GameSizePreset", preview)) {
				for (int i = 0; i < static_cast<int>(IM_ARRAYSIZE(kResolutionPresets)); ++i) {
					const bool selected = (i == preset_index);
					if (ImGui::Selectable(kResolutionPresets[i].label, selected)) {
						ctx.editor.SetGameSize(V2_int{ kResolutionPresets[i].width,
													   kResolutionPresets[i].height });
					}
					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::Spacing();

			V2_int size{ ctx.editor.GetGameSize() };
			drag_int_pair("##GameWidth", &size.x, 1, 4096, "##GameHeight", &size.y, 1, 2160);
			ctx.editor.SetGameSize(size);
		}

		// Scaling Mode
		table_row_label("Scaling Mode");

		{
			int scaling_mode = static_cast<int>(ctx.editor.GetScalingMode());

			full_width();
			if (ImGui::Combo(
					"##ScalingMode", &scaling_mode, scaling_mode_names.data(),
					static_cast<int>(scaling_mode_names.size())
				)) {
				ctx.editor.SetScalingMode(static_cast<ScalingMode>(scaling_mode));
			}
		}

		ImGui::EndTable();
	}

	ImGui::End();
}

} // namespace ptgn::editor