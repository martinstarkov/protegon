#include "core/graphics/gradient.h"

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/math_utils.h"
#include "core/util/regex.h"

namespace ptgn {

Gradient::Gradient(std::string_view css) {
	static const std::regex stop_regex(
		R"(rgba\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*([0-9.]+)\s*\)\s*(\d+)%?)"
	);

	for (const auto& match : RegexRange(css, stop_regex)) {
		int r{ std::stoi(match[1].str()) };
		int g{ std::stoi(match[2].str()) };
		int b{ std::stoi(match[3].str()) };
		float a_float{ std::stof(match[4].str()) };
		float percent{ std::stof(match[5].str()) };

		Color color{ static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b),
					 static_cast<uint8_t>(a_float * 255.0f) };

		AddStop(percent / 100.0f, color);
	}
}

void Gradient::AddStop(float t, Color color) {
	stops_.emplace_back(Clamp01(t), color);

	std::stable_sort(stops_.begin(), stops_.end(), [](const auto& a, const auto& b) {
		return a.t < b.t;
	});
}

Color Gradient::Sample(float t) const {
	t = Clamp01(t);

	if (stops_.empty()) {
		return color::White;
	}

	if (t <= stops_.front().t) {
		return stops_.front().color;
	}

	if (t >= stops_.back().t) {
		return stops_.back().color;
	}

	for (auto&& [a, b] : stops_ | std::views::adjacent<2>) {
		if (t >= a.t && t <= b.t) {
			float local = (t - a.t) / (b.t - a.t);
			return Lerp(a.color, b.color, local);
		}
	}

	return stops_.back().color;
}

} // namespace ptgn