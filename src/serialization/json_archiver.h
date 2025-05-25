#pragma once

#include "serialization/json.h"

namespace ptgn {

class JSONArchiver {
public:
	template <typename T>
	void FromVector(const std::vector<T>& value) {
		j = value;
	}

	template <typename T>
	std::vector<T> ToVector() const {
		std::vector<T> vector;
		j.get_to(vector);
		return vector;
	}

	json j;
};

} // namespace ptgn