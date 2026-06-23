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

	fragments.emplace_back(
		PaginationFragment{
			.run			  = { .text	 = std::string{ marker },
								  .font	 = anchor.run.font,
								  .style = anchor.run.style },
			.source_run_index = std::nullopt,
		}
	);
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

			std::string_view fragment{
				run.text.data() + begin,
				end - begin,
			};

			if (whitespace) {
				if (current_word.has_value()) {
					result.words.push_back(std::move(current_word.value()));
					current_word.reset();
				}

				AppendSourceFragment(
					pending_whitespace,
					{ .text = std::string{ fragment }, .font = run.font, .style = run.style },
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
					{ .text = std::string{ fragment }, .font = run.font, .style = run.style },
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

		if (bool can_merge{ fragment.source_run_index.has_value() &&
							previous_source_run == fragment.source_run_index &&
							!result.runs.empty() };
			can_merge) {
			result.runs.back().text += fragment.run.text;
		} else {
			result.runs.emplace_back(fragment.run);
		}

		previous_source_run = fragment.source_run_index;
	}

	return result;
}

bool FitsTextPage(
	const ResolvedStyledText& styled_text, const TextBox& box, std::size_t max_lines
) {
	TextBox measurement_box{ box };

	measurement_box.style.max_lines		= 0;
	measurement_box.style.overflow_mode = OverflowMode::Overflow;

	TextMeasurement measurement{ MeasureText(styled_text, measurement_box) };

	bool fits_lines{ max_lines == 0 || measurement.line_count <= max_lines };

	float box_height{ box.rect.GetSize().y };

	bool fits_height{ box_height <= 0.0f || measurement.size.y <= box_height };

	return fits_lines && fits_height;
}

} // namespace

TextPaginationResult PaginateText(
	AssetManager& asset_manager, const StyledText& styled_text, const TextBox& box,
	const TextPageOptions& options
) {
	TextPaginationResult result;

	auto add_page = [&](StyledText&& page_text) {
		auto resolved_page_text{ ResolveStyledText(asset_manager, page_text) };

		auto measurement{ MeasureText(resolved_page_text, box) };

		auto layout{ BuildTextLayout(resolved_page_text, box) };

		result.pages.emplace_back(
			TextPage{
				.styled_text = std::move(page_text),
				.measurement = measurement,
				.glyph_count = layout.glyphs.size(),
			}
		);
	};

	PaginationContent content{ BuildPaginationContent(styled_text) };

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

		// Preserve leading whitespace on the first page and after a
		// split-begin marker. Drop it on an otherwise empty continuation
		// page so the page does not begin with ordinary spaces.
		bool include_leading_whitespace{ !candidate.empty() || result.pages.empty() };

		AppendWord(candidate, word, include_leading_whitespace);

		std::vector<PaginationFragment> measured_candidate{ candidate };

		if (bool has_more_words{ word_index + 1 < content.words.size() };
			options.add_split_markers && has_more_words && !measured_candidate.empty()) {
			AppendMarker(measured_candidate, options.split_end, measured_candidate.back());
		}

		if (auto candidate_text{ BuildStyledText(measured_candidate) };
			FitsTextPage(impl::ResolveStyledText(asset_manager, candidate_text), box, max_lines)) {
			current_page = std::move(candidate);
			++current_page_word_count;
			++word_index;
			continue;
		}

		// A single word does not fit on an otherwise empty page. Keep it
		// intact and allow the configured layout behavior to handle it.
		if (!current_page_word_count) {
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
