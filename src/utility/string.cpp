#include "utility/string.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace ptgn {

std::string ToLower(const std::string& str) {
	std::string lower = str;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return lower;
}

std::string ToUpper(const std::string& str) {
	std::string upper = str;
	std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
		return static_cast<char>(std::toupper(c));
	});
	return upper;
}

} // namespace ptgn