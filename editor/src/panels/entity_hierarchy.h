#pragma once

#include <imgui.h>

#include <cstddef>
#include <string>
#include <string_view>

#include "core/util/string.h"
#include "editor/editor_icons.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/visible.h"

namespace ptgn::editor {

namespace hierarchy {

[[nodiscard]] inline bool MatchesFilter(Entity entity, std::string_view filter_text) {
	if (filter_text.empty()) {
		return true;
	}

	std::string name{ ToLowerAscii(entity.Get<Tag>().value) };
	bool has_name_include{ false };
	bool matched_name_include{ false };
	bool require_hidden{ false };
	bool require_shown{ false };
	std::size_t start{ 0 };

	while (start <= filter_text.size()) {
		std::size_t comma{ filter_text.find(',', start) };
		std::string_view part{ comma == std::string_view::npos
								   ? filter_text.substr(start)
								   : filter_text.substr(start, comma - start) };
		std::string token{ ToLowerAscii(TrimWhitespace(part)) };
		std::string_view token_view{ token };

		if (!token_view.empty()) {
			if (token_view.front() == '*') {
				std::string_view filter_name{ TrimWhitespace(token_view.substr(1)) };

				if (filter_name == "hidden") {
					require_hidden = true;
				} else if (filter_name == "shown") {
					require_shown = true;
				}
			} else {
				bool exclude{ token_view.front() == '-' };
				std::string_view needle{
					TrimWhitespace(exclude ? token_view.substr(1) : token_view)
				};

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

		if (comma == std::string_view::npos) {
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

} // namespace hierarchy

} // namespace ptgn::editor
