#pragma once

#include <string>
#include <vector>

#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/graphics/text/text.h"

namespace ptgn {

class AssetManager;

namespace impl {

struct TextPageOptions {
	std::string split_end{ "..." };
	std::string split_begin{ "..." };

	std::size_t max_lines_per_page{ 0 };

	bool add_split_markers{ true };
};

struct TextPage {
	StyledText styled_text{};
	TextMeasurement measurement{};
	std::size_t glyph_count{ 0 };
};

struct TextPaginationResult {
	std::vector<TextPage> pages{};
};

TextPaginationResult PaginateText(
	AssetManager& asset_manager, const StyledText& styled_text, const TextBox& box,
	const TextPageOptions& options
);

} // namespace impl

} // namespace ptgn