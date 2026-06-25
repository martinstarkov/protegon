#include "runtime/graphics/text/text_pagination.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/graphics/text/text.h"

namespace ptgn::impl {

namespace {

struct PaginationFragment {
	TextRun run;
	std::optional<std::size_t> source_run_index;
};

struct PaginationWord {
	std::vector<PaginationFragment> leading_whitespace;
	std::vector<PaginationFragment> content;
};

struct PaginationContent {
	std::vector<PaginationWord> words;
	std::vector<PaginationFragment> trailing_whitespace;
};

bool IsPaginationWhitespace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void AppendSourceFragment(
	std::vector<PaginationFragment>& fragments, TextRun&& run, std::size_t run_index
) {
	if (run.text.empty()) {
		return;
	}

	fragments.emplace_back(
		PaginationFragment{
			.run			  = std::move(run),
			.source_run_index = run_index,
		}
	);
}

void AppendMarker(
	std::vector<PaginationFragment>& fragments, std::string_view marker,
	const PaginationFragment& anchor
) {
	if (marker.empty()) {
		return;
	}

	fragments.emplace_back(PaginationFragment{
		.run = {
			.text = std::string{ marker },
			.font = anchor.run.font,
			.style = anchor.run.style,
		},
		.source_run_index = std::nullopt,
	});
}

void AppendWord(
	std::vector<PaginationFragment>& fragments, const PaginationWord& word,
	bool include_leading_whitespace
) {
	if (include_leading_whitespace) {
		fragments.append_range(word.leading_whitespace);
	}
	fragments.append_range(word.content);
}

PaginationContent BuildPaginationContent(const StyledText& styled_text) {
	PaginationContent result;
	std::vector<PaginationFragment> pending_whitespace;
	std::optional<PaginationWord> current_word;

	for (auto run_index{ 0uz }; run_index < styled_text.runs.size(); ++run_index) {
		const auto& run{ styled_text.runs[run_index] };
		auto begin{ 0uz };

		while (begin < run.text.size()) {
			bool whitespace{ IsPaginationWhitespace(run.text[begin]) };
			auto end{ begin + 1 };

			while (end < run.text.size() && IsPaginationWhitespace(run.text[end]) == whitespace) {
				++end;
			}

			std::string_view fragment{ run.text.data() + begin, end - begin };

			if (whitespace) {
				if (current_word.has_value()) {
					result.words.push_back(std::move(current_word.value()));
					current_word.reset();
				}

				AppendSourceFragment(
					pending_whitespace,
					TextRun{
						.text  = std::string{ fragment },
						.font  = run.font,
						.style = run.style,
					},
					run_index
				);
			} else {
				if (!current_word.has_value()) {
					current_word.emplace();
					current_word->leading_whitespace = std::move(pending_whitespace);
					pending_whitespace.clear();
				}

				AppendSourceFragment(
					current_word->content,
					TextRun{
						.text  = std::string{ fragment },
						.font  = run.font,
						.style = run.style,
					},
					run_index
				);
			}

			begin = end;
		}
	}

	if (current_word.has_value()) {
		result.words.push_back(std::move(current_word.value()));
	}

	result.trailing_whitespace = std::move(pending_whitespace);
	return result;
}

StyledText BuildStyledText(const std::vector<PaginationFragment>& fragments) {
	StyledText result;
	std::optional<std::size_t> previous_source_run;

	for (const auto& fragment : fragments) {
		if (fragment.run.text.empty()) {
			continue;
		}

		bool can_merge{ fragment.source_run_index.has_value() &&
						previous_source_run == fragment.source_run_index && !result.runs.empty() };

		if (can_merge) {
			result.runs.back().text += fragment.run.text;
		} else {
			result.runs.emplace_back(fragment.run);
		}

		previous_source_run = fragment.source_run_index;
	}

	return result;
}

bool FitsTextPage(const ResolvedStyledText& styled_text, TextBox box, std::size_t max_lines) {
	box.style.max_lines		= 0;
	box.style.overflow_mode = OverflowMode::Overflow;

	auto measurement{ MeasureText(styled_text, box) };
	if (max_lines > 0 && measurement.line_count > max_lines) {
		return false;
	}

	auto box_size{ box.rect.GetSize() };
	bool fits_width{ !box.HasWidth() || measurement.size.x <= box_size.x };
	bool fits_height{ !box.HasHeight() || measurement.size.y <= box_size.y };
	return fits_width && fits_height;
}

} // namespace

TextPaginationResult PaginateText(
	AssetManager& asset_manager, const StyledText& styled_text, const TextBox& box,
	const TextPageOptions& options
) {
	TextPaginationResult result;

	auto add_page = [&](StyledText&& page_text) {
		auto resolved_page_text{ ResolveStyledText(asset_manager, page_text) };
		auto layout{ BuildTextLayout(resolved_page_text, box) };

		result.pages.emplace_back(TextPage{
			.styled_text = std::move(page_text),
			.measurement = {
				.size = layout.size,
				.line_count = layout.lines.size(),
				.truncated = layout.truncated,
				.used_shrink_scale = layout.used_shrink_scale,
			},
			.glyph_count = layout.GetVisibleGlyphCount(),
		});
	};

	auto content{ BuildPaginationContent(styled_text) };

	if (content.words.empty()) {
		auto page_text{ styled_text };
		add_page(std::move(page_text));
		return result;
	}

	auto max_lines{ options.max_lines_per_page };
	if (max_lines == 0) {
		max_lines = box.style.max_lines;
	}

	std::vector<PaginationFragment> current_page;
	std::size_t current_page_word_count{ 0 };
	auto word_index{ 0uz };

	while (word_index < content.words.size()) {
		const auto& word{ content.words[word_index] };
		std::vector<PaginationFragment> candidate{ current_page };

		// The first page preserves source-leading whitespace. A continuation page discards only
		// its ordinary separator whitespace unless a split-begin marker already anchors the page.
		bool include_leading_whitespace{ !candidate.empty() || result.pages.empty() };
		AppendWord(candidate, word, include_leading_whitespace);

		std::vector<PaginationFragment> measured_candidate{ candidate };
		bool has_more_words{ word_index + 1 < content.words.size() };

		if (options.add_split_markers && has_more_words && !measured_candidate.empty()) {
			AppendMarker(measured_candidate, options.split_end, measured_candidate.back());
		}

		auto candidate_text{ BuildStyledText(measured_candidate) };
		if (FitsTextPage(ResolveStyledText(asset_manager, candidate_text), box, max_lines)) {
			current_page = std::move(candidate);
			++current_page_word_count;
			++word_index;
			continue;
		}

		// Keep a single oversized word intact. The configured TextBox wrapping and overflow mode
		// remains responsible for deciding how that word is displayed.
		if (current_page_word_count == 0) {
			current_page = std::move(candidate);
			++current_page_word_count;
			++word_index;
		}

		bool has_next_page{ word_index < content.words.size() };
		std::vector<PaginationFragment> completed_page{ current_page };

		if (options.add_split_markers && has_next_page && !completed_page.empty()) {
			AppendMarker(completed_page, options.split_end, completed_page.back());
		}

		add_page(BuildStyledText(completed_page));
		current_page.clear();
		current_page_word_count = 0;

		if (options.add_split_markers && has_next_page) {
			const auto& next_word{ content.words[word_index] };
			if (!next_word.content.empty()) {
				AppendMarker(current_page, options.split_begin, next_word.content.front());
			}
		}
	}

	current_page.append_range(content.trailing_whitespace);

	if (!current_page.empty()) {
		add_page(BuildStyledText(current_page));
	}

	return result;
}

} // namespace ptgn::impl
