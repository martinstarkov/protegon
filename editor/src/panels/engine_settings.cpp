#include "panels/engine_settings.h"

#include <imgui.h>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/scaling_mode.h"

// TODO: Add fps modification.

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
								  int* h, int h_min, int h_max, bool enabled = true) {
		ImGui::BeginDisabled(!enabled);

		const float total_width = ImGui::GetContentRegionAvail().x;
		const float field_width = (total_width - kSpacing) * 0.5f;

		ImGui::SetNextItemWidth(field_width);
		ImGui::DragInt(id_w, w, 1.0f, w_min, w_max, "W: %d", ImGuiSliderFlags_AlwaysClamp);

		ImGui::SameLine(0.0f, kSpacing);

		ImGui::SetNextItemWidth(field_width);
		ImGui::DragInt(id_h, h, 1.0f, h_min, h_max, "H: %d", ImGuiSliderFlags_AlwaysClamp);

		ImGui::EndDisabled();
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

	const auto draw_centered_label = [](const char* text, float width) {
		float text_width = ImGui::CalcTextSize(text).x;
		float cursor_x	 = ImGui::GetCursorPosX();
		float offset	 = (width - text_width) * 0.5f;
		if (offset > 0.0f) {
			ImGui::SetCursorPosX(cursor_x + offset);
		}
		ImGui::TextUnformatted(text);
	};

	const auto draw_rgba_color_row = [&](const char* label, const char* id_prefix, auto get_color,
										 auto set_color) {
		table_row_label(label);

		constexpr float kPickerWidth = 36.0f;
		const float total_width		 = ImGui::GetContentRegionAvail().x;
		const float field_width		 = (total_width - 4.0f * kSpacing - kPickerWidth) / 4.0f;
		const float sublabel_height	 = ImGui::GetTextLineHeight();

		auto color = get_color();

		int r = static_cast<int>(color.r);
		int g = static_cast<int>(color.g);
		int b = static_cast<int>(color.b);
		int a = static_cast<int>(color.a);

		float colorf[4]{ static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
						 static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f };

		bool changed_from_sliders = false;
		bool changed_from_picker  = false;

		ImGui::BeginGroup();

		ImGui::BeginGroup();
		draw_centered_label("R", field_width);
		ImGui::SetNextItemWidth(field_width);
		changed_from_sliders |= ImGui::DragInt(
			(std::string("##") + id_prefix + "R").c_str(), &r, 1.0f, 0, 255, "%d",
			ImGuiSliderFlags_AlwaysClamp
		);
		ImGui::EndGroup();

		ImGui::SameLine(0.0f, kSpacing);

		ImGui::BeginGroup();
		draw_centered_label("G", field_width);
		ImGui::SetNextItemWidth(field_width);
		changed_from_sliders |= ImGui::DragInt(
			(std::string("##") + id_prefix + "G").c_str(), &g, 1.0f, 0, 255, "%d",
			ImGuiSliderFlags_AlwaysClamp
		);
		ImGui::EndGroup();

		ImGui::SameLine(0.0f, kSpacing);

		ImGui::BeginGroup();
		draw_centered_label("B", field_width);
		ImGui::SetNextItemWidth(field_width);
		changed_from_sliders |= ImGui::DragInt(
			(std::string("##") + id_prefix + "B").c_str(), &b, 1.0f, 0, 255, "%d",
			ImGuiSliderFlags_AlwaysClamp
		);
		ImGui::EndGroup();

		ImGui::SameLine(0.0f, kSpacing);

		ImGui::BeginGroup();
		draw_centered_label("A", field_width);
		ImGui::SetNextItemWidth(field_width);
		changed_from_sliders |= ImGui::DragInt(
			(std::string("##") + id_prefix + "A").c_str(), &a, 1.0f, 0, 255, "%d",
			ImGuiSliderFlags_AlwaysClamp
		);
		ImGui::EndGroup();

		ImGui::SameLine(0.0f, kSpacing);

		ImGui::BeginGroup();
		ImGui::Dummy(ImVec2(0.0f, sublabel_height));
		ImGui::SetNextItemWidth(kPickerWidth);
		changed_from_picker |= ImGui::ColorEdit4(
			(std::string("##") + id_prefix + "Picker").c_str(), colorf,
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
				ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf
		);
		ImGui::EndGroup();

		ImGui::EndGroup();

		if (changed_from_sliders) {
			color.r = static_cast<std::uint8_t>(std::clamp(r, 0, 255));
			color.g = static_cast<std::uint8_t>(std::clamp(g, 0, 255));
			color.b = static_cast<std::uint8_t>(std::clamp(b, 0, 255));
			color.a = static_cast<std::uint8_t>(std::clamp(a, 0, 255));
			set_color(color);
		} else if (changed_from_picker) {
			color.r = static_cast<std::uint8_t>(std::round(colorf[0] * 255.0f));
			color.g = static_cast<std::uint8_t>(std::round(colorf[1] * 255.0f));
			color.b = static_cast<std::uint8_t>(std::round(colorf[2] * 255.0f));
			color.a = static_cast<std::uint8_t>(std::round(colorf[3] * 255.0f));
			set_color(color);
		}
	};

	if (ImGui::BeginTable("##EngineSettingsTable", 2, ImGuiTableFlags_SizingFixedFit)) {
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, kLabelWidth);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		// Logical Resolution
		table_row_label("Resolution");

		bool use_game_size = true;

		{
			// Resolution source
			constexpr std::array resolution_mode_names{
				"Use Window Size",
				"Use Game Size",
			};

			int resolution_mode = static_cast<int>(ctx.editor.HasGameSize());

			full_width();
			if (ImGui::Combo(
					"##ResolutionMode", &resolution_mode, resolution_mode_names.data(),
					static_cast<int>(resolution_mode_names.size())
				)) {
				if (resolution_mode == 0) {
					ctx.editor.SetGameSize(std::nullopt);
				} else {
					ctx.editor.SetGameSize(ctx.editor.GetDisplayViewport().size);
				}
			}

			use_game_size = ctx.editor.HasGameSize();

			if (use_game_size) {
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
			} else {
				auto window_size{ ctx.editor.GetDisplayViewport().size };
				drag_int_pair(
					"##WindowWidth", &window_size.x, 0, 0, "##WindowHeight", &window_size.y, 0, 0,
					false
				);
			}
		}

		if (use_game_size) {
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
		}

		// Window Background Color
		draw_rgba_color_row(
			"Window Background", "WindowBackground",
			[&]() { return ctx.editor.GetWindowBackgroundColor(); },
			[&](const auto& color) { ctx.editor.SetWindowBackgroundColor(color); }
		);

		// Renderer Background Color
		draw_rgba_color_row(
			"Renderer Background", "RendererBackground",
			[&]() { return ctx.editor.GetRendererBackgroundColor(); },
			[&](const auto& color) { ctx.editor.SetRendererBackgroundColor(color); }
		);

		ImGui::EndTable();
	}

	ImGui::End();
}

} // namespace ptgn::editor