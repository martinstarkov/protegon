#pragma once

#include <iterator>
#include <ranges>
#include <regex>

namespace ptgn {

template <std::bidirectional_iterator It>
auto RegexRange(It begin, It end, const std::regex& regex) {
	using Iter = std::regex_iterator<It>;
	return std::ranges::subrange(Iter{ begin, end, regex }, Iter{});
}

template <std::ranges::range Range>
	requires std::bidirectional_iterator<std::ranges::iterator_t<Range>>
auto RegexRange(Range& r, const std::regex& re) {
	using It   = std::ranges::iterator_t<Range>;
	using Iter = std::regex_iterator<It>;

	return std::ranges::subrange(Iter{ std::ranges::begin(r), std::ranges::end(r), re }, Iter{});
}

template <std::ranges::range Range>
	requires std::bidirectional_iterator<std::ranges::iterator_t<const Range>>
auto RegexRange(const Range& r, const std::regex& re) {
	using It   = std::ranges::iterator_t<const Range>;
	using Iter = std::regex_iterator<It>;

	return std::ranges::subrange(Iter{ std::ranges::begin(r), std::ranges::end(r), re }, Iter{});
}

} // namespace ptgn