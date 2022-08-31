#pragma once

#include <cstdlib>    // std::size_t
#include <functional> // std::hash
#include <cmath>      // std::roun

#include "type_traits.h"
#include "math.h"

namespace ptgn {

template <typename T,
    type_traits::arithmetic<T> = true>
struct Vector2 {
	T x{};
	T y{};

    constexpr Vector2() = default;
    ~Vector2() = default;

    constexpr Vector2(T x, T y) : 
        x{ x }, 
        y{ y } {}

    template <typename U, type_traits::not_narrowing<U, T> = true>
    constexpr Vector2(const Vector2<U>& o) :
        x{ static_cast<T>(o.x) },
        y{ static_cast<T>(o.y) } {}

    // Note: use of explicit keyword for narrowing constructors.

    template <typename U, type_traits::narrowing<U, T> = true>
    explicit constexpr Vector2(U x, U y) :
        x{ static_cast<T>(x) },
        y{ static_cast<T>(y) } {}

    template <typename U, type_traits::narrowing<U, T> = true>
    explicit constexpr Vector2(const Vector2<U>& o) :
        x{ static_cast<T>(o.x) },
        y{ static_cast<T>(o.y) } {}

    Vector2 operator-() const {
        return { -x, -y };
    }

    template <typename U, type_traits::not_narrowing<U, T> = true>
    Vector2& operator+=(const Vector2<U>& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    template <typename U, type_traits::not_narrowing<U, T> = true>
    Vector2& operator-=(const Vector2<U>& rhs) {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    template <typename U, type_traits::not_narrowing<U, T> = true>
    Vector2& operator*=(const Vector2<U>& rhs) {
        x *= rhs.x;
        y *= rhs.y;
        return *this;
    }

    template <typename U, type_traits::not_narrowing<U, T> = true>
    Vector2& operator/=(const Vector2<U>& rhs) {
        x /= rhs.x;
        y /= rhs.y;
        return *this;
    }

    template <typename U, type_traits::not_narrowing<U, T> = true>
    Vector2& operator*=(U rhs) {
        x *= rhs;
        y *= rhs;
        return *this;
    }

    template <typename U, type_traits::not_narrowing<U, T> = true>
    Vector2& operator/=(U rhs) {
        x /= rhs;
        y /= rhs;
        return *this;
    }

    // Returns the dot product of two vectors.
    T Dot(const Vector2& o) const {
        return x * o.x + y * o.y;
    }

    // Returns a vector with both components rounded to the nearest 0.5.
    Vector2 Rounded() const {
        return { std::round(x), std::round(y) };
    }

    // Returns a new vector rotated by the radian angle in the clockwise direction.
    // See https://en.wikipedia.org/wiki/Rotation_matrix for details
    template <typename U, type_traits::not_narrowing<T, U> = true>
    Vector2<U> Rotated(U rad) const {
        return { x * std::cos(rad) - y * std::sin(rad),
                 x * std::sin(rad) + y * std::cos(rad) };
    }

    // Returns angle between x and y components relative to the x-axis (clockwise positive).
    template <typename U, type_traits::not_narrowing<T, U> = true>
    U Angle() const {
        return std::atan2(y, x);
    }
};

using V2_int = Vector2<int>;
using V2_float = Vector2<float>;
using V2_double = Vector2<double>;

template <typename T>
inline bool operator==(const Vector2<T>& lhs,
                       const Vector2<T>& rhs) {
    return NearlyEqual(lhs.x, rhs.x) &&
           NearlyEqual(lhs.y, rhs.y);
}

template <typename T>
inline bool operator!=(const Vector2<T>& lhs,
                       const Vector2<T>& rhs) {
    return !operator==(lhs, rhs);
}

} // namespace ptgn

// Custom hashing function for Vector2 class.
// This allows for use of unordered maps and sets with Vector2s as keys.
template <typename T>
struct std::hash<ptgn::Vector2<T>> {
    std::size_t operator()(const ptgn::Vector2<T>& v) const noexcept {
        // Hashing combination algorithm from:
        // https://stackoverflow.com/a/17017281
        std::size_t hash{ 17 };
        hash = hash * 31 + std::hash<T>()(v.x);
        hash = hash * 31 + std::hash<T>()(v.y);
        return hash;
    }
};