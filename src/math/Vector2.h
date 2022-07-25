#pragma once

#include <algorithm> // std::swap
#include <iostream> // std::ostream, std::istream
#include <string> // std::string
#include <limits> // std::numeric_limits
#include <cmath> // std::round
#include <cstdint> // std::int32_t, etc
#include <cstdlib> // std::size_t
#include <type_traits>
#include <cassert> // assert

#include "math/Math.h"
#include "math/RNG.h"

namespace ptgn {

namespace math {

// Vector stream output / input delimeters, allow for consistent serialization / deserialization.

inline constexpr const char VECTOR_LEFT_DELIMETER{ '(' };
inline constexpr const char VECTOR_CENTER_DELIMETER{ ',' };
inline constexpr const char VECTOR_RIGHT_DELIMETER{ ')' };
template <typename T,
    std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
inline constexpr const T VECTOR_EPSILON{ math::EPSILON<T> };

/*
* @tparam T Type contained in vector.
*/
template <typename T,
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
struct Vector2 {
    // Return a vector with numeric_limit::infinity() set for both components.
    static Vector2 Infinite() {
        static_assert(std::is_floating_point_v<T>,
                      "Cannot create infinite vector for integer type. Must use floating points.");
        return { std::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity() };
    }

    // Return a vector with std::numeric_limits<T>::max() set for both components.
    static Vector2 Maximum() {
        return { std::numeric_limits<T>::max(), std::numeric_limits<T>::max() };
    }

    // Return a vector with std::numeric_limits<T>::min() set for both components.
    static Vector2 Minimum() {
        return { std::numeric_limits<T>::min(), std::numeric_limits<T>::min() };
    }

    // Return a vector with 0 for both components.
    static Vector2 Zero() {
        return { (T)0, (T)0 };
    }

    // Return a vector with both components randomized in the given ranges.
    static Vector2 Random(T min_x = static_cast<T>(0), T max_x = static_cast<T>(1), 
                          T min_y = static_cast<T>(0), T max_y = static_cast<T>(1)) {
        assert(min_x < max_x &&
               "Minimum random value must be less than maximum random value");
        assert(min_y < max_y &&
               "Minimum random value must be less than maximum random value");
        math::RNG<T> rng_x{ min_x, max_x };
        math::RNG<T> rng_y{ min_y, max_y };
        // Vary distribution type based on template parameter type.
        return { rng_x(), rng_y() };
    }

    T x{ 0 };
    T y{ 0 };

    // Zero construction by default.
    constexpr Vector2() = default;

    ~Vector2() = default;
    
    // Allow construction from two different types, cast to the vector type.
    template <typename U, typename V, 
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true,
        std::enable_if_t<std::is_arithmetic_v<V>, bool> = true>
    constexpr Vector2(U x, V y) : x{ static_cast<T>(x) }, y{ static_cast<T>(y) } {}

    // Copy / assignment construction.

    Vector2(const Vector2& copy) = default;
    Vector2(Vector2&& move) = default;
    Vector2& operator=(const Vector2& copy) = default;
    Vector2& operator=(Vector2&& move) = default;

    // Unary increment / decrement / minus operators.

    Vector2& operator++() {
        ++x; 
        ++y;
        return *this;
    }

    Vector2 operator++(int) {
        Vector2 tmp{ *this };
        operator++();
        return tmp;
    }

    Vector2& operator--() {
        --x;
        --y;
        return *this;
    }

    Vector2 operator--(int) {
        Vector2 tmp{ *this };
        operator--();
        return tmp;
    }

    Vector2 operator-() const {
        return { -x, -y };
    }

    // Arithmetic operators between vectors.

    template <typename U,
        std::enable_if_t<std::is_convertible_v<U, T>, bool> = true>
    Vector2& operator+=(const Vector2<U>& rhs) {
        x += static_cast<T>(rhs.x);
        y += static_cast<T>(rhs.y);
        return *this;
    }

    template <typename U, 
        std::enable_if_t<std::is_convertible_v<U, T>, bool> = true>
    Vector2& operator-=(const Vector2<U>& rhs) {
        x -= static_cast<T>(rhs.x);
        y -= static_cast<T>(rhs.y);
        return *this;
    }

    template <typename U, 
        std::enable_if_t<std::is_convertible_v<U, T>, bool> = true>
    Vector2& operator*=(const Vector2<U>& rhs) {
        x *= static_cast<T>(rhs.x);
        y *= static_cast<T>(rhs.y);
        return *this;
    }

    template <typename U, 
        std::enable_if_t<std::is_convertible_v<U, T>, bool> = true>
    Vector2& operator/=(const Vector2<U>& rhs) {
        if (rhs.x != static_cast<U>(0))
            x /= static_cast<T>(rhs.x);
        else
            x = std::numeric_limits<T>::infinity();
        if (rhs.y != static_cast<U>(0))
            y /= static_cast<T>(rhs.y);
        else
            y = std::numeric_limits<T>::infinity();
        return *this;
    }

    // Arithmetic operators with basic types.

    template <typename U,
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true,
        std::enable_if_t<std::is_convertible_v<U, T>, bool> = true>
    Vector2& operator+=(U rhs) {
        x += static_cast<T>(rhs);
        y += static_cast<T>(rhs);
        return *this;
    }

    template <typename U, 
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true,
        std::enable_if_t<std::is_convertible_v<U, T>, bool> = true>
    Vector2& operator-=(U rhs) {
        x -= static_cast<T>(rhs);
        y -= static_cast<T>(rhs);
        return *this;
    }

    template <typename U,
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true,
        std::enable_if_t<std::is_convertible_v<U, T>, bool> = true>
    Vector2& operator*=(U rhs) {
        x *= static_cast<T>(rhs);
        y *= static_cast<T>(rhs);
        return *this;
    }

    template <typename U,
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true,
        std::enable_if_t<std::is_convertible_v<U, T>, bool> = true>
    Vector2& operator/=(U rhs) {
        if (rhs != static_cast<U>(0)) {
            x /= static_cast<T>(rhs);
            y /= static_cast<T>(rhs);
        } else {
            x = std::numeric_limits<T>::infinity();
            y = std::numeric_limits<T>::infinity();
        }
        return *this;
    }

    // Modulo operator on both components for number type vectors.
    Vector2 operator%(const int rhs) const {
        return { x % rhs, y % rhs };
    }
    Vector2 operator%(const Vector2<int>& rhs) const {
        return { x % rhs.x, y % rhs.y };
    }

    // Accessor operators.

    // Access vector elements by index, 0 for x, 1 for y. (reference)
    T& operator[](std::size_t idx) {
        assert(idx < 2 && "Vector2 subscript out of range");
        if (idx == 0) return x;
        return y; // idx == 1
    }
    // Access vector elements by index, 0 for x, 1 for y. (constant)
    T operator[](std::size_t idx) const {
        assert(idx < 2 && "Vector2 subscript out of range");
        if (idx == 0) return x;
        return y; // idx == 1
    }

    // Explicit conversion from this vector to other arithmetic type vectors.
    /*
    template <typename U, 
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true, 
        std::enable_if_t<std::is_convertible_v<T, U>, bool> = true>
    explicit operator Vector2<U>() const {
        return Vector2<U>{ static_cast<U>(x), static_cast<U>(y) }; 
    }
    */

    // Implicit conversion from this vector to other arithmetic type vectors.

    template <typename U,
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
    operator Vector2<U>() const {
        return Vector2<U>{ static_cast<U>(x), static_cast<U>(y) };
    }

    // Vector specific utility functions start here.

    // Return true if both vector components equal 0.
    bool IsZero() const {
        if constexpr (std::is_floating_point_v<T>) {
            return ptgn::math::Compare(x, 0, ptgn::math::VECTOR_EPSILON<T>) && 
                   ptgn::math::Compare(y, 0, ptgn::math::VECTOR_EPSILON<T>);
        }
        return x == static_cast<T>(0) && y == static_cast<T>(0);
    }

    // Return true if either vector component equals 0.
    bool HasZero() const {
        if constexpr (std::is_floating_point_v<T>) {
            return ptgn::math::Compare(x, 0, ptgn::math::VECTOR_EPSILON<T>) ||
                   ptgn::math::Compare(y, 0, ptgn::math::VECTOR_EPSILON<T>);
        }
        return x == static_cast<T>(0) || y == static_cast<T>(0);
    }

    // Return true if both vector components are equal (or close to epsilon).
    bool IsEqual() const {
        if constexpr (std::is_floating_point_v<T>) {
            return ptgn::math::Compare(x, y, ptgn::math::VECTOR_EPSILON<T>);
        }
        return x == y;
    }

    friend void Swap(Vector2& lhs, Vector2& rhs) {
        std::swap(lhs.x, rhs.x);
        std::swap(lhs.y, rhs.y);
    }

    // Return true if both vector components equal numeric limits infinity.
    bool IsInfinite() const {
        return x == std::numeric_limits<T>::infinity() && y == std::numeric_limits<T>::infinity();
    }

    // Return true if either vector component equals numeric limits infinity.
    bool HasInfinity() const {
        return x == std::numeric_limits<T>::infinity() || y == std::numeric_limits<T>::infinity();
    }

    // Return 2D vector projection (dot product).
    template <typename U, 
        typename S = typename std::common_type<T, U>::type>
    S DotProduct(const Vector2<U>& other) const {
        return static_cast<S>(x) * static_cast<S>(other.x) + 
               static_cast<S>(y) * static_cast<S>(other.y);
    }

    // Return area of cross product between x and y components.
    template <typename U, 
        typename S = typename std::common_type<T, U>::type>
    S CrossProduct(const Vector2<U>& other) const {
        return static_cast<S>(x) * static_cast<S>(other.y) - 
               static_cast<S>(y) * static_cast<S>(other.x);
    }

    template <typename U,
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true,
        typename S = typename std::common_type<T, U>::type>
    Vector2<S> CrossProduct(U value) {
        return { static_cast<S>(value) * static_cast<S>(y),
                -static_cast<S>(value) * static_cast<S>(x) };
    }

    // Return a unit vector (normalized).
    template <typename U = double,
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
    Vector2<U> Unit() const {
        // Cache magnitude calculation.
        U m{ Magnitude<U>() };
        // Avoid division by zero error for zero magnitude vectors.
        if (m > 0) return *this / m;
        return static_cast<Vector2<U>>(*this);
    }

    // Return normalized (unit) vector.
    template <typename U = double,
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
    Vector2<U> Normalize() const {
        return Unit<U>();
    }

    // Return identity vector, both components must be 0, 1 or -1.
    template <typename U = T,
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
    Vector2<U> Identity() const {
        return static_cast<U>({ math::Sign(x), math::Sign(y) });
    }

    // Return tangent vector, (x, y) -> (y, -x).
    template <typename U = T,
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
    Vector2<U> Tangent() const {
        return static_cast<U>({ y, -x });
    }

    // Flip signs of both vector components, (x, y) -> (-x, -y).
    template <typename U = T,
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
    Vector2<U> Opposite() const {
        return static_cast<U>(-(*this));
    }

    // Flip places of both vector components, (x, y) -> (y, x).
    Vector2<T> Flip() const {
        return { y, x };
    }

    // Return magnitude squared, x * x + y * y.
    T MagnitudeSquared() const {
        return x * x + y * y;
    }

    // Return magnitude, sqrt(x * x + y * y).
    template <typename U = double,
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
    U Magnitude() const {
        return static_cast<U>(std::sqrt(MagnitudeSquared()));
    }

    template <typename U = T,
        std::enable_if_t<std::is_floating_point_v<U>, bool> = true>
    Vector2<U> Fraction() const {
        return *this - math::Floor(*this);
    }

    // Returns a new vector rotated by the radian angle in the clockwise direction.
    // See https://en.wikipedia.org/wiki/Rotation_matrix for details
    template <typename U = double,
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
    Vector2<U> Rotate(U angle) const {
        return { x * std::cos(angle) - y * std::sin(angle),
                 x * std::sin(angle) + y * std::cos(angle) };
    }

    // Returns the angle between the x and y components of the vector relative to the x-axis (clockwise positive).,
    template <typename U = double,
        std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
    U Angle() const {
        return std::atan2(y, x);
    }

    bool operator==(const ptgn::math::Vector2<T>& rhs) const {
        if constexpr (std::is_floating_point_v<T>) {
            return ptgn::math::Compare(x, rhs.x, ptgn::math::VECTOR_EPSILON<T>) &&
                   ptgn::math::Compare(y, rhs.y, ptgn::math::VECTOR_EPSILON<T>);
        }
        return x == rhs.x && y == rhs.y;
    }

    bool operator!=(const ptgn::math::Vector2<T>& rhs) const {
        return !operator==(rhs);
    }
};

} // namespace math

// Common vector aliases.

using V2_int = math::Vector2<int>;
using V2_uint = math::Vector2<unsigned int>;
using V2_double = math::Vector2<double>;
using V2_float = math::Vector2<float>;

} // namespace ptgn

// Bitshift / stream operators.

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const ptgn::math::Vector2<T>& obj) {
    os << ptgn::math::VECTOR_LEFT_DELIMETER;
    os << obj.x << ptgn::math::VECTOR_CENTER_DELIMETER;
    os << obj.y << ptgn::math::VECTOR_RIGHT_DELIMETER;
    return os;
}

// Comparison operators.

template <typename T, typename U, 
    typename S = typename std::common_type<T, U>::type>
inline bool operator==(const ptgn::math::Vector2<T>& lhs, const ptgn::math::Vector2<U>& rhs) {
    if constexpr (std::is_floating_point_v<T> && std::is_floating_point_v<U>) {
        return ptgn::math::Compare(lhs.x, rhs.x, ptgn::math::VECTOR_EPSILON<T>) &&
               ptgn::math::Compare(lhs.y, rhs.y, ptgn::math::VECTOR_EPSILON<T>);
    }
    return static_cast<S>(lhs.x) == static_cast<S>(rhs.x) &&
           static_cast<S>(lhs.y) == static_cast<S>(rhs.y);
}

template <typename T, typename U>
inline bool operator!=(const ptgn::math::Vector2<T>& lhs, const ptgn::math::Vector2<U>& rhs) {
    return !operator==(lhs, rhs);
}

template <typename T, typename U, 
    typename S = typename std::common_type<T, U>::type,
    std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
inline bool operator==(const ptgn::math::Vector2<T>& lhs, U rhs) {
    if constexpr (std::is_floating_point_v<T> && std::is_floating_point_v<U>) {
        return ptgn::math::Compare(lhs.x, rhs, ptgn::math::VECTOR_EPSILON<T>) &&
               ptgn::math::Compare(lhs.y, rhs, ptgn::math::VECTOR_EPSILON<T>);
    }
    return static_cast<S>(lhs.x) == static_cast<S>(rhs) && 
           static_cast<S>(lhs.y) == static_cast<S>(rhs);
}

template <typename T, typename U, 
    std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
inline bool operator!=(const ptgn::math::Vector2<T>& lhs, U rhs) {
    return !operator==(lhs, rhs);
}

template <typename T, typename U, 
    std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
inline bool operator==(U lhs, const ptgn::math::Vector2<T>& rhs) {
    return operator==(rhs, lhs);
}

template <typename T, typename U, 
    std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
inline bool operator!=(U lhs, const ptgn::math::Vector2<T>& rhs) {
    return !operator==(rhs, lhs);
}

template <typename T>
inline bool operator< (const ptgn::math::Vector2<T>& lhs, const ptgn::math::Vector2<T>& rhs) {
    return lhs.x < rhs.x && lhs.y < rhs.y; 
}

template <typename T>
inline bool operator> (const ptgn::math::Vector2<T>& lhs, const ptgn::math::Vector2<T>& rhs) {
    return  operator< (rhs, lhs);
}

template <typename T>
inline bool operator<=(const ptgn::math::Vector2<T>& lhs, const ptgn::math::Vector2<T>& rhs) {
    return !operator> (lhs, rhs);
}

template <typename T>
inline bool operator>=(const ptgn::math::Vector2<T>& lhs, const ptgn::math::Vector2<T>& rhs) {
    return !operator< (lhs, rhs);
}

// Binary arithmetic operators between two vectors.

template <typename T, typename U>
inline ptgn::math::Vector2<typename std::common_type<T, U>::type> operator+(const ptgn::math::Vector2<T>& lhs, const ptgn::math::Vector2<U>& rhs) {
    return { lhs.x + rhs.x, lhs.y + rhs.y };
}

template <typename T, typename U>
inline ptgn::math::Vector2<typename std::common_type<T, U>::type> operator-(const ptgn::math::Vector2<T>& lhs, const ptgn::math::Vector2<U>& rhs) {
    return { lhs.x - rhs.x, lhs.y - rhs.y };
}

template <typename T, typename U>
inline ptgn::math::Vector2<typename std::common_type<T, U>::type> operator*(const ptgn::math::Vector2<T>& lhs, const ptgn::math::Vector2<U>& rhs) {
    return { lhs.x * rhs.x, lhs.y * rhs.y };
}

template <typename T, typename U,
    typename S = typename std::common_type<T, U>::type>
inline ptgn::math::Vector2<S> operator/(const ptgn::math::Vector2<T>& lhs, const ptgn::math::Vector2<U>& rhs) {
    ptgn::math::Vector2<S> vector;
    if (rhs.x != static_cast<U>(0)) {
        vector.x = lhs.x / rhs.x;
    } else {
        vector.x = std::numeric_limits<S>::infinity();
    }
    if (rhs.y != static_cast<U>(0)) {
        vector.y = lhs.y / rhs.y;
    } else {
        vector.y = std::numeric_limits<S>::infinity();
    }
    return vector;
}

// Binary arithmetic operators with a basic type.

template <typename T, typename U,
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
inline ptgn::math::Vector2<typename std::common_type<T, U>::type> operator+(T lhs, const ptgn::math::Vector2<U>& rhs) {
    return { lhs + rhs.x, lhs + rhs.y };
}

template <typename T, typename U,
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
inline ptgn::math::Vector2<typename std::common_type<T, U>::type> operator-(T lhs, const ptgn::math::Vector2<U>& rhs) {
    return { lhs - rhs.x, lhs - rhs.y };
}

template <typename T, typename U,
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
inline ptgn::math::Vector2<typename std::common_type<T, U>::type> operator*(T lhs, const ptgn::math::Vector2<U>& rhs) {
    return { lhs * rhs.x, lhs * rhs.y };
}

template <typename T, typename U,
    std::enable_if_t<std::is_arithmetic_v<T>, bool> = true,
    typename S = typename std::common_type<T, U>::type>
inline ptgn::math::Vector2<S> operator/(T lhs, const ptgn::math::Vector2<U>& rhs) {
    ptgn::math::Vector2<S> vector;
    if (rhs.x != static_cast<U>(0)) {
        vector.x = lhs / rhs.x;
    } else {
        vector.x = std::numeric_limits<S>::infinity();
    }
    if (rhs.y != static_cast<U>(0)) {
        vector.y = lhs / rhs.y;
    } else {
        vector.y = std::numeric_limits<S>::infinity();
    }
    return vector;
}

template <typename T, typename U,
    std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
inline ptgn::math::Vector2<typename std::common_type<T, U>::type> operator+(const ptgn::math::Vector2<T>& lhs, U rhs) {
    return { lhs.x + rhs, lhs.y + rhs };
}

template <typename T, typename U,
    std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
inline ptgn::math::Vector2<typename std::common_type<T, U>::type> operator-(const ptgn::math::Vector2<T>& lhs, U rhs) {
    return { lhs.x - rhs, lhs.y - rhs };
}

template <typename T, typename U,
    std::enable_if_t<std::is_arithmetic_v<U>, bool> = true>
inline ptgn::math::Vector2<typename std::common_type<T, U>::type> operator*(const ptgn::math::Vector2<T>& lhs, U rhs) {
    return { lhs.x * rhs, lhs.y * rhs };
}

template <typename T, typename U,
    std::enable_if_t<std::is_arithmetic_v<U>, bool> = true,
    typename S = typename std::common_type<T, U>::type>
inline ptgn::math::Vector2<S> operator/(const ptgn::math::Vector2<T>& lhs, U rhs) {
    ptgn::math::Vector2<S> vector;
    if (rhs != static_cast<U>(0)) {
        vector.x = lhs.x / rhs;
        vector.y = lhs.y / rhs;
    } else {
        vector.x = std::numeric_limits<S>::infinity();
        vector.y = std::numeric_limits<S>::infinity();
    }
    return vector;
}

namespace ptgn {

namespace math {

// Special vector functions.

// Cross product function between a number and a vector.
template <typename T, typename U,
    std::enable_if_t<std::is_arithmetic_v<U>, bool> = true,
    typename S = typename std::common_type<T, U>::type>
inline Vector2<S> CrossProduct(U value, const Vector2<T>& vector) {
    return { -static_cast<S>(value) * static_cast<S>(vector.y), 
              static_cast<S>(value) * static_cast<S>(vector.x) };
}

// Return the distance squared between two vectors.
template <typename T, typename U, 
    typename S = typename std::common_type<T, U>::type>
inline S DistanceSquared(const Vector2<T>& lhs, const Vector2<U>& rhs) {
    S x{ lhs.x - rhs.x };
    S y{ lhs.y - rhs.y };
    return x * x + y * y;
}

// Return the distance between two vectors.
template <typename T, typename U, 
    typename S = typename std::common_type<T, U>::type,
    typename V = S,
    std::enable_if_t<std::is_arithmetic_v<V>, bool> = true>
inline V Distance(const Vector2<T>& lhs, const Vector2<U>& rhs) {
    return static_cast<V>(std::sqrt<S>(DistanceSquared(lhs, rhs)));
}

// Return minimum component of vector. (reference)
template <typename T>
inline T& Min(Vector2<T>& vector) {
    return (vector.y < vector.x) ? vector.y : vector.x;
}

// Return maximum component of vector. (reference)
template <typename T>
inline T& Max(Vector2<T>& vector) {
    return (vector.x < vector.y) ? vector.y : vector.x;
}


// Return a vector composed of the minimum components of two vectors.
template <typename T>
inline Vector2<T> Min(const Vector2<T>& a, const Vector2<T>& b) {
    return { math::Min(a.x, b.x), math::Min(a.y, b.y) };
}

// Return a vector composed of the maximum components of two vectors.
template <typename T>
inline Vector2<T> Max(const Vector2<T>& a, const Vector2<T>& b) {
    return { math::Max(a.x, b.x), math::Max(a.y, b.y) };
}

// Return the absolute value of both vectors components.
template <typename T>
inline Vector2<T> Abs(const Vector2<T>& vector) {
    return { math::Abs(vector.x), math::Abs(vector.y) };
}

// Return both vector components rounded to the closest integer.
template <typename S, typename T = S>
inline Vector2<T> Round(const Vector2<S>& vector) {
    return { math::Round<T>(vector.x), math::Round<T>(vector.y) };
}

// Return both vector components ceiled to the closest integer.
template <typename S, typename T = S>
inline Vector2<T> Ceil(const Vector2<S>& vector) {
    return { math::Ceil<T>(vector.x), math::Ceil<T>(vector.y) };
}

// Return both vector components floored to the closest integer.
template <typename S, typename T = S>
inline Vector2<T> Floor(const Vector2<S>& vector) {
    return { math::Floor<T>(vector.x), math::Floor<T>(vector.y) };
}

// Clamp both vector components within a vector range.
template <typename T>
inline Vector2<T> Clamp(const Vector2<T>& value, const Vector2<T>& low, const Vector2<T>& high) {
    return { math::Clamp(value.x, low.x, high.x), math::Clamp(value.y, low.y, high.y) };
}

// Get the sign of both vector components.
template <typename T>
inline Vector2<T> Sign(const Vector2<T>& value) {
    return { math::Sign(value.x), math::Sign(value.y) };
}

// Linearly interpolates both vector components by the given value.
template <typename T, typename U,
    std::enable_if_t<std::is_floating_point_v<U>, bool> = true>
inline Vector2<U> Lerp(const Vector2<T>& a, const Vector2<T>& b, U amount) {
    return { math::Lerp(a.x, b.x, amount), math::Lerp(a.y, b.y, amount) };
}

// Return both vector components smooth stepped.
template <typename T>
inline Vector2<T> SmoothStep(const Vector2<T>& vector) {
    return { math::SmoothStep(vector.x), math::SmoothStep(vector.y) };
}

} // namespace math

} // namespace ptgn

namespace std {

// Custom hashing function for Vector class.
// This allows for use of unordered maps and sets with vectors as keys.
template <typename T>
struct hash<ptgn::math::Vector2<T>> {
    std::size_t operator()(const ptgn::math::Vector2<T>& k) const {
        // Hashing combination algorithm from:
        // https://stackoverflow.com/a/17017281
        std::size_t hash{ 17 };
        hash = hash * 31 + std::hash<T>()(k.x);
        hash = hash * 31 + std::hash<T>()(k.y);
        return hash;
    }
};

} // namespace std