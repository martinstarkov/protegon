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
#include <fstream>
#include <optional>
#include <sstream>
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
	std::vector<bool> palette_open{};
	std::optional<std::size_t> selected_palette{};
	std::optional<std::size_t> renaming_palette{};
	std::string rename_buffer{};
	bool rename_focus_requested{ false };

	std::optional<std::pair<std::size_t, std::size_t>> renaming_color{};
	std::string color_rename_buffer{};
	bool color_rename_focus_requested{ false };
	std::string palette_import_error{};
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

struct PaletteFileResult {
	std::vector<Color> colors{};
	std::string error{};
};

[[nodiscard]] inline std::string_view TrimLine(std::string_view line) {
	while (!line.empty() &&
		(line.front() == ' ' || line.front() == '\t' || line.front() == '\r' || line.front() == '\n')) {
		line.remove_prefix(1);
	}
	while (!line.empty() &&
		(line.back() == ' ' || line.back() == '\t' || line.back() == '\r' || line.back() == '\n')) {
		line.remove_suffix(1);
	}
	return line;
}

[[nodiscard]] inline bool IsByteSequence(
	const std::vector<std::uint8_t>& bytes,
	std::size_t offset,
	std::string_view value
) {
	return offset + value.size() <= bytes.size() &&
		std::equal(value.begin(), value.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

[[nodiscard]] inline std::uint16_t ReadLe16(
	const std::vector<std::uint8_t>& bytes,
	std::size_t offset
) {
	return static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(bytes[offset]) |
		(static_cast<std::uint32_t>(bytes[offset + 1]) << 8u)
	);
}

[[nodiscard]] inline std::uint32_t ReadLe32(
	const std::vector<std::uint8_t>& bytes,
	std::size_t offset
) {
	return static_cast<std::uint32_t>(bytes[offset]) |
		(static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
		(static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
		(static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
}

[[nodiscard]] inline std::optional<Color> ParsePaletteTextColorLine(std::string line) {
	for (char& character : line) {
		if (character == ',' || character == ';') {
			character = ' ';
		}
	}

	std::istringstream stream{ line };
	int red{};
	int green{};
	int blue{};
	int alpha{ 255 };
	if (!(stream >> red >> green >> blue)) {
		return std::nullopt;
	}

	int optional_alpha{};
	if (stream >> optional_alpha) {
		alpha = optional_alpha;
	}

	if (red < 0 || red > 255 || green < 0 || green > 255 ||
		blue < 0 || blue > 255 || alpha < 0 || alpha > 255) {
		return std::nullopt;
	}

	return Color{
		static_cast<std::uint8_t>(red),
		static_cast<std::uint8_t>(green),
		static_cast<std::uint8_t>(blue),
		static_cast<std::uint8_t>(alpha),
	};
}

[[nodiscard]] inline PaletteFileResult ParseJascPalette(
	const std::vector<std::uint8_t>& bytes
) {
	PaletteFileResult result;
	const std::string text{ bytes.begin(), bytes.end() };
	std::istringstream stream{ text };

	std::string line;
	if (!std::getline(stream, line) || TrimLine(line) != "JASC-PAL") {
		result.error = "The file is not a valid JASC-PAL palette.";
		return result;
	}

	// Version is normally 0100. Keep this tolerant so palettes written by compatible
	// tools with a different version marker can still be imported.
	if (!std::getline(stream, line)) {
		result.error = "The JASC-PAL file is missing its version line.";
		return result;
	}

	if (!std::getline(stream, line)) {
		result.error = "The JASC-PAL file is missing its color count.";
		return result;
	}

	std::size_t expected_count{};
	try {
		const std::string count_text{ TrimLine(line) };
		std::size_t consumed{};
		expected_count = std::stoull(count_text, &consumed);
		if (consumed != count_text.size()) {
			result.error = "The JASC-PAL color count is invalid.";
			return result;
		}
	} catch (...) {
		result.error = "The JASC-PAL color count is invalid.";
		return result;
	}

	result.colors.reserve(expected_count);
	while (result.colors.size() < expected_count && std::getline(stream, line)) {
		const std::string_view trimmed{ TrimLine(line) };
		if (trimmed.empty()) {
			continue;
		}

		const auto color{ ParsePaletteTextColorLine(std::string{ trimmed }) };
		if (!color.has_value()) {
			result.error = "The JASC-PAL file contains an invalid RGB/RGBA color row.";
			result.colors.clear();
			return result;
		}
		result.colors.push_back(*color);
	}

	if (result.colors.size() != expected_count) {
		result.error = "The JASC-PAL file ended before all declared colors were read.";
		result.colors.clear();
	}
	return result;
}

[[nodiscard]] inline PaletteFileResult ParseRiffPalette(
	const std::vector<std::uint8_t>& bytes
) {
	PaletteFileResult result;
	if (bytes.size() < 12 || !IsByteSequence(bytes, 0, "RIFF") ||
		!IsByteSequence(bytes, 8, "PAL ")) {
		result.error = "The file is not a valid RIFF PAL palette.";
		return result;
	}

	for (std::size_t chunk{ 12 }; chunk + 8 <= bytes.size();) {
		const std::uint32_t chunk_size{ ReadLe32(bytes, chunk + 4) };
		const std::size_t data_begin{ chunk + 8 };
		const std::size_t data_end{ data_begin + static_cast<std::size_t>(chunk_size) };
		if (data_end > bytes.size()) {
			result.error = "The RIFF PAL file contains a truncated chunk.";
			return result;
		}

		if (IsByteSequence(bytes, chunk, "data")) {
			if (chunk_size < 4) {
				result.error = "The RIFF PAL data chunk is too small.";
				return result;
			}

			const std::uint16_t color_count{ ReadLe16(bytes, data_begin + 2) };
			const std::size_t colors_begin{ data_begin + 4 };
			const std::size_t required_size{
				colors_begin + static_cast<std::size_t>(color_count) * 4
			};
			if (required_size > data_end) {
				result.error = "The RIFF PAL file ended before all palette entries were read.";
				return result;
			}

			result.colors.reserve(color_count);
			for (std::size_t index{ 0 }; index < color_count; ++index) {
				const std::size_t entry{ colors_begin + index * 4 };
				// The fourth PALETTEENTRY byte contains flags, not alpha.
				result.colors.push_back(Color{
					bytes[entry],
					bytes[entry + 1],
					bytes[entry + 2],
					255,
				});
			}
			return result;
		}

		chunk = data_end + (chunk_size & 1u);
	}

	result.error = "The RIFF PAL file does not contain a palette data chunk.";
	return result;
}

[[nodiscard]] inline PaletteFileResult ParseSimpleTextPalette(
	const std::vector<std::uint8_t>& bytes
) {
	PaletteFileResult result;
	const std::string text{ bytes.begin(), bytes.end() };
	std::istringstream stream{ text };
	std::string line;

	while (std::getline(stream, line)) {
		const std::string_view trimmed{ TrimLine(line) };
		if (trimmed.empty() || trimmed.starts_with("#") || trimmed.starts_with("//")) {
			continue;
		}

		const auto color{ ParsePaletteTextColorLine(std::string{ trimmed }) };
		if (!color.has_value()) {
			result.colors.clear();
			result.error = "Unsupported .PAL format or invalid RGB/RGBA text row.";
			return result;
		}
		result.colors.push_back(*color);
	}

	if (result.colors.empty()) {
		result.error = "The .PAL file did not contain any readable colors.";
	}
	return result;
}

[[nodiscard]] inline PaletteFileResult ReadPaletteFile(const path& file_path) {
	PaletteFileResult result;
	std::ifstream file{ file_path, std::ios::binary | std::ios::ate };
	if (!file) {
		result.error = "Could not open the selected .PAL file.";
		return result;
	}

	const std::streamsize size{ file.tellg() };
	if (size < 0) {
		result.error = "Could not determine the selected .PAL file size.";
		return result;
	}

	std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
	file.seekg(0, std::ios::beg);
	if (size > 0 && !file.read(
			reinterpret_cast<char*>(bytes.data()),
			size
		)) {
		result.error = "Could not read the selected .PAL file.";
		return result;
	}

	if (IsByteSequence(bytes, 0, "JASC-PAL")) {
		return ParseJascPalette(bytes);
	}
	if (IsByteSequence(bytes, 0, "RIFF") && IsByteSequence(bytes, 8, "PAL ")) {
		return ParseRiffPalette(bytes);
	}

	// A number of palette tools also write .pal as plain RGB/RGBA rows. Accept that
	// lightweight form as a fallback after checking the two standardized/common formats.
	return ParseSimpleTextPalette(bytes);
}

[[nodiscard]] inline bool PaletteContainsColor(
	const EditorColorPalette& palette,
	Color color
) {
	return std::ranges::any_of(
		palette.colors,
		[color](const EditorPaletteColor& entry) {
			return entry.color == color;
		}
	);
}

[[nodiscard]] inline bool PaletteColorNameExists(
	const EditorColorPalette& palette,
	std::string_view name
) {
	return std::ranges::any_of(
		palette.colors,
		[name](const EditorPaletteColor& entry) {
			return entry.name == name;
		}
	);
}

[[nodiscard]] inline std::string MakePaletteColorName(
	const EditorColorPalette& palette,
	Color color
) {
	if (const auto* registered{ FindRegisteredColor(color) };
		registered && !PaletteColorNameExists(palette, registered->key)) {
		return registered->key;
	}

	for (std::size_t index{ 1 };; ++index) {
		std::string candidate{ "Color " + std::to_string(index) };
		if (!PaletteColorNameExists(palette, candidate)) {
			return candidate;
		}
	}
}

inline std::size_t AppendUniqueColors(
	EditorColorPalette& palette,
	const std::vector<Color>& colors
) {
	std::size_t added{};
	for (const Color color : colors) {
		if (PaletteContainsColor(palette, color)) {
			continue;
		}

		palette.colors.push_back(EditorPaletteColor{
			.name = MakePaletteColorName(palette, color),
			.color = color,
		});
		++added;
	}
	return added;
}

[[nodiscard]] inline std::optional<std::vector<Color>> ChoosePaletteFileColors(
	EditorContext& ctx,
	ColorPickerUiState& state
) {
	state.palette_import_error.clear();
	const auto dialog_result{ OpenPaletteFileDialog(ctx) };
	if (!dialog_result.has_value()) {
		state.palette_import_error = dialog_result.error();
		return std::nullopt;
	}
	if (!dialog_result->has_value()) {
		return std::nullopt;
	}

	const PaletteFileResult read_result{ ReadPaletteFile(**dialog_result) };
	if (!read_result.error.empty()) {
		state.palette_import_error = read_result.error;
		return std::nullopt;
	}
	return read_result.colors;
}

inline void CancelColorRename(ColorPickerUiState& state) {
	state.renaming_color.reset();
	state.color_rename_buffer.clear();
	state.color_rename_focus_requested = false;
}

inline void BeginColorRename(
	ColorPickerUiState& state,
	std::size_t palette_index,
	std::size_t color_index,
	std::string_view current_name
) {
	// Starting a different rename always abandons the previous uncommitted edit.
	CancelColorRename(state);
	state.renaming_color = std::pair{ palette_index, color_index };
	state.color_rename_buffer = std::string{ current_name };
	state.color_rename_focus_requested = true;
}

inline void AdjustColorRenameAfterDelete(
	ColorPickerUiState& state,
	std::size_t palette_index,
	std::size_t deleted_color_index
) {
	if (!state.renaming_color.has_value() ||
		state.renaming_color->first != palette_index) {
		return;
	}

	if (state.renaming_color->second == deleted_color_index) {
		CancelColorRename(state);
	} else if (state.renaming_color->second > deleted_color_index) {
		--state.renaming_color->second;
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

inline void AdjustPaletteUiStateAfterDelete(
	ColorPickerUiState& state,
	std::size_t deleted_index
) {
	if (deleted_index < state.palette_open.size()) {
		state.palette_open.erase(
			state.palette_open.begin() + static_cast<std::ptrdiff_t>(deleted_index)
		);
	}

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

	if (state.renaming_color.has_value()) {
		auto& [palette_index, color_index]{ *state.renaming_color };
		(void)color_index;
		if (palette_index == deleted_index) {
			CancelColorRename(state);
		} else if (palette_index > deleted_index) {
			--palette_index;
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
	if (ui_state.renaming_color.has_value()) {
		const auto [palette_index, color_index]{ *ui_state.renaming_color };
		if (palette_index >= palettes.size() ||
			color_index >= palettes[palette_index].colors.size()) {
			color_picker_detail::CancelColorRename(ui_state);
		}
	}

	if (ui_state.palette_open.size() < palettes.size()) {
		ui_state.palette_open.resize(palettes.size(), false);
	} else if (ui_state.palette_open.size() > palettes.size()) {
		ui_state.palette_open.resize(palettes.size());
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
		color_picker_detail::PaletteContainsColor(
			palettes[*ui_state.selected_palette],
			value
		)
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
		ImGui::OpenPopup("AddPaletteMenu");
	}
	if (ImGui::BeginPopup("AddPaletteMenu")) {
		if (ImGui::MenuItem("Blank Palette")) {
			auto before{ palettes };
			const std::size_t new_palette_index{ palettes.size() };
			palettes.push_back(EditorColorPalette{
				.name = color_picker_detail::MakeUniquePaletteName(ctx.project_state),
			});

			if (ui_state.palette_open.size() < palettes.size()) {
				ui_state.palette_open.resize(palettes.size(), false);
			}
			ui_state.selected_palette = new_palette_index;
			ui_state.palette_open[new_palette_index] = true;

			color_picker_detail::RecordPaletteChange(
				ctx,
				"Add Color Palette",
				std::move(before)
			);
		}

		if (ImGui::MenuItem("From PAL File")) {
			if (const auto imported_colors{
					color_picker_detail::ChoosePaletteFileColors(ctx, ui_state)
				}) {
				auto before{ palettes };
				const std::size_t new_palette_index{ palettes.size() };
				EditorColorPalette palette{
					.name = color_picker_detail::MakeUniquePaletteName(ctx.project_state),
				};
				color_picker_detail::AppendUniqueColors(palette, *imported_colors);
				palettes.push_back(std::move(palette));

				if (ui_state.palette_open.size() < palettes.size()) {
					ui_state.palette_open.resize(palettes.size(), false);
				}
				ui_state.selected_palette = new_palette_index;
				ui_state.palette_open[new_palette_index] = true;

				color_picker_detail::RecordPaletteChange(
					ctx,
					"Import Color Palette",
					std::move(before)
				);
			}
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine(0.0f, button_spacing);
	ImGui::BeginDisabled(!has_selected_palette || selected_palette_has_color);
	if (ImGui::Button(
			add_color_label.c_str(),
			ImVec2{ add_color_button_width, button_height }
		)) {
		const std::size_t palette_index{ *ui_state.selected_palette };
		auto before{ palettes };
		auto& palette{ palettes[palette_index] };
		palette.colors.push_back(EditorPaletteColor{
			.name = color_picker_detail::MakePaletteColorName(palette, value),
			.color = value,
		});
		ui_state.palette_open[palette_index] = true;
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

	if (!ui_state.palette_import_error.empty()) {
		ImGui::TextColored(
			ImVec4{ 1.0f, 0.35f, 0.35f, 1.0f },
			"PAL import failed: %s",
			ui_state.palette_import_error.c_str()
		);
	}

	if (palettes.empty()) {
		ImGui::TextDisabled("No palettes");
		ImGui::PopID();
		return result;
	}

	const ImGuiViewport* viewport{ ImGui::GetWindowViewport() };
	const float resize_grip_height{ 7.0f };
	const float color_rename_reserved_height{
		ui_state.renaming_color.has_value() ? ImGui::GetFrameHeightWithSpacing() : 0.0f
	};
	const float viewport_bottom{ viewport
		? viewport->WorkPos.y + viewport->WorkSize.y
		: ImGui::GetCursorScreenPos().y + 260.0f };
	const float available_to_bottom{ std::max(
		1.0f,
		viewport_bottom - ImGui::GetCursorScreenPos().y -
			ImGui::GetStyle().WindowPadding.y * 2.0f - resize_grip_height -
			color_rename_reserved_height
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

			const float row_height{ ImGui::GetFrameHeight() };
			bool open{ ui_state.palette_open[palette_index] };
			const bool renaming{
				ui_state.renaming_palette.has_value() &&
				*ui_state.renaming_palette == palette_index
			};

			bool commit_rename{ false };
			bool cancel_rename{ false };

			if (renaming) {
				// Draw the rename editor as a normal in-flow item. Do not overlay it by
				// rewinding the cursor over a TreeNode row: recent ImGui versions assert
				// when SetCursorPos()/SetCursorScreenPos() extends child boundaries that way.
				if (ui_state.rename_focus_requested) {
					ImGui::SetKeyboardFocusHere();
					ui_state.rename_focus_requested = false;
				}

				ImGui::SetNextItemWidth(-FLT_MIN);
				const bool submitted{ ImGui::InputText(
					"##PaletteRename",
					&ui_state.rename_buffer,
					ImGuiInputTextFlags_EnterReturnsTrue |
						ImGuiInputTextFlags_AutoSelectAll
				) };
				const bool rename_hovered{ ImGui::IsItemHovered() };

				if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
					cancel_rename = true;
				} else if (submitted) {
					commit_rename = true;
				} else {
					const bool clicked_elsewhere{
						(ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
						 ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
						 ImGui::IsMouseClicked(ImGuiMouseButton_Right)) &&
						!rename_hovered
					};
					commit_rename = clicked_elsewhere;
				}
			} else {
				ImGui::SetNextItemOpen(open, ImGuiCond_Always);

				ImGuiTreeNodeFlags node_flags{
					ImGuiTreeNodeFlags_FramePadding |
					ImGuiTreeNodeFlags_SpanAvailWidth |
					ImGuiTreeNodeFlags_NoTreePushOnOpen |
					ImGuiTreeNodeFlags_AllowOverlap
				};
				if (ui_state.selected_palette == palette_index) {
					node_flags |= ImGuiTreeNodeFlags_Selected;
				}

				open = ImGui::TreeNodeEx(
					"##PaletteNode",
					node_flags,
					"%s",
					palette.name.c_str()
				);
				ui_state.palette_open[palette_index] = open;

				if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
					// With SpanAvailWidth and no OpenOnArrow restriction, the entire
					// palette row selects and toggles the tree node in one click.
					ui_state.selected_palette = palette_index;
				}

				if (ImGui::IsItemHovered() &&
					ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
					ui_state.selected_palette = palette_index;
				}

				if (ImGui::BeginPopupContextItem("PaletteContext")) {
					if (ImGui::MenuItem("Rename")) {
						color_picker_detail::BeginPaletteRename(
							ui_state,
							palette_index,
							palette.name
						);
					}
					if (ImGui::MenuItem("Import from PAL file")) {
						if (const auto imported_colors{
								color_picker_detail::ChoosePaletteFileColors(ctx, ui_state)
							}) {
							auto before{ palettes };
							color_picker_detail::AppendUniqueColors(
								palette,
								*imported_colors
							);
							ui_state.selected_palette = palette_index;
							ui_state.palette_open[palette_index] = true;
							open = true;
							color_picker_detail::RecordPaletteChange(
								ctx,
								"Import Palette Colors",
								std::move(before)
							);
						}
					}
					if (ImGui::MenuItem("Delete")) {
						palette_to_delete = palette_index;
					}
					ImGui::EndPopup();
				}
			}

			if (cancel_rename) {
				ui_state.renaming_palette.reset();
				ui_state.rename_buffer.clear();
				ui_state.rename_focus_requested = false;
			} else if (commit_rename) {
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
					const auto& palette_entry{ palette.colors[color_index] };
					const Color palette_color{ palette_entry.color };
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
							palette_entry.name + "\n" +
							color_picker_detail::ColorDescription(palette_color)
						};
						ImGui::SetTooltip("%s", description.c_str());
					}
					if (ImGui::BeginPopupContextItem("ColorContext")) {
						if (ImGui::MenuItem("Rename")) {
							color_picker_detail::BeginColorRename(
								ui_state,
								palette_index,
								color_index,
								palette_entry.name
							);
						}
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
					color_picker_detail::AdjustColorRenameAfterDelete(
						ui_state,
						palette_index,
						*color_to_delete
					);
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
			color_picker_detail::AdjustPaletteUiStateAfterDelete(
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

	if (ui_state.renaming_color.has_value()) {
		const auto [palette_index, color_index]{ *ui_state.renaming_color };
		if (palette_index < palettes.size() &&
			color_index < palettes[palette_index].colors.size()) {
			auto& palette_entry{ palettes[palette_index].colors[color_index] };
			const float row_width{ ImGui::GetContentRegionAvail().x };
			const float row_height{ ImGui::GetFrameHeight() };
			const float row_spacing{ ImGui::GetStyle().ItemSpacing.x };
			const float left_width{ row_width * 0.60f };
			const float thumbnail_width{ row_height };
			const float input_width{ std::max(
				1.0f,
				left_width - thumbnail_width - row_spacing
			) };
			const float actions_width{ std::max(
				1.0f,
				row_width - left_width - row_spacing
			) };
			const float action_width{ std::max(
				1.0f,
				(actions_width - row_spacing) * 0.5f
			) };

			ImGui::PushID("PaletteColorRenameRow");
			ImGui::ColorButton(
				"##Preview",
				color_picker_detail::ToImVec4(palette_entry.color),
				ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip,
				ImVec2{ thumbnail_width, row_height }
			);
			ImGui::SameLine(0.0f, row_spacing);

			if (ui_state.color_rename_focus_requested) {
				ImGui::SetKeyboardFocusHere();
				ui_state.color_rename_focus_requested = false;
			}
			ImGui::SetNextItemWidth(input_width);
			ImGui::InputText(
				"##Name",
				&ui_state.color_rename_buffer,
				ImGuiInputTextFlags_AutoSelectAll
			);

			ImGui::SameLine(0.0f, row_spacing);
			const bool cancel{ ImGui::Button(
				"Cancel",
				ImVec2{ action_width, row_height }
			) };

			ImGui::SameLine(0.0f, row_spacing);
			const bool can_save{ !ui_state.color_rename_buffer.empty() };
			ImGui::BeginDisabled(!can_save);
			const bool save{ ImGui::Button(
				"Save",
				ImVec2{ action_width, row_height }
			) };
			ImGui::EndDisabled();

			if (cancel) {
				color_picker_detail::CancelColorRename(ui_state);
			} else if (save && can_save) {
				if (ui_state.color_rename_buffer != palette_entry.name) {
					auto before{ palettes };
					palette_entry.name = ui_state.color_rename_buffer;
					color_picker_detail::RecordPaletteChange(
						ctx,
						"Rename Palette Color",
						std::move(before)
					);
				}
				color_picker_detail::CancelColorRename(ui_state);
			}
			ImGui::PopID();
		} else {
			color_picker_detail::CancelColorRename(ui_state);
		}
	}

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
