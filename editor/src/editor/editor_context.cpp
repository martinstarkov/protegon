#include "editor/editor_context.h"

#include <optional>
#include <ranges>
#include <utility>

#include "app/project.h"
#include "app/project_settings.h"
#if !defined(__EMSCRIPTEN__)
#include "editor/editor.h"
#include "platform/window.h"
#endif
#include "serialization/json/json.h"
#include "serialization/json/json_file.h"

namespace ptgn::editor {

namespace {

[[nodiscard]] bool LooksLikeLegacyEditorLocalState(const json& value) {
	return value.is_object() && !value.contains("color_palettes") &&
		(value.contains("settings") || value.contains("state") || value.contains("selection"));
}

[[nodiscard]] std::optional<std::uint8_t> ReadColorChannel(const json& value) {
	if (!value.is_number_integer() && !value.is_number_unsigned()) {
		return std::nullopt;
	}

	try {
		const int channel{ value.get<int>() };
		if (channel < 0 || channel > 255) {
			return std::nullopt;
		}
		return static_cast<std::uint8_t>(channel);
	} catch (...) {
		return std::nullopt;
	}
}

[[nodiscard]] std::optional<Color> ReadPaletteColorValue(const json& value) {
	if (!value.is_array() || (value.size() != 3 && value.size() != 4)) {
		return std::nullopt;
	}

	const auto red{ ReadColorChannel(value[0]) };
	const auto green{ ReadColorChannel(value[1]) };
	const auto blue{ ReadColorChannel(value[2]) };
	const auto alpha{ value.size() == 4
		? ReadColorChannel(value[3])
		: std::optional<std::uint8_t>{ 255 } };

	if (!red || !green || !blue || !alpha) {
		return std::nullopt;
	}

	return Color{ *red, *green, *blue, *alpha };
}

[[nodiscard]] std::string DefaultPaletteColorName(std::size_t index) {
	return "Color " + std::to_string(index + 1);
}

[[nodiscard]] bool PaletteContainsColor(
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

} // namespace

void to_json(json& value, const EditorColorPalette& palette) {
	value = json::object();
	value["name"] = palette.name;
	value["colors"] = json::array();

	for (const auto& palette_color : palette.colors) {
		json serialized = json::object();
		serialized["name"] = palette_color.name;
		serialized["color"] = palette_color.color;
		value["colors"].push_back(std::move(serialized));
	}
}

void from_json(const json& value, EditorColorPalette& palette) {
	palette = EditorColorPalette{};

	if (!value.is_object()) {
		return;
	}

	if (const auto name{ value.find("name") };
		name != value.end() && name->is_string()) {
		palette.name = name->get<std::string>();
	}

	const auto colors{ value.find("colors") };
	if (colors == value.end() || !colors->is_array()) {
		return;
	}

	palette.colors.reserve(colors->size());
	for (const auto& serialized_color : *colors) {
		EditorPaletteColor palette_color;
		palette_color.name = DefaultPaletteColorName(palette.colors.size());

		// Legacy .ptgneditor files stored palette colors directly as [r,g,b,a].
		if (serialized_color.is_array()) {
			if (const auto color{ ReadPaletteColorValue(serialized_color) };
				color && !PaletteContainsColor(palette, *color)) {
				palette_color.color = *color;
				palette.colors.push_back(std::move(palette_color));
			}
			continue;
		}

		if (!serialized_color.is_object()) {
			continue;
		}

		if (const auto name{ serialized_color.find("name") };
			name != serialized_color.end() && name->is_string() && !name->get_ref<const std::string&>().empty()) {
			palette_color.name = name->get<std::string>();
		}

		const auto color_value{ serialized_color.find("color") };
		if (color_value == serialized_color.end()) {
			continue;
		}

		if (const auto color{ ReadPaletteColorValue(*color_value) };
			color && !PaletteContainsColor(palette, *color)) {
			palette_color.color = *color;
			palette.colors.push_back(std::move(palette_color));
		}
	}
}

void to_json(json& value, const EditorProjectState& state) {
	value = json::object();
	value["color_palettes"] = state.color_palettes;
}

void from_json(const json& value, EditorProjectState& state) {
	state = EditorProjectState{};

	if (!value.is_object()) {
		return;
	}

	const auto palettes{ value.find("color_palettes") };
	if (palettes == value.end() || !palettes->is_array()) {
		return;
	}

	state.color_palettes.reserve(palettes->size());
	for (const auto& serialized_palette : *palettes) {
		if (!serialized_palette.is_object()) {
			continue;
		}

		EditorColorPalette palette;
		from_json(serialized_palette, palette);
		state.color_palettes.emplace_back(std::move(palette));
	}
}

std::expected<std::optional<path>, std::string> OpenPaletteFileDialog(
	EditorContext& ctx
) {
#if defined(__EMSCRIPTEN__)
	return std::unexpected<std::string>{
		"Native palette file dialogs are unavailable in web builds."
	};
#else
	FileDialog::Options options;
	options.filters = {
		FileDialog::Filter{ .name = "Palette Files", .spec = "pal" },
	};
	return ctx.editor.GetWindow().file.OpenFile(std::move(options));
#endif
}

path GetEditorProjectStatePath(const Project& project) {
	auto file_path{ project.file_path };
	file_path.replace_extension(".ptgneditor");
	return file_path;
}

EditorProjectState LoadEditorProjectState(const Project& project) {
	const auto file_path{ GetEditorProjectStatePath(project) };

	EditorProjectState state;

	if (!FileExists(file_path)) {
		return state;
	}

	const json value{ LoadJson(file_path) };

	// Before .ptgneditor became shared project data it contained EditorLocalState.
	// Treat that shape as empty shared state; Editor::OnProjectChanged migrates the
	// old local state into .ptgnlocal before replacing this file with the new shape.
	if (LooksLikeLegacyEditorLocalState(value)) {
		return state;
	}

	from_json(value, state);
	return state;
}

void SaveEditorProjectState(
	const Project& project,
	const EditorProjectState& state
) {
	const auto file_path{ GetEditorProjectStatePath(project) };

	EnsureDirectory(file_path.parent_path());
	json value = state;
	SaveJson(value, file_path);
}

EditorLocalState LoadEditorLocalState(const Project& project) {
	EditorLocalState state;

	const ProjectLocalState local_state{ LoadProjectLocalState(project) };
	if (local_state.editor.is_object() && !local_state.editor.empty()) {
		local_state.editor.get_to(state);
		return state;
	}

	// Migration path from the old layout where .ptgneditor contained user-local
	// settings/state/selection instead of shared editor project data.
	const auto legacy_file_path{ GetEditorProjectStatePath(project) };
	if (!FileExists(legacy_file_path)) {
		return state;
	}

	const json legacy{ LoadJson(legacy_file_path) };
	if (LooksLikeLegacyEditorLocalState(legacy)) {
		legacy.get_to(state);
	}

	return state;
}

void SaveEditorLocalState(
	const Project& project,
	const EditorLocalState& state
) {
	ProjectLocalState local_state{ LoadProjectLocalState(project) };
	local_state.editor = state;
	SaveProjectLocalState(project, local_state);
}

} // namespace ptgn::editor
