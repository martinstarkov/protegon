#pragma once

#include <imgui.h>

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "editor/editor_icons.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/visible.h"
#include "runtime/world/entity_layer.h"

namespace ptgn::editor {

namespace hierarchy {

// TODO: Use string.h functions.
[[nodiscard]] inline std::string TrimWhitespace(std::string value) {
	auto first{ value.find_first_not_of(" \t\r\n") };

	if (first == std::string::npos) {
		return {};
	}

	auto last{ value.find_last_not_of(" \t\r\n") };

	return value.substr(first, last - first + 1);
}

// TODO: Use string.h functions.
[[nodiscard]] inline std::string ToLower(std::string value) {
	for (char& c : value) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}

	return value;
}

[[nodiscard]] inline bool MatchesFilter(Entity entity, std::string_view filter_text) {
	if (filter_text.empty()) {
		return true;
	}

	auto name{ ToLower(entity.Get<Tag>().value) };
	auto filter{ std::string{ filter_text } };
	bool has_name_include{ false };
	bool matched_name_include{ false };
	bool require_hidden{ false };
	bool require_shown{ false };
	std::size_t start{ 0 };

	while (start <= filter.size()) {
		auto comma{ filter.find(',', start) };
		auto token{ comma == std::string::npos ? filter.substr(start)
											   : filter.substr(start, comma - start) };
		token = ToLower(TrimWhitespace(std::move(token)));

		if (!token.empty()) {
			if (token.front() == '*') {
				auto filter_name{ TrimWhitespace(token.substr(1)) };

				if (filter_name == "hidden") {
					require_hidden = true;
				} else if (filter_name == "shown") {
					require_shown = true;
				}
			} else {
				bool exclude{ token.front() == '-' };
				auto needle{ TrimWhitespace(exclude ? token.substr(1) : token) };

				if (!needle.empty()) {
					bool contains{ name.find(needle) != std::string::npos };

					if (exclude && contains) {
						return false;
					}

					if (!exclude) {
						has_name_include	  = true;
						matched_name_include |= contains;
					}
				}
			}
		}

		if (comma == std::string::npos) {
			break;
		}

		start = comma + 1;
	}

	if (require_hidden && require_shown) {
		return false;
	}

	bool visible{ IsVisible(entity) };

	if (require_hidden && visible) {
		return false;
	}

	if (require_shown && !visible) {
		return false;
	}

	return !has_name_include || matched_name_include;
}

inline void DrawFilterTooltip() {
	if (!ImGui::IsItemHovered()) {
		return;
	}

	ImGui::BeginTooltip();
	ImGui::TextUnformatted("Hierarchy filter syntax:");
	ImGui::Separator();
	ImGui::TextUnformatted("player");
	ImGui::SameLine();
	ImGui::TextDisabled("Name contains \"player\"");
	ImGui::TextUnformatted("-enemy");
	ImGui::SameLine();
	ImGui::TextDisabled("Name does not contain \"enemy\"");
	ImGui::TextUnformatted("*shown");
	ImGui::SameLine();
	ImGui::TextDisabled("Entity is visible");
	ImGui::TextUnformatted("*hidden");
	ImGui::SameLine();
	ImGui::TextDisabled("Entity is hidden");
	ImGui::Spacing();
	ImGui::TextDisabled("Separate filters with commas.");
	ImGui::TextDisabled("Positive name filters use OR; all other filters must match.");
	ImGui::EndTooltip();
}

struct LayerControlsOptions {
	bool show_visibility{ true };
	bool visibility_interactive{ true };
};

inline bool DrawLayerControls(const SceneLayer& layer, const LayerControlsOptions& options = {}) {
	DrawEditorIconButton(
		"##LayerType", layer.kind == SceneLayerKind::Entity ? EditorIcon::Entity : EditorIcon::Tile,
		EditorIconButtonOptions{
			.tooltip	 = layer.kind == SceneLayerKind::Entity ? "Entity layer" : "Tile layer",
			.compact	 = true,
			.interactive = false,
			.muted		 = true,
		}
	);

	if (!options.show_visibility) {
		ImGui::SameLine(0.0f, 4.0f);

		return false;
	}

	ImGui::SameLine(0.0f, 2.0f);
	bool pressed{ DrawEditorIconButton(
		"##LayerVisible", layer.visible ? EditorIcon::Visible : EditorIcon::Hidden,
		EditorIconButtonOptions{
			.tooltip	 = layer.visible ? "Hide layer" : "Show layer",
			.compact	 = true,
			.interactive = options.visibility_interactive,
			.muted		 = !layer.visible,
		}
	) };
	ImGui::SameLine(0.0f, 4.0f);

	return pressed;
}

struct LayerRowOptions {
	LayerControlsOptions controls{};
	bool selected{ false };
};

struct LayerRowResult {
	bool open{ false };
	bool left_clicked{ false };
	bool visibility_clicked{ false };
};

[[nodiscard]] inline LayerRowResult DrawLayerRow(
	const SceneLayer& layer, const LayerRowOptions& options = {}
) {
	LayerRowResult result;
	result.visibility_clicked = DrawLayerControls(layer, options.controls);

	ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_OpenOnArrow |
							  ImGuiTreeNodeFlags_OpenOnDoubleClick |
							  ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen |
							  ImGuiTreeNodeFlags_FramePadding };
	if (options.selected) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	ImVec2 previous_frame_padding{ ImGui::GetStyle().FramePadding };
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ previous_frame_padding.x, 1.0f });
	result.open = ImGui::TreeNodeEx("##Layer", flags, "%s", layer.name.c_str());
	ImGui::PopStyleVar();
	result.left_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

	return result;
}

} // namespace hierarchy

} // namespace ptgn::editor
