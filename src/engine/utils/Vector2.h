#pragma once

#include <cmath>
#include <type_traits>
#include <nlohmann/json.hpp>

#include <engine/utils/Math.h>
#include <engine/utils/Utility.h>

#include <Vec2D.h>

static constexpr char VECTOR2_LEFT_DELIMETER = '(';
static constexpr char VECTOR2_CENTER_DELIMETER = ',';
static constexpr char VECTOR2_RIGHT_DELIMETER = ')';

namespace internal {

// Credit to Johannes Schaub - litb at https://stackoverflow.com/a/2450157 for the promote functionality
// typedef either to A or B, depending on what integer is passed.
template<int, typename A, typename B>
struct cond;

#define CCASE(N, typed) \
  template<typename A, typename B> \
  struct cond<N, A, B> { \
    typedef typed type; \
  }

CCASE(1, A); CCASE(2, B);
CCASE(3, int); CCASE(4, unsigned int);
CCASE(5, long); CCASE(6, unsigned long);
CCASE(7, float); CCASE(8, double);
CCASE(9, long double);

#undef CCASE

// for a better syntax...
template<typename T> struct identity { typedef T type; };

// different type => figure out common type
template<typename A, typename B>
struct promote {
private:
	static A a;
	static B b;

	// in case A or B is a promoted arithmetic type, the template
	// will make it less preferred than the nontemplates below
	template<typename T>
	static identity<char[1]>::type& check(A, T);
	template<typename T>
	static identity<char[2]>::type& check(B, T);

	// "promoted arithmetic types"
	static identity<char[3]>::type& check(int, int);
	static identity<char[4]>::type& check(unsigned int, int);
	static identity<char[5]>::type& check(long, int);
	static identity<char[6]>::type& check(unsigned long, int);
	static identity<char[7]>::type& check(float, int);
	static identity<char[8]>::type& check(double, int);
	static identity<char[9]>::type& check(long double, int);

public:
	typedef typename cond<sizeof check(0 ? a : b, 0), A, B>::type
		type;
};

// same type => finished
template<typename A>
struct promote<A, A> {
	typedef A type;
};

template <typename ...Ts>
using is_number = std::enable_if_t<(std::is_arithmetic_v<Ts> && ...), int>;

} // namespace internal

template <typename T, internal::is_number<T> = 0>
struct Vector2 {
	T x = 0, y = 0;

	Vector2(Vec2D vector) : x{ static_cast<T>(std::round(vector.x)) }, y{ static_cast<T>(std::round(vector.y)) } {}


	// Zero construction
	Vector2() = default;
	// Regular construction
	Vector2(T x, T y) : x{ x }, y{ y } {}
	// Other number construction
	template <typename S, internal::is_number<T> = 0, std::enable_if_t<!std::is_same_v<S, T>, int> = 0>
	Vector2(S x, S y) : x{ static_cast<T>(std::round(x)) }, y{ static_cast<T>(std::round(y)) } {}
	// Copy / move construction
	Vector2(const Vector2& vector) = default;
	Vector2(Vector2&& vector) = default;
	Vector2& operator=(const Vector2& vector) = default;
	Vector2& operator=(Vector2&& vector) = default;
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
		return { internal::GetRandomValue<T>(min_x, max_x), internal::GetRandomValue<T>(min_y, max_y) };
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

	// Unary arithmetic operators

	Vector2& operator+=(const Vector2& rhs) { x += rhs.x; y += rhs.y; return *this; }
	Vector2& operator-=(const Vector2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
	Vector2& operator*=(const Vector2& rhs) { x *= rhs.x; y *= rhs.y; return *this;	}
	Vector2& operator/=(const Vector2& rhs) { x /= rhs.x; y /= rhs.y; return *this; }
	template <typename S, internal::is_number<S> = 0>
	Vector2& operator+=(S number) { x += number; y += number; return *this; }
	template <typename S, internal::is_number<S> = 0>
	Vector2& operator-=(S number) { x -= number; y -= number; return *this; }
	template <typename S, internal::is_number<S> = 0>
	Vector2& operator*=(S number) { x *= number; y *= number; return *this; }
	template <typename S, internal::is_number<S> = 0>
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
};

// Common type aliases
using V2_int = Vector2<int>;
using V2_double = Vector2<double>;
using V2_float = Vector2<float>;

// Comparison operators

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
template <typename A, typename B, internal::is_number<B> = 0>
inline bool operator==(const Vector2<A>& a, B b) { return a.x == b && a.y == b; }
template <typename A, typename B, internal::is_number<B> = 0>
inline bool operator!=(const Vector2<A>& a, B b) { return !operator==(a, b); }
template <typename A, typename B, internal::is_number<B> = 0>
inline bool operator<(const Vector2<A>& a, B b) { return a.x < b && a.y < b; }
template <typename A, typename B, internal::is_number<B> = 0>
inline bool operator>(const Vector2<A>& a, B b) { return operator<(b, a); }
template <typename A, typename B, internal::is_number<B> = 0>
inline bool operator<=(const Vector2<A>& a, B b) { return !operator>(a, b); }
template <typename A, typename B, internal::is_number<B> = 0>
inline bool operator>=(const Vector2<A>& a, B b) { return !operator<(a, b); }
template <typename A, typename B, internal::is_number<A> = 0>
inline bool operator==(A a, const Vector2<B>& b) { return operator==(b, a); }
template <typename A, typename B, internal::is_number<A> = 0>
inline bool operator!=(A a, const Vector2<B>& b) { return operator!=(b, a); }
template <typename A, typename B, internal::is_number<A> = 0>
inline bool operator<(A a, const Vector2<B>& b) { return !operator>=(b, a); }
template <typename A, typename B, internal::is_number<A> = 0>
inline bool operator>(A a, const Vector2<B>& b) { return operator<(b, a); }
template <typename A, typename B, internal::is_number<A> = 0>
inline bool operator<=(A a, const Vector2<B>& b) { return !operator>(b, a); }
template <typename A, typename B, internal::is_number<A> = 0>
inline bool operator>=(A a, const Vector2<B>& b) { return !operator<(b, a); }

// Binary arithmetic operators

template <typename A, typename B, typename P = typename internal::promote<A, B>::type>
inline Vector2<P> operator+(const Vector2<A>& a, const Vector2<B>& b) { return { static_cast<P>(a.x) + static_cast<P>(b.x), static_cast<P>(a.y) + static_cast<P>(b.y) }; }
template <typename A, typename B, typename P = typename internal::promote<A, B>::type>
inline Vector2<P> operator-(const Vector2<A>& a, const Vector2<B>& b) { return { static_cast<P>(a.x) - static_cast<P>(b.x), static_cast<P>(a.y) - static_cast<P>(b.y) }; }
template <typename A, typename B, typename P = typename internal::promote<A, B>::type>
inline Vector2<P> operator/(const Vector2<A>& a, const Vector2<B>& b) {
	auto vector = Vector2<P>::Infinite();
	// Prevents division by zero.
	if (b.x) vector.x = a.x / b.x;
	if (b.y) vector.y = a.y / b.y;
	return vector; 
}
// * returns a new vector composed of the products of the individual components of the two vectors
// The * operator DOES NOT calculate the dot product of two vectors
template <typename A, typename B, typename P = typename internal::promote<A, B>::type>
inline Vector2<P> operator*(const Vector2<A>& a, const Vector2<B>& b) { return { static_cast<P>(a.x) * static_cast<P>(b.x), static_cast<P>(a.y) * static_cast<P>(b.y) }; }

// Special functions

template <typename T>
inline Vector2<T> abs(const Vector2<T>& obj) {
	return { std::abs(obj.x), std::abs(obj.y) };
}

template <typename T>
inline T Distance(const Vector2<T>& a, const Vector2<T>& b) {
	return static_cast<T>(std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)));
}
// Return maximum component of vector
template <typename T>
inline T& Min(const Vector2<T>& vector) {
	return std::min(vector.x, vector.y);
}
// Return minimum component of vector
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