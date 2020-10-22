#pragma once

#include <cmath>
#include <type_traits>
#include <nlohmann/json.hpp>

#include <engine/utils/Math.h>
#include <engine/utils/Utility.h>

static constexpr char VECTOR2_LEFT_DELIMETER = '(';
static constexpr char VECTOR2_CENTER_DELIMETER = ',';
static constexpr char VECTOR2_RIGHT_DELIMETER = ')';

template <typename T, engine::math::is_number<T> = 0>
struct Vector2 {
	T x = 0, y = 0;
	// Zero construction
	Vector2() = default;
	// Regular construction
	Vector2(T x, T y) : x{ x }, y{ y } {}
	// Copy / move construction
	Vector2(const Vector2& vector) = default;
	Vector2(Vector2&& vector) = default;
	Vector2& operator=(const Vector2& vector) = default;
	Vector2& operator=(Vector2&& vector) = default;
	~Vector2() = default;
	// Implicit conversion to integer vector
	operator Vector2<int>() {
		return { engine::math::RoundCast<int>(x), engine::math::RoundCast<int>(y) };
	}
	// Implicit conversion to double vector
	operator Vector2<double>() {
		return { static_cast<double>(x), static_cast<double>(y) };
	}
	// Implicit conversion to float vector
	operator Vector2<float>() {
		return { static_cast<float>(x), static_cast<float>(y) };
	}
	// String construction
	Vector2(const std::string& s) {
		std::size_t delimeter = s.find(VECTOR2_CENTER_DELIMETER); // return index of centerDelimeter
		assert(s[0] == VECTOR2_LEFT_DELIMETER && "Vector2 string constructor must start with LEFT_DELIMETER");
		assert(delimeter != std::string::npos && "Vector2 string constructor must contain CENTER_DELIMETER");
		assert(s[s.length() - 1] == VECTOR2_RIGHT_DELIMETER && "Vector2 string constructor must end with RIGHT_DELIMETER");
		x = std::stod(s.substr(1, delimeter - 1)); // from after leftDelimeter to before centerDelimeter
		y = std::stod(s.substr(delimeter + 1, s.size() - 2)); // from after centerDelimeter to after rightDelimeter
	}
	// Return true if either vector component is not equal to 0
	inline operator bool() const {
		return x || y;
	}
	// Return true if both vector components equal 0
	inline bool IsZero() const {
		return !x && !y;
	}
	// Return true if either vector component equals 0
	inline bool HasZero() const {
		return !x || !y;
	}
	// Return true if both vector components equal numeric limits infinity
	inline bool IsInfinite() const {
		return std::isinf(x) && std::isinf(y);
	}
	// Return true if either vector components equals numeric limits infinity
	inline bool HasInfinity() const {
		return std::isinf(x) || std::isinf(y);
	}
	// Return a vector with numeric_limit::infinity() set for both components
	static Vector2 Infinite() {
		return Vector2{ std::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity() };
	}
	static Vector2 Random(T min_x, T max_x, T min_y, T max_y) {
		return { engine::math::GetRandomValue<T>(min_x, max_x), engine::math::GetRandomValue<T>(min_y, max_y) };
	}
	inline Vector2 Absolute() {
		return abs(*this);
	}
	// 2D vector projection (dot product)
	inline double DotProduct(const Vector2& other) const {
		return x * other.x + y * other.y;
	}
	// Area of cross product between x and y components
	inline double CrossProduct(const Vector2& other) const {
		return x * other.y - y * other.x;
	}
	// Unit vector
	inline Vector2<double> Unit() const {
		Vector2<double> vector;
		auto m = Magnitude();
		if (m) { // avoid division by zero error for zero vectors
			return vector / m;
		}
		return vector;
	}
	// Identity vector
	inline Vector2<int> Identity() const {
		return { engine::math::sgn(x), engine::math::sgn(y) };
	}
	// Tangent to direction vector (y, -x)
	inline Vector2 Tangent() const {
		return { y, -x };
	}
	// Flipped signs for both vector components
	inline Vector2 Opposite() const {
		return -(*this);
	}
	// Return the magnitude squared of a vector
	inline double MagnitudeSquared() const {
		return x * x + y * y;
	}
	// Return the magnitude of a vector
	inline double Magnitude() const {
		return std::sqrt(MagnitudeSquared());
	}

	// Operators.

	inline Vector2& operator-() { x *= -1; y *= -1; return *this; }
	inline Vector2 operator-() const { return { -x, -y }; }

	// Increment/Decrement operators.

	Vector2& operator++() { ++x; ++y; return *this;	}
	Vector2& operator--() { --x; --y; return *this; }
	Vector2 operator++(int) { Vector2 tmp{ *this }; operator++(); return tmp; }
	Vector2 operator--(int) { Vector2 tmp{ *this }; operator--(); return tmp; }

	template <typename S>
	Vector2& operator+=(const Vector2<S>& rhs) {
		x += rhs.x;
		y += rhs.y;
		return *this;
	}
	template <typename S>
	Vector2& operator-=(const Vector2<S>& rhs) {
		x -= rhs.x;
		y -= rhs.y;
		return *this;
	}
	template <typename S>
	Vector2& operator*=(const Vector2<S>& rhs) {
		x *= rhs.x;
		y *= rhs.y;
		return *this;
	}
	template <typename S>
	Vector2& operator/=(const Vector2<S>& rhs) {
		x /= rhs.x;
		y /= rhs.y;
		return *this;
	}
	template <typename S, engine::math::is_number<S> = 0>
	Vector2& operator+=(S number) {
		x += number;
		y += number;
		return *this;
	}
	template <typename S, engine::math::is_number<S> = 0>
	Vector2& operator-=(S number) {
		x -= number;
		y -= number;
		return *this;
	}
	template <typename S, engine::math::is_number<S> = 0>
	Vector2& operator*=(S number) {
		x *= number;
		y *= number;
		return *this;
	}
	template <typename S, engine::math::is_number<S> = 0>
	Vector2& operator/=(S number) {
		// Prevents division by zero.
		if (number) {
			x /= number;
			y /= number;
		} else {
			*this = Infinite();
		}
		return *this;
	}

	// Binary arithmetic operations (one type Vector + other type Vector).

	// BIG TODO: Figure out casted subtraction and addition

	/*template <typename A, typename B, typename P = engine::math::promote<A, B>>
	friend inline Vector2<P> operator-(const Vector2<A>& a, const Vector2<B>& b) {
		return Vector2<P>{ a.x - b.x, a.x - b.x };
	}*/

	// Binary arithmetic operations (same type Vector + same type Vector).

	friend inline Vector2 operator+(Vector2 a, const Vector2& b) {
		a += b;
		return a;
	}
	friend inline Vector2 operator-(Vector2 a, const Vector2& b) {
		a -= b;
		return a;
	}
	friend inline Vector2 operator*(Vector2 a, const Vector2& b) {
		a *= b;
		return a;
	}
	friend inline Vector2 operator/(Vector2 a, const Vector2& b) {
		a /= b;
		return a;
	}
};

// Common type aliases
using V2_int = Vector2<int>;
using V2_double = Vector2<double>;
using V2_float = Vector2<float>;

// source: https://stackoverflow.com/a/12919377
template< typename T >
struct check {
	static const bool value = false;
};

template <typename T>
struct check <Vector2<T>> {
	static const bool value = true;
};

template <typename T>
using is_vector = std::enable_if_t<check<T>::value, int>;

template <typename T>
using is_not_vector = std::enable_if_t<!check<T>::value, int>;

// Comparison operators (Vector vs Vector).

template <typename A, typename B>
inline bool operator==(const Vector2<A>& a, const Vector2<B>& b) { return a.x == b.x && a.y == b.y; }
template <typename A, typename B>
inline bool operator!=(const Vector2<A>& a, const Vector2<B>& b) { return !operator==(a, b); }
template <typename A, typename B>
inline bool operator<(const Vector2<A>& a, const Vector2<B>& b) { return a.MagnitudeSquared() < b.MagnitudeSquared(); }
template <typename A, typename B>
inline bool operator>(const Vector2<A>& a, const Vector2<B>& b) { return operator<(b, a); }
template <typename A, typename B>
inline bool operator<=(const Vector2<A>& a, const Vector2<B>& b) { return !operator>(a, b); }
template <typename A, typename B>
inline bool operator>=(const Vector2<A>& a, const Vector2<B>& b) { return !operator<(a, b); }

// Comparison operators (Vector vs Other).

template <typename A, typename B, engine::math::is_number<B> = 0>
inline bool operator==(const Vector2<A>& a, B b) { return a.x == b && a.y == b; }
template <typename A, typename B, engine::math::is_number<B> = 0>
inline bool operator!=(const Vector2<A>& a, B b) { return !operator==(a, b); }
template <typename A, typename B, engine::math::is_number<B> = 0>
inline bool operator<(const Vector2<A>& a, B b) { return a.x < b && a.y < b; }
template <typename A, typename B, engine::math::is_number<B> = 0>
inline bool operator>(const Vector2<A>& a, B b) { return operator<(b, a); }
template <typename A, typename B, engine::math::is_number<B> = 0>
inline bool operator<=(const Vector2<A>& a, B b) { return !operator>(a, b); }
template <typename A, typename B, engine::math::is_number<B> = 0>
inline bool operator>=(const Vector2<A>& a, B b) { return !operator<(a, b); }
template <typename A, typename B, engine::math::is_number<A> = 0>
inline bool operator==(A a, const Vector2<B>& b) { return operator==(b, a); }
template <typename A, typename B, engine::math::is_number<A> = 0>
inline bool operator!=(A a, const Vector2<B>& b) { return operator!=(b, a); }
template <typename A, typename B, engine::math::is_number<A> = 0>
inline bool operator<(A a, const Vector2<B>& b) { return !operator>=(b, a); }
template <typename A, typename B, engine::math::is_number<A> = 0>
inline bool operator>(A a, const Vector2<B>& b) { return operator<(b, a); }
template <typename A, typename B, engine::math::is_number<A> = 0>
inline bool operator<=(A a, const Vector2<B>& b) { return !operator>(b, a); }
template <typename A, typename B, engine::math::is_number<A> = 0>
inline bool operator>=(A a, const Vector2<B>& b) { return !operator<(b, a); }

// Binary arithmetic operations (Vector + Other).

inline V2_double operator/(const V2_double& a, double b) {
	return { a.x / b, a.y / b };
}
inline V2_double operator/(double b, V2_double a) {
	return { b / a.x, b / a.y };
}
inline V2_double operator/(const V2_int& a, double b) {
	return { static_cast<double>(a.x) / b, static_cast<double>(a.y) / b };
}
inline V2_double operator/(double b, V2_int a) {
	return { b / static_cast<double>(a.x), b / static_cast<double>(a.y) };
}
inline V2_int operator/(const V2_int& a, int b) {
	return { a.x / b, a.y / b };
}
inline V2_int operator/(int b, V2_int a) {
	return { b / a.x, b / a.y };
}
template <typename T>
inline Vector2<T> operator*(Vector2<T> a, double b) {
	a *= b;
	return a;
}
template <typename T>
inline Vector2<T> operator*(double b, Vector2<T> a) {
	a *= b;
	return a;
}
template <typename T>
inline Vector2<T> operator*(Vector2<T> a, float b) {
	a *= b;
	return a;
}
template <typename T>
inline Vector2<T> operator*(float b, Vector2<T> a) {
	a *= b;
	return a;
}
template <typename T>
inline Vector2<T> operator*(Vector2<T> a, int b) {
	a *= b;
	return a;
}
template <typename T>
inline Vector2<T> operator*(int b, Vector2<T> a) {
	a *= b;
	return a;
}

// Special functions

template <typename T>
inline Vector2<T> abs(const Vector2<T>& obj) {
	return { std::abs(obj.x), std::abs(obj.y) };
}
// Return the distance between two vectors.
template <typename T, typename S>
inline double Distance(const Vector2<T>& a, const Vector2<S>& b) {
	return static_cast<double>(std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)));
}
// Return maximum component of vector.
template <typename T>
inline T& Min(const Vector2<T>& vector) {
	return std::min(vector.x, vector.y);
}
// Return minimum component of vector.
template <typename T>
inline T& Max(const Vector2<T>& vector) {
	return std::min(vector.x, vector.y);
}

// Both vector components rounded to the closest integeral
template <typename T>
inline Vector2<T> Round(const Vector2<T>& obj) {
	return { static_cast<T>(std::round(obj.x)), static_cast<T>(std::round(obj.y)) };
}

// Stream operators

template <typename T>
std::ostream& operator<<(std::ostream& os, const Vector2<T>& obj) {
	os << VECTOR2_LEFT_DELIMETER << obj.x << VECTOR2_CENTER_DELIMETER << obj.y << VECTOR2_RIGHT_DELIMETER;
	return os;
}

template <typename T>
std::istream& operator>>(std::istream& is, Vector2<T>& obj) {
	std::string temp;
	is >> temp;
	obj = std::move(Vector2<T>{ temp });
	return is;
}

// Json serialization

template <typename T, nlohmann::detail::enable_if_t<nlohmann::detail::has_to_json<nlohmann::json::basic_json_t, T>::value, int > = 0>
inline void to_json(nlohmann::json& j, const Vector2<T>& o) {
	j["x"] = o.x;
	j["y"] = o.y;
}

template <typename T, nlohmann::detail::enable_if_t<nlohmann::detail::has_from_json<nlohmann::json::basic_json_t, T>::value, int > = 0>
inline void from_json(const nlohmann::json& j, Vector2<T>& o) {
	if (j.find("x") != j.end()) {
		o.x = j.at("x").get<T>();
	}
	if (j.find("y") != j.end()) {
		o.y = j.at("y").get<T>();
	}
}