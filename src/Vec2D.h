#pragma once

#include <cmath>

#include "common.h"

constexpr const char leftDelimeter = '(';
constexpr const char centerDelimeter = ',';
constexpr const char rightDelimeter = ')';

struct Vec2D {
	double x, y;
	// Zero construction
	Vec2D() : x(0.0), y(0.0) {}
	// Regular construction
	Vec2D(double x, double y) : x(x), y(y) {}
	Vec2D(int x, int y) : x(static_cast<double>(x)), y(static_cast<double>(y)) {}
	// Copy construction
	Vec2D(const Vec2D& copy) {
		x = copy.x;
		y = copy.y;
	}
	// Dual construction
	Vec2D(double both) : x(both), y(both) {}
	Vec2D(int both) : x(static_cast<double>(both)), y(static_cast<double>(both)) {}
	// String construction
	Vec2D(std::string s) {
		std::size_t delimeter = s.find(centerDelimeter); // return index of centerDelimeter
		assert(s[0] == leftDelimeter && "Vec2D string constructor must start with leftDelimeter");
		assert(delimeter != std::string::npos && "Vec2D string constructor must contain centerDelimeter");
		assert(s[s.length() - 1] == rightDelimeter && "Vec2D string constructor must end with rightDelimeter");
		x = std::stod(s.substr(1, delimeter - 1)); // from after leftDelimeter to before centerDelimeter
		y = std::stod(s.substr(delimeter + 1, s.size() - 2)); // from after centerDelimeter to after rightDelimeter
	}
	// Return true if either vector component is not equal to 0
	operator bool() const {
		return x && y;
	}
	// Return true if both vector components equal 0
	bool isZero() const {
		return !(*this);
	}
	bool isZero() {
		return !*const_cast<const Vec2D*>(this);
	}
	// Return true if both vector components equal numeric limits infinity
	bool isInfinite() const {
		return x == INFINITE && y == INFINITE;
	}
	bool isInfinite() {
		return const_cast<const Vec2D*>(this)->isInfinite();
	}
	// Return a vector with numeric_limit::infinity() set for both components
	Vec2D infinite() const {
		return Vec2D(INFINITE);
	}
	Vec2D infinite() {
		return const_cast<const Vec2D*>(this)->infinite();
	}
	// Stream operators
	friend std::ostream& operator<<(std::ostream& os, const Vec2D& obj) {
		os << leftDelimeter << obj.x << centerDelimeter << obj.y << rightDelimeter;
		return os;
	}
	friend std::istream& operator>>(std::istream& is, Vec2D& obj) {
		std::string temp;
		is >> temp;
		obj = Vec2D(temp);
		return is;
	}
	// Both vector components rounded to the closest integeral
	friend Vec2D round(const Vec2D& obj) {
		return Vec2D(round(obj.x), round(obj.y));
	}
	// Return a reference to the vector with lower magnitude, or first vector if equal
	friend Vec2D& min(Vec2D& v1, Vec2D& v2) {
		if (v1 <= v2) {
			return v1;
		} else {
			return v2;
		}
	}
	// Return a reference to the vector with higher magnitude, or first vector if equal
	friend Vec2D& max(Vec2D& v1, Vec2D& v2) {
		if (v1 >= v2) {
			return v1;
		} else {
			return v2;
		}
	}
	// Absolute value of both vector components
	friend Vec2D abs(const Vec2D& obj) {
		return Vec2D(std::abs(obj.x), std::abs(obj.y));
	}
	Vec2D absolute() const {
		return abs(*this);
	}
	Vec2D absolute() {
		return const_cast<const Vec2D*>(this)->absolute();
	}
	// 2D vector projection (dot product)
	double dotProduct(const Vec2D& other) const {
		return x * other.x + y * other.y;
	}
	double dotProduct(const Vec2D& other) {
		return const_cast<const Vec2D*>(this)->dotProduct(other);
	}
	// Area of cross product between x and y components
	double crossProduct(const Vec2D& other) const {
		return x * other.y - y * other.x;
	}
	double crossProduct(const Vec2D& other) {
		return const_cast<const Vec2D*>(this)->crossProduct(other);
	}
	// Unit vector
	Vec2D unit() const {
		Vec2D obj;
		if (magnitude()) { // avoid division by zero error
			return obj / magnitude();
		}
		return obj;
	}
	Vec2D unit() {
		return const_cast<const Vec2D*>(this)->unit();
	}
	Vec2D normalized() const {
		return unit();
	}
	Vec2D normalized() {
		return unit();
	}
	// Identity vector
	Vec2D identity() const {
		Vec2D obj;
		obj.x = x > 0.0 ? 1.0 : x < 0.0 ? -1.0 : 0.0;
		obj.y = y > 0.0 ? 1.0 : y < 0.0 ? -1.0 : 0.0;
		return obj;
	}
	Vec2D identity() {
		return const_cast<const Vec2D*>(this)->identity();
	}
	// Tangent to direction vector (y, -x)
	Vec2D tangent() const {
		return Vec2D(y, -x);
	}
	Vec2D tangent() {
		return const_cast<const Vec2D*>(this)->tangent();
	}
	// Flipped signs for both vector components
	Vec2D opposite() const {
		return Vec2D(-x, -y);
	}
	Vec2D opposite() {
		return const_cast<const Vec2D*>(this)->opposite();
	}
	// Return the magnitude squared of a vector
	double magnitudeSquared() const {
		return x * x + y * y;
	}
	double magnitudeSquared() {
		return const_cast<const Vec2D*>(this)->magnitudeSquared();
	}
	// Return the magnitude of a vector
	double magnitude() const {
		return sqrt(magnitudeSquared());
	}
	double magnitude() {
		return const_cast<const Vec2D*>(this)->magnitude();
	}

	Vec2D operator -() { return Vec2D(-x, -y); }
	// Increment/Decrement operators
	Vec2D& operator++() {
		++x;
		++y;
		return *this;
	}
	Vec2D operator++(int) {
		Vec2D tmp(*this);
		operator++();
		return tmp;
	}
	Vec2D& operator--() {
		--x;
		--y;
		return *this;
	}
	Vec2D operator--(int) {
		Vec2D tmp(*this);
		operator--();
		return tmp;
	}
	// Unary arithmetic operators
	Vec2D& operator+=(const Vec2D& rhs) {
		x += rhs.x;
		y += rhs.y;
		return *this;
	}
	Vec2D& operator-=(const Vec2D& rhs) {
		x -= rhs.x;
		y -= rhs.y;
		return *this;
	}
	Vec2D& operator*=(const Vec2D& rhs) {
		x *= rhs.x;
		y *= rhs.y;
		return *this;
	}
	Vec2D& operator/=(const Vec2D& rhs) {
		x /= rhs.x;
		y /= rhs.y;
		return *this;
	}
	Vec2D& operator+=(const double& rhs) {
		x += rhs;
		y += rhs;
		return *this;
	}
	Vec2D& operator-=(const double& rhs) {
		x -= rhs;
		y -= rhs;
		return *this;
	}
	Vec2D& operator*=(const double& rhs) {
		x *= rhs;
		y *= rhs;
		return *this;
	}
	Vec2D& operator/=(const double& rhs) {
		x /= rhs;
		y /= rhs;
		return *this;
	}
	// Array subscript operators
	double& operator[](std::size_t idx) {
		assert(idx <= 1 && "Vec2D [] subscript out of range");
		return idx == 0 ? x : y;
	}
	double operator[](std::size_t idx) const {
		assert(idx <= 1 && "Vec2D [] subscript out of range");
		return idx == 0 ? x : y;
	}
};

// Comparison operators

inline bool operator==(const Vec2D& lhs, const Vec2D& rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }
inline bool operator!=(const Vec2D& lhs, const Vec2D& rhs) { return !operator==(lhs, rhs); }
inline bool operator<(const Vec2D& lhs, const Vec2D& rhs) { return lhs.magnitude() < rhs.magnitude(); }
inline bool operator>(const Vec2D& lhs, const Vec2D& rhs) { return operator<(rhs, lhs); }
inline bool operator<=(const Vec2D& lhs, const Vec2D& rhs) { return !operator>(lhs, rhs); }
inline bool operator>=(const Vec2D& lhs, const Vec2D& rhs) { return !operator<(lhs, rhs); }
inline bool operator==(const Vec2D& lhs, const double& rhs) { return lhs.x == rhs && lhs.y == rhs; }
inline bool operator!=(const Vec2D& lhs, const double& rhs) { return !operator==(lhs, rhs); }
inline bool operator<(const Vec2D& lhs, const double& rhs) { return lhs.x < rhs && lhs.y < rhs; }
inline bool operator>(const Vec2D& lhs, const double& rhs) { return operator<(rhs, lhs); }
inline bool operator<=(const Vec2D& lhs, const double& rhs) { return !operator>(lhs, rhs); }
inline bool operator>=(const Vec2D& lhs, const double& rhs) { return !operator<(lhs, rhs); }
inline bool operator==(const double& lhs, const Vec2D& rhs) { return operator==(rhs, lhs); }
inline bool operator!=(const double& lhs, const Vec2D& rhs) { return operator!=(rhs, lhs); }
inline bool operator<(const double& lhs, const Vec2D& rhs) { return rhs.x > lhs && rhs.y > lhs; }
inline bool operator>(const double& lhs, const Vec2D& rhs) { return operator<(rhs, lhs); }
inline bool operator<=(const double& lhs, const Vec2D& rhs) { return !operator>(rhs, lhs); }
inline bool operator>=(const double& lhs, const Vec2D& rhs) { return !operator<(rhs, lhs); }

// Binary arithmetic operators

inline Vec2D operator+(Vec2D lhs, const Vec2D& rhs) {
	lhs += rhs;
	return lhs;
}
inline Vec2D operator+(Vec2D lhs, Vec2D& rhs) {
	lhs += rhs;
	return lhs;
}
inline Vec2D operator-(Vec2D lhs, const Vec2D& rhs) {
	lhs -= rhs;
	return lhs;
}
inline Vec2D operator-(Vec2D lhs, Vec2D& rhs) {
	lhs -= rhs;
	return lhs;
}
inline Vec2D operator*(Vec2D lhs, const Vec2D& rhs) {
	lhs *= rhs;
	return lhs;
}
inline Vec2D operator*(Vec2D lhs, Vec2D& rhs) {
	lhs *= rhs;
	return lhs;
}
inline Vec2D operator/(Vec2D lhs, const Vec2D& rhs) {
	lhs /= rhs;
	return lhs;
}
inline Vec2D operator/(Vec2D lhs, Vec2D& rhs) {
	lhs /= rhs;
	return lhs;
}
inline Vec2D operator+(Vec2D lhs, const double& rhs) {
	lhs += rhs;
	return lhs;
}
inline Vec2D operator+(Vec2D lhs, double& rhs) {
	lhs += rhs;
	return lhs;
}
inline Vec2D operator-(Vec2D lhs, const double& rhs) {
	lhs -= rhs;
	return lhs;
}
inline Vec2D operator-(Vec2D lhs, double& rhs) {
	lhs -= rhs;
	return lhs;
}
inline Vec2D operator*(Vec2D lhs, const double& rhs) {
	lhs *= rhs;
	return lhs;
}
inline Vec2D operator*(Vec2D lhs, double& rhs) {
	lhs *= rhs;
	return lhs;
}
inline Vec2D operator/(Vec2D lhs, const double& rhs) {
	lhs /= rhs;
	return lhs;
}
inline Vec2D operator/(Vec2D lhs, double& rhs) {
	lhs /= rhs;
	return lhs;
}
inline Vec2D operator+(const double& lhs, Vec2D& rhs) {
	rhs += lhs;
	return rhs;
}
inline Vec2D operator+(double& lhs, Vec2D& rhs) {
	rhs += lhs;
	return rhs;
}
inline Vec2D operator-(const double& lhs, Vec2D& rhs) {
	rhs -= lhs;
	return rhs;
}
inline Vec2D operator-(double& lhs, Vec2D& rhs) {
	rhs -= lhs;
	return rhs;
}
inline Vec2D operator*(const double& lhs, Vec2D& rhs) {
	rhs *= lhs;
	return rhs;
}
inline Vec2D operator*(double& lhs, Vec2D& rhs) {
	rhs *= lhs;
	return rhs;
}
inline Vec2D operator/(const double& lhs, Vec2D& rhs) {
	rhs /= lhs;
	return rhs;
}
inline Vec2D operator/(double& lhs, Vec2D& rhs) {
	rhs /= lhs;
	return rhs;
}

// Json serialization

inline void to_json(nlohmann::json& j, const Vec2D& o) {
	j["x"] = o.x;
	j["y"] = o.y;
}
inline void from_json(const nlohmann::json& j, Vec2D& o) {
	if (j.find("x") != j.end()) {
		o.x = j.at("x").get<double>();
	}
	if (j.find("y") != j.end()) {
		o.y = j.at("y").get<double>();
	}
}