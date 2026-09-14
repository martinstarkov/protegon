#pragma once

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "editor/editor_context.h"

namespace ptgn::editor {

struct ColorPickerResult {
	bool changed{ false };
	bool interaction_started{ false };
	bool use_default{ false };
};

namespace color_picker_detail {

struct ColorPickerUiState {
	bool initialized{ false };
	Color previous{};
	std::string hex{};
	Color hex_color{};
	bool hex_was_active{ false };
	std::string registered_search{};
	float palette_height{ 0.0f };
	std::optional<std::size_t> selected_palette{};
	std::optional<std::size_t> renaming_palette{};
	std::string rename_buffer{};
	bool rename_focus_requested{ false };
};

struct ParsedHexColor {
	std::optional<Color> color{};
	std::string error{};
};

inline std::unordered_map<ImGuiID, ColorPickerUiState>& PickerStates() {
	static std::unordered_map<ImGuiID, ColorPickerUiState> states;
	return states;
}

[[nodiscard]] inline std::array<float, 4> ToFloatColor(Color value) {
	return {
		static_cast<float>(value.r) / 255.0f,
		static_cast<float>(value.g) / 255.0f,
		static_cast<float>(value.b) / 255.0f,
		static_cast<float>(value.a) / 255.0f,
	};
}

[[nodiscard]] inline Color ToColor(const float* value) {
	auto to_byte = [](float channel) {
		return static_cast<std::uint8_t>(
			std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f)
		);
	};

	return Color{
		to_byte(value[0]),
		to_byte(value[1]),
		to_byte(value[2]),
		to_byte(value[3]),
	};
}

[[nodiscard]] inline ImVec4 ToImVec4(Color value) {
	const auto color{ ToFloatColor(value) };
	return ImVec4{ color[0], color[1], color[2], color[3] };
}

[[nodiscard]] inline bool ContainsCaseInsensitive(
	std::string_view text,
	std::string_view search
) {
	if (search.empty()) {
		return true;
	}

	const auto match{ std::search(
		text.begin(),
		text.end(),
		search.begin(),
		search.end(),
		[](char lhs, char rhs) {
			return std::tolower(static_cast<unsigned char>(lhs)) ==
				std::tolower(static_cast<unsigned char>(rhs));
		}
	) };
	return match != text.end();
}

[[nodiscard]] inline std::string ColorDescription(Color value) {
	if (const auto* registered{ FindRegisteredColor(value) }) {
		return registered->key + " (" + std::to_string(value.r) + ", " +
			std::to_string(value.g) + ", " + std::to_string(value.b) + ", " +
			std::to_string(value.a) + ")";
	}

	return "RGBA (" + std::to_string(value.r) + ", " + std::to_string(value.g) + ", " +
		std::to_string(value.b) + ", " + std::to_string(value.a) + ")";
}

inline void RecordPaletteChange(
	EditorContext& ctx,
	std::string label,
	std::vector<EditorColorPalette> before
) {
	const auto after{ ctx.project_state.color_palettes };
	if (before == after) {
		return;
	}

	ctx.undo.PushApplied(
		std::move(label),
		[context = &ctx, before = std::move(before)]() {
			context->project_state.color_palettes = before;
		},
		[context = &ctx, after]() {
			context->project_state.color_palettes = after;
		},
		false,
		true
	);
}

[[nodiscard]] inline bool PaletteNameExists(
	const EditorProjectState& state,
	std::string_view name
) {
	return std::ranges::any_of(
		state.color_palettes,
		[name](const EditorColorPalette& palette) {
			return palette.name == name;
		}
	);
}

[[nodiscard]] inline std::string MakeUniquePaletteName(const EditorProjectState& state) {
	for (std::size_t index{ 1 };; ++index) {
		std::string candidate{ "Palette " + std::to_string(index) };
		if (!PaletteNameExists(state, candidate)) {
			return candidate;
		}
	}
}

[[nodiscard]] inline std::string ToHex(Color value) {
	constexpr char kDigits[]{ "0123456789ABCDEF" };
	std::string result{ "#00000000" };
	const std::array<std::uint8_t, 4> channels{ value.r, value.g, value.b, value.a };

	for (std::size_t index{ 0 }; index < channels.size(); ++index) {
		const auto channel{ channels[index] };
		result[1 + index * 2] = kDigits[(channel >> 4u) & 0x0Fu];
		result[2 + index * 2] = kDigits[channel & 0x0Fu];
	}

	return result;
}

[[nodiscard]] inline int HexDigit(char value) {
	if (value >= '0' && value <= '9') {
		return value - '0';
	}
	if (value >= 'a' && value <= 'f') {
		return 10 + value - 'a';
	}
	if (value >= 'A' && value <= 'F') {
		return 10 + value - 'A';
	}
	return -1;
}

[[nodiscard]] inline ParsedHexColor ParseHexColor(std::string_view value) {
	ParsedHexColor result;

	if (value.empty()) {
		result.error = "Enter a hex color such as #RRGGBB or #RRGGBBAA.";
		return result;
	}

	if (value.front() == '#') {
		value.remove_prefix(1);
	}

	if (value.size() != 3 && value.size() != 4 && value.size() != 6 && value.size() != 8) {
		result.error = "Hex colors must contain 3, 4, 6, or 8 hexadecimal digits.";
		return result;
	}

	for (const char character : value) {
		if (HexDigit(character) < 0) {
			result.error = std::string{ "Invalid hexadecimal character '" } + character + "'.";
			return result;
		}
	}

	auto read_short = [&](std::size_t index) {
		const int digit{ HexDigit(value[index]) };
		return static_cast<std::uint8_t>(digit * 17);
	};

	auto read_long = [&](std::size_t index) {
		return static_cast<std::uint8_t>(
			HexDigit(value[index]) * 16 + HexDigit(value[index + 1])
		);
	};

	if (value.size() == 3 || value.size() == 4) {
		result.color = Color{
			read_short(0),
			read_short(1),
			read_short(2),
			value.size() == 4 ? read_short(3) : static_cast<std::uint8_t>(255),
		};
		return result;
	}

	result.color = Color{
		read_long(0),
		read_long(2),
		read_long(4),
		value.size() == 8 ? read_long(6) : static_cast<std::uint8_t>(255),
	};
	return result;
}

inline void DrawInvalidInputBorder(std::string_view error) {
	if (error.empty()) {
		return;
	}

	ImGui::GetWindowDrawList()->AddRect(
		ImGui::GetItemRectMin(),
		ImGui::GetItemRectMax(),
		ImGui::GetColorU32(ImVec4{ 1.0f, 0.20f, 0.20f, 1.0f }),
		ImGui::GetStyle().FrameRounding,
		0,
		1.5f
	);

	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%.*s", static_cast<int>(error.size()), error.data());
	}
}

inline void BeginPaletteRename(
	ColorPickerUiState& state,
	std::size_t palette_index,
	std::string_view current_name
) {
	state.renaming_palette = palette_index;
	state.rename_buffer = std::string{ current_name };
	state.rename_focus_requested = true;
}

inline void AdjustSelectionAfterPaletteDelete(
	ColorPickerUiState& state,
	std::size_t deleted_index
) {
	if (state.selected_palette.has_value()) {
		if (*state.selected_palette == deleted_index) {
			state.selected_palette.reset();
		} else if (*state.selected_palette > deleted_index) {
			--*state.selected_palette;
		}
	}

	if (state.renaming_palette.has_value()) {
		if (*state.renaming_palette == deleted_index) {
			state.renaming_palette.reset();
			state.rename_buffer.clear();
			state.rename_focus_requested = false;
		} else if (*state.renaming_palette > deleted_index) {
			--*state.renaming_palette;
		}
	}
}

} // namespace color_picker_detail

/// @brief Draws the extended color picker with registered colors, optional default-color action,
/// current/previous previews, RGBA/hex editing, and shared project palettes.
/// Palette mutations are immediately pushed onto the editor undo stack.
inline ColorPickerResult DrawColorPickerContents(
	EditorContext& ctx,
	const char* id,
	Color& value,
	ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaBar |
		ImGuiColorEditFlags_AlphaPreviewHalf |
		ImGuiColorEditFlags_DisplayRGB |
		ImGuiColorEditFlags_InputRGB |
		ImGuiColorEditFlags_Uint8,
	const Color* default_color = nullptr
) {
	ColorPickerResult result;
	ImGui::PushID(id);

	const ImGuiID state_id{ ImGui::GetID("##ColorPickerState") };
	auto& ui_state{ color_picker_detail::PickerStates()[state_id] };
	if (!ui_state.initialized || ImGui::IsWindowAppearing()) {
		ui_state.initialized = true;
		ui_state.previous = value;
		ui_state.hex = color_picker_detail::ToHex(value);
		ui_state.hex_color = value;
		ui_state.hex_was_active = false;
	}

	auto& palettes{ ctx.project_state.color_palettes };
	if (ui_state.selected_palette.has_value() &&
		*ui_state.selected_palette >= palettes.size()) {
		ui_state.selected_palette.reset();
	}
	if (ui_state.renaming_palette.has_value() &&
		*ui_state.renaming_palette >= palettes.size()) {
		ui_state.renaming_palette.reset();
		ui_state.rename_buffer.clear();
		ui_state.rename_focus_requested = false;
	}

	ImGui::SeparatorText("Registered Color");
	const auto* selected_registered{ FindRegisteredColor(value) };
	const char* preview{ selected_registered ? selected_registered->key.c_str() : "Custom" };
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::BeginCombo("##RegisteredColor", preview)) {
		if (ImGui::IsWindowAppearing()) {
			ui_state.registered_search.clear();
			ImGui::SetKeyboardFocusHere();
		}

		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint(
			"##RegisteredColorSearch",
			"Search colors...",
			&ui_state.registered_search
		);
		ImGui::Separator();

		bool any_visible{ false };
		for (const auto& registered : GetRegisteredColors()) {
			if (!color_picker_detail::ContainsCaseInsensitive(
					registered.key,
					ui_state.registered_search
				)) {
				continue;
			}

			any_visible = true;
			ImGui::PushID(registered.key.c_str());
			ImGui::ColorButton(
				"##Swatch",
				color_picker_detail::ToImVec4(registered.value),
				ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip,
				ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() }
			);
			ImGui::SameLine();
			const bool selected{ value == registered.value };
			if (ImGui::Selectable(registered.key.c_str(), selected)) {
				result.interaction_started = true;
				if (!selected) {
					value = registered.value;
					result.changed = true;
				}
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
			ImGui::PopID();
		}

		if (!any_visible) {
			ImGui::TextDisabled("No matching registered colors");
		}
		ImGui::EndCombo();
	}

	if (default_color) {
		if (ImGui::Button("Use Default Color", ImVec2{ -FLT_MIN, 0.0f })) {
			result.interaction_started = true;
			result.use_default = true;
			if (value != *default_color) {
				value = *default_color;
				result.changed = true;
			}
		}
	}

	ImGui::SeparatorText("Picker");

	auto rgba{ color_picker_detail::ToFloatColor(value) };
	const ImGuiColorEditFlags picker_flags{
		flags | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs
	};

	const float preview_width{ std::max(72.0f, ImGui::GetFrameHeight() * 3.25f) };
	const float picker_width{ 300.0f };
	const float preview_height{ std::max(36.0f, ImGui::GetFrameHeight() * 1.75f) };

	ImGui::BeginGroup();
	ImGui::SetNextItemWidth(picker_width);
	if (ImGui::ColorPicker4("##Picker", rgba.data(), picker_flags)) {
		value = color_picker_detail::ToColor(rgba.data());
		result.changed = true;
	}
	result.interaction_started |= ImGui::IsItemActivated();
	ImGui::EndGroup();

	ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
	ImGui::BeginGroup();
	ImGui::TextUnformatted("Current");
	ImGui::ColorButton(
		"##CurrentColor",
		color_picker_detail::ToImVec4(value),
		ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip,
		ImVec2{ preview_width, preview_height }
	);

	ImGui::Spacing();
	ImGui::TextUnformatted("Previous");
	if (ImGui::ColorButton(
			"##PreviousColor",
			color_picker_detail::ToImVec4(ui_state.previous),
			ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip,
			ImVec2{ preview_width, preview_height }
		)) {
		result.interaction_started = true;
		if (value != ui_state.previous) {
			value = ui_state.previous;
			result.changed = true;
		}
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Restore the color that was set when this picker was opened.");
	}
	ImGui::EndGroup();

	std::array<int, 4> channels{
		static_cast<int>(value.r),
		static_cast<int>(value.g),
		static_cast<int>(value.b),
		static_cast<int>(value.a),
	};
	const bool has_alpha{ (flags & ImGuiColorEditFlags_NoAlpha) == 0 };
	const int channel_count{ has_alpha ? 4 : 3 };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float channels_available{ ImGui::GetContentRegionAvail().x };
	const float channel_width{ std::max(
		48.0f,
		(channels_available - spacing * static_cast<float>(channel_count - 1)) /
			static_cast<float>(channel_count)
	) };
	const std::array<const char*, 4> channel_formats{ "R: %d", "G: %d", "B: %d", "A: %d" };

	bool channels_changed{ false };
	for (int index{ 0 }; index < channel_count; ++index) {
		if (index > 0) {
			ImGui::SameLine(0.0f, spacing);
		}
		ImGui::SetNextItemWidth(channel_width);
		channels_changed |= ImGui::DragInt(
			(index == 0 ? "##R" : index == 1 ? "##G" : index == 2 ? "##B" : "##A"),
			&channels[static_cast<std::size_t>(index)],
			1.0f,
			0,
			255,
			channel_formats[static_cast<std::size_t>(index)],
			ImGuiSliderFlags_AlwaysClamp
		);
		result.interaction_started |= ImGui::IsItemActivated();
	}

	if (channels_changed) {
		value = Color{
			static_cast<std::uint8_t>(channels[0]),
			static_cast<std::uint8_t>(channels[1]),
			static_cast<std::uint8_t>(channels[2]),
			has_alpha ? static_cast<std::uint8_t>(channels[3]) : value.a,
		};
		result.changed = true;
	}

	if (!ui_state.hex_was_active && ui_state.hex_color != value) {
		ui_state.hex = color_picker_detail::ToHex(value);
		ui_state.hex_color = value;
	}

	ImGui::TextUnformatted("Hex");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-FLT_MIN);
	const bool hex_changed{ ImGui::InputTextWithHint(
		"##HexColor",
		"#RRGGBB or #RRGGBBAA",
		&ui_state.hex
	) };
	result.interaction_started |= ImGui::IsItemActivated();
	const bool hex_active{ ImGui::IsItemActive() };

	if (hex_changed) {
		const auto parsed{ color_picker_detail::ParseHexColor(ui_state.hex) };
		if (parsed.color.has_value()) {
			ui_state.hex_color = *parsed.color;
			if (value != *parsed.color) {
				value = *parsed.color;
				result.changed = true;
			}
		}
	}

	const auto parsed_hex{ color_picker_detail::ParseHexColor(ui_state.hex) };
	color_picker_detail::DrawInvalidInputBorder(parsed_hex.error);

	if (!hex_active && ui_state.hex_was_active) {
		// Leaving the field always resynchronizes it to the selected color. This also
		// clears an invalid partial value when the user picks a color elsewhere.
		ui_state.hex = color_picker_detail::ToHex(value);
		ui_state.hex_color = value;
	}
	ui_state.hex_was_active = hex_active;

	ImGui::SeparatorText("Palettes");

	const bool has_selected_palette{
		ui_state.selected_palette.has_value() &&
		*ui_state.selected_palette < palettes.size()
	};
	const bool selected_palette_has_color{
		has_selected_palette &&
		std::ranges::contains(palettes[*ui_state.selected_palette].colors, value)
	};
	const std::string add_color_label{ has_selected_palette
		? "+ Add Color to " + palettes[*ui_state.selected_palette].name
		: "+ Add Color" };

	const float button_spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float button_height{ ImGui::GetFrameHeight() };
	const float palette_button_width{
		ImGui::CalcTextSize("+ Palette").x +
		ImGui::GetStyle().FramePadding.x * 2.0f + 4.0f
	};
	const float add_color_button_width{ std::max(
		1.0f,
		ImGui::GetContentRegionAvail().x - palette_button_width - button_spacing
	) };

	if (ImGui::Button("+ Palette", ImVec2{ palette_button_width, button_height })) {
		auto before{ palettes };
		palettes.push_back(EditorColorPalette{
			.name = color_picker_detail::MakeUniquePaletteName(ctx.project_state),
		});
		color_picker_detail::RecordPaletteChange(
			ctx,
			"Add Color Palette",
			std::move(before)
		);
		ui_state.selected_palette = palettes.empty()
			? std::optional<std::size_t>{}
			: std::optional<std::size_t>{ palettes.size() - 1 };
	}

	ImGui::SameLine(0.0f, button_spacing);
	ImGui::BeginDisabled(!has_selected_palette || selected_palette_has_color);
	if (ImGui::Button(
			add_color_label.c_str(),
			ImVec2{ add_color_button_width, button_height }
		)) {
		auto before{ palettes };
		palettes[*ui_state.selected_palette].colors.push_back(value);
		color_picker_detail::RecordPaletteChange(
			ctx,
			"Add Palette Color",
			std::move(before)
		);
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		if (!has_selected_palette) {
			ImGui::SetTooltip("Select a palette first.");
		} else if (selected_palette_has_color) {
			ImGui::SetTooltip("This color is already in the selected palette.");
		}
	}

	if (palettes.empty()) {
		ImGui::TextDisabled("No palettes");
		ImGui::PopID();
		return result;
	}

	const ImGuiViewport* viewport{ ImGui::GetWindowViewport() };
	const float resize_grip_height{ 7.0f };
	const float viewport_bottom{ viewport
		? viewport->WorkPos.y + viewport->WorkSize.y
		: ImGui::GetCursorScreenPos().y + 260.0f };
	const float available_to_bottom{ std::max(
		1.0f,
		viewport_bottom - ImGui::GetCursorScreenPos().y -
			ImGui::GetStyle().WindowPadding.y * 2.0f - resize_grip_height
	) };
	const float minimum_palette_height{ std::min(140.0f, available_to_bottom) };
	const float natural_palette_height{ std::max(
		minimum_palette_height,
		static_cast<float>(palettes.size()) * ImGui::GetFrameHeightWithSpacing() +
			ImGui::GetStyle().WindowPadding.y * 2.0f
	) };

	if (ui_state.palette_height <= 0.0f) {
		ui_state.palette_height = std::min(
			natural_palette_height,
			std::min(260.0f, available_to_bottom)
		);
	}
	ui_state.palette_height = std::clamp(
		ui_state.palette_height,
		minimum_palette_height,
		available_to_bottom
	);
	const float palette_child_height{ ui_state.palette_height };

	if (ImGui::BeginChild(
			"##PaletteList",
			ImVec2{ 0.0f, palette_child_height },
			ImGuiChildFlags_Borders
		)) {
		std::optional<std::size_t> palette_to_delete;

		for (std::size_t palette_index{ 0 }; palette_index < palettes.size(); ++palette_index) {
			auto& palette{ palettes[palette_index] };
			ImGui::PushID(static_cast<int>(palette_index));

			bool begin_rename{ false };
			bool began_rename_this_frame{ false };
			bool commit_rename{ false };
			bool cancel_rename{ false };
			ImVec2 rename_input_min{};
			ImVec2 rename_input_max{};
			bool rename_input_drawn{ false };
			bool rename_input_hovered{ false };

			const bool renaming_before_draw{
				ui_state.renaming_palette.has_value() &&
				*ui_state.renaming_palette == palette_index
			};

			const ImGuiID node_id{ ImGui::GetID("##PaletteNode") };
			bool stored_open{ ImGui::GetStateStorage()->GetBool(node_id, false) };
			ImGui::SetNextItemOpen(stored_open, ImGuiCond_Always);

			ImGuiTreeNodeFlags node_flags{
				ImGuiTreeNodeFlags_FramePadding |
				ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_AllowOverlap
			};
			if (renaming_before_draw) {
				// Keep clicks in the overlaid rename field from toggling the tree node
				// underneath it while the palette name is being edited.
				node_flags |= ImGuiTreeNodeFlags_OpenOnArrow;
			}
			if (ui_state.selected_palette == palette_index) {
				node_flags |= ImGuiTreeNodeFlags_Selected;
			}

			bool open{ ImGui::TreeNodeEx(
				"##PaletteNode",
				node_flags,
				"%s",
				renaming_before_draw ? "" : palette.name.c_str()
			) };
			stored_open = open;
			ImGui::GetStateStorage()->SetBool(node_id, stored_open);

			const bool tree_hovered{ ImGui::IsItemHovered() };
			const ImVec2 tree_min{ ImGui::GetItemRectMin() };
			const ImVec2 tree_max{ ImGui::GetItemRectMax() };
			const float row_height{ ImGui::GetFrameHeight() };
			const float text_start_x{ tree_min.x + row_height };
			const float minimum_name_width{ 48.0f };
			const bool row_left_clicked{ ImGui::IsItemClicked(ImGuiMouseButton_Left) };

			if (row_left_clicked) {
				ui_state.selected_palette = palette_index;
			}

			if (tree_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
				ui_state.selected_palette = palette_index;
			}

			if (ImGui::BeginPopupContextItem("PaletteContext")) {
				if (ImGui::MenuItem("Rename")) {
					begin_rename = true;
				}
				if (ImGui::MenuItem("Delete")) {
					palette_to_delete = palette_index;
				}
				ImGui::EndPopup();
			}

			if (begin_rename) {
				color_picker_detail::BeginPaletteRename(
					ui_state,
					palette_index,
					palette.name
				);
				began_rename_this_frame = true;
			}

			if (ui_state.renaming_palette.has_value() &&
				*ui_state.renaming_palette == palette_index) {
				const ImVec2 after_tree_cursor{ ImGui::GetCursorScreenPos() };
				ImGui::SetCursorScreenPos(ImVec2{ text_start_x, tree_min.y });
				ImGui::SetNextItemWidth(std::max(
					minimum_name_width,
					tree_max.x - text_start_x - ImGui::GetStyle().FramePadding.x
				));
				if (ui_state.rename_focus_requested) {
					ImGui::SetKeyboardFocusHere();
					ui_state.rename_focus_requested = false;
				}

				const bool submitted{ ImGui::InputText(
					"##PaletteRename",
					&ui_state.rename_buffer,
					ImGuiInputTextFlags_EnterReturnsTrue |
						ImGuiInputTextFlags_AutoSelectAll
				) };
				rename_input_min = ImGui::GetItemRectMin();
				rename_input_max = ImGui::GetItemRectMax();
				rename_input_drawn = true;
				rename_input_hovered = ImGui::IsItemHovered() || ImGui::IsItemActive();

				if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
					cancel_rename = true;
				} else if (submitted) {
					commit_rename = true;
				}

				ImGui::SetCursorScreenPos(after_tree_cursor);
			}

			if (ui_state.renaming_palette.has_value() &&
				*ui_state.renaming_palette == palette_index) {
				if (cancel_rename) {
					ui_state.renaming_palette.reset();
					ui_state.rename_buffer.clear();
					ui_state.rename_focus_requested = false;
				} else {
					if (rename_input_drawn && !began_rename_this_frame) {
						const bool clicked{
							ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
							ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
							ImGui::IsMouseClicked(ImGuiMouseButton_Right)
						};
						const ImVec2 click_mouse{ ImGui::GetMousePos() };
						const bool inside_input{
							click_mouse.x >= rename_input_min.x &&
							click_mouse.x <= rename_input_max.x &&
							click_mouse.y >= rename_input_min.y &&
							click_mouse.y <= rename_input_max.y
						};
						if (clicked && !inside_input && !rename_input_hovered) {
							commit_rename = true;
						}
					}

					if (commit_rename) {
						if (!ui_state.rename_buffer.empty() &&
							ui_state.rename_buffer != palette.name) {
							auto before{ palettes };
							palette.name = ui_state.rename_buffer;
							color_picker_detail::RecordPaletteChange(
								ctx,
								"Rename Color Palette",
								std::move(before)
							);
						}
						ui_state.renaming_palette.reset();
						ui_state.rename_buffer.clear();
						ui_state.rename_focus_requested = false;
					}
				}
			}

			if (open) {
				ImGui::Indent(row_height * 0.65f);
				std::optional<std::size_t> color_to_delete;
				const float swatch_size{ ImGui::GetFrameHeight() };
				const float swatch_spacing{ ImGui::GetStyle().ItemSpacing.x };
				const float available{ ImGui::GetContentRegionAvail().x };
				const std::size_t columns{ std::max<std::size_t>(
					1,
					static_cast<std::size_t>(
						(available + swatch_spacing) /
						(swatch_size + swatch_spacing)
					)
				) };

				if (palette.colors.empty()) {
					ImGui::TextDisabled("No colors");
				}

				for (std::size_t color_index{ 0 };
					 color_index < palette.colors.size();
					 ++color_index) {
					ImGui::PushID(static_cast<int>(color_index));
					const Color palette_color{ palette.colors[color_index] };
					if (ImGui::ColorButton(
							"##Color",
							color_picker_detail::ToImVec4(palette_color),
							ImGuiColorEditFlags_AlphaPreviewHalf |
								ImGuiColorEditFlags_NoTooltip,
							ImVec2{ swatch_size, swatch_size }
						)) {
						result.interaction_started = true;
						ui_state.selected_palette = palette_index;
						if (value != palette_color) {
							value = palette_color;
							result.changed = true;
						}
					}
					if (ImGui::IsItemHovered()) {
						const std::string description{
							color_picker_detail::ColorDescription(palette_color)
						};
						ImGui::SetTooltip("%s", description.c_str());
					}
					if (ImGui::BeginPopupContextItem("ColorContext")) {
						if (ImGui::MenuItem("Delete")) {
							color_to_delete = color_index;
						}
						ImGui::EndPopup();
					}
					if ((color_index + 1) % columns != 0 &&
						color_index + 1 < palette.colors.size()) {
						ImGui::SameLine(0.0f, swatch_spacing);
					}
					ImGui::PopID();
				}

				if (color_to_delete.has_value()) {
					auto before{ palettes };
					palette.colors.erase(
						palette.colors.begin() +
							static_cast<std::ptrdiff_t>(*color_to_delete)
					);
					color_picker_detail::RecordPaletteChange(
						ctx,
						"Delete Palette Color",
						std::move(before)
					);
				}

				ImGui::Unindent(row_height * 0.65f);
			}

			ImGui::PopID();
		}

		if (palette_to_delete.has_value()) {
			auto before{ palettes };
			palettes.erase(
				palettes.begin() + static_cast<std::ptrdiff_t>(*palette_to_delete)
			);
			color_picker_detail::AdjustSelectionAfterPaletteDelete(
				ui_state,
				*palette_to_delete
			);
			color_picker_detail::RecordPaletteChange(
				ctx,
				"Delete Color Palette",
				std::move(before)
			);
		}
	}
	ImGui::EndChild();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
	ImGui::Button("##PaletteHeightResize", ImVec2{ -FLT_MIN, resize_grip_height });
	ImGui::PopStyleColor(3);

	if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}
	if (ImGui::IsItemActive() && ImGui::GetIO().MouseDelta.y != 0.0f) {
		ui_state.palette_height = std::clamp(
			ui_state.palette_height + ImGui::GetIO().MouseDelta.y,
			minimum_palette_height,
			available_to_bottom
		);
	}

	const ImVec2 grip_min{ ImGui::GetItemRectMin() };
	const ImVec2 grip_max{ ImGui::GetItemRectMax() };
	const float grip_y{ (grip_min.y + grip_max.y) * 0.5f };
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2{ grip_min.x + ImGui::GetStyle().FramePadding.x, grip_y },
		ImVec2{ grip_max.x - ImGui::GetStyle().FramePadding.x, grip_y },
		ImGui::GetColorU32(ImGuiCol_Separator)
	);

	ImGui::PopID();
	return result;
}

/// @brief ColorEdit replacement that preserves numeric RGBA editing while routing the swatch to
/// the extended registered-color/palette picker.
inline bool DrawColorEdit(
	EditorContext& ctx,
	const char* label,
	Color& value,
	ImGuiColorEditFlags flags = ImGuiColorEditFlags_Uint8 |
		ImGuiColorEditFlags_AlphaBar |
		ImGuiColorEditFlags_AlphaPreviewHalf
) {
	ImGui::PushID(label);

	bool changed{ false };
	auto rgba{ color_picker_detail::ToFloatColor(value) };
	const bool no_inputs{ (flags & ImGuiColorEditFlags_NoInputs) != 0 };
	const float swatch_size{ ImGui::GetFrameHeight() };

	if (!no_inputs) {
		const float requested_width{ ImGui::CalcItemWidth() };
		const float input_width{ std::max(
			1.0f,
			requested_width - swatch_size - ImGui::GetStyle().ItemInnerSpacing.x
		) };
		ImGui::SetNextItemWidth(input_width);
		if (ImGui::ColorEdit4(
				"##Channels",
				rgba.data(),
				flags | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoSmallPreview
			)) {
			value = color_picker_detail::ToColor(rgba.data());
			changed = true;
		}
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
	}

	if (ImGui::ColorButton(
		"##OpenPicker",
		color_picker_detail::ToImVec4(value),
		ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip,
		ImVec2{ swatch_size, swatch_size }
		)) {
		ImGui::OpenPopup("PickerPopup");
	}

	const std::string_view label_view{ label ? label : "" };
	if (!label_view.empty() && !label_view.starts_with("##")) {
		ImGui::SameLine();
		ImGui::TextUnformatted(label);
	}

	if (ImGui::BeginPopup("PickerPopup")) {
		auto result{ DrawColorPickerContents(ctx, "ExtendedPicker", value) };
		changed |= result.changed;
		ImGui::EndPopup();
	}

	ImGui::PopID();
	return changed;
}

} // namespace ptgn::editor
