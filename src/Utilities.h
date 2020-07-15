#pragma once
#include <iomanip> // setprecision
#include <sstream> // string streams for limiting precision / significant figures

// Truncate to specific amount of significant figures
static double truncate(double value, int digits) {
	std::stringstream stream;
	stream << std::fixed << std::setprecision(digits) << value;
	return std::stod(stream.str());
}

// Find the sign of a numeric value
template <typename T>
static int sgn(T val) {
	return (T(0) < val) - (val < T(0));
}

// Wapper function which allows calling a function on a parameter pack (variadic templates)
template <typename ...Ts> 
void swallow(Ts&&... args) {} 
