#include "renderer/primitives/gradient.h"

#include <algorithm>
#include <cstdint>
#include <regex>
#include <string>
#include <vector>

#include "renderer/primitives/color.h"

namespace ptgn {

Gradient::Gradient(const std::string& css) { // NOSONAR
	std::regex stop_regex(R"(rgba\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*([0-9.]+)\s*\)\s*(\d+)%?)"
	);

	auto begin = std::sregex_iterator(css.begin(), css.end(), stop_regex);
	auto end   = std::sregex_iterator();

	for (auto it{ begin }; it != end; ++it) {
		const std::smatch& match = *it;

		int r		  = std::stoi(match[1]);
		int g		  = std::stoi(match[2]);
		int b		  = std::stoi(match[3]);
		float a_float = std::stof(match[4]);
		float percent = std::stof(match[5]);

		Color color{ static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b),
					 static_cast<uint8_t>(a_float * 255.0f) };

		float t = percent / 100.0f;

		AddStop(t, color);
	}
}

void Gradient::AddStop(float t, Color color) {
	stops_.emplace_back(std::clamp(t, 0.0f, 1.0f), color);

	std::stable_sort(stops_.begin(), stops_.end(), [](const auto& a, const auto& b) {
		return a.t < b.t;
	});
}

Color Gradient::Sample(float t) const {
	t = std::clamp(t, 0.0f, 1.0f);

	if (stops_.empty()) {
		return color::White;
	}

	if (t <= stops_.front().t) {
		return stops_.front().color;
	}

	if (t >= stops_.back().t) {
		return stops_.back().color;
	}

	for (std::size_t i = 0; i < stops_.size() - 1; ++i) {
		const auto& a = stops_[i];
		const auto& b = stops_[i + 1];

		if (t >= a.t && t <= b.t) {
			float local = (t - a.t) / (b.t - a.t);
			return Lerp(a.color, b.color, local);
		}
	}

	return stops_.back().color;
}

} // namespace ptgn