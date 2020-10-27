#pragma once

#include <iostream> // std::cout for logging
#include <iomanip> // std::setprecision for truncating
#include <sstream> // std::stringstream for truncating

#define LOG(x) { std::cout << x << std::endl; }
#define LOG_(x) { std::cout << x; }

namespace engine {

// Truncate to specific amount of significant figures
inline double truncate(double value, int digits) {
	std::stringstream stream;
	stream << std::fixed << std::setprecision(digits) << value;
	return std::stod(stream.str());
}

template <typename T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
	static_assert(std::is_arithmetic<T>::value, "clamp can only accept numeric types");
	assert(!(hi < lo));
	return (v < lo) ? lo : (hi < v) ? hi : v;
}

} // namespace internal