#pragma once

#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/util/strong_string.h"
#include "serialization/serialize.h"

namespace ptgn {

struct Tag : public StrongString<Tag> {
	using StrongString::StrongString;

	constexpr Tag() : StrongString{ "Entity" } {}

	PTGN_REFLECT_VALUE(Tag, value)
};

// Filtering labels are independent of the entity display name (Tag).
struct Tags {
	std::vector<std::string> values{};

	[[nodiscard]] bool Contains(std::string_view value) const {
		return std::ranges::any_of(values, [value](const std::string& tag) {
			return tag == value;
		});
	}

	bool Add(std::string value) {
		if (value.empty() || value == "Untagged" || Contains(value)) {
			return false;
		}
		values.push_back(std::move(value));
		return true;
	}

	bool Remove(std::string_view value) {
		auto it{ std::ranges::find(values, value) };
		if (it == values.end()) {
			return false;
		}
		values.erase(it);
		return true;
	}

	PTGN_REFLECT_VALUE(Tags, values)
};

} // namespace ptgn