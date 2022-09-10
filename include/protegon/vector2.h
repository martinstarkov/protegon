#pragma once

#include <cassert>    // assert
#include <cmath>      // std::round
#include <cstdlib>    // std::size_t
#include <functional> // std::hash

#include "type_traits.h"
#include "math.h"
#include "color.h"

namespace ptgn {

namespace impl {

void DrawPoint(int x, int y, const Color& color);

} // namespace impl

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

    // Access vector elements by index, 0 for x, 1 for y.
    T& operator[](std::size_t idx) {
        assert(idx >= 0 && idx < 2 && "Vector2 subscript out of range");
        if (idx == 1)
            return y;
        return x; // idx == 0
    }

    // Access vector elements by index, 0 for x, 1 for y.
    T operator[](std::size_t idx) const {
        assert(idx >= 0 && idx < 2 && "Vector2 subscript out of range");
        if (idx == 1)
            return y;
        return x; // idx == 0
    }

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

    // Returns the dot product (this * o).
    T Dot(const Vector2& o) const {
        return x * o.x + y * o.y;
    }

    // Returns the cross product (this x o).
    T Cross(const Vector2& o) const {
        return x * o.y - y * o.x;
    }

    // Returns a vector with both components rounded to the nearest 0.5.
    Vector2 Rounded() const {
        return { std::round(x), std::round(y) };
    }

    Vector2 FastAbs() const {
        return { ptgn::FastAbs(x), ptgn::FastAbs(y) };
    }

    Vector2 FastCeil() const {
        return { ptgn::FastCeil(x), ptgn::FastCeil(y) };
    }

    Vector2 FastFloor() const {
        return { ptgn::FastFloor(x), ptgn::FastFloor(y) };
    }

    // Returns a new vector rotated by the radian angle in the clockwise direction.
    // See https://en.wikipedia.org/wiki/Rotation_matrix for details
    template <typename U, type_traits::not_narrowing<T, U> = true>
    Vector2<U> Rotated(U rad) const {
        return { x * std::cos(rad) - y * std::sin(rad),
                 x * std::sin(rad) + y * std::cos(rad) };
    }

    /*
    * Returns angle between vector x and y components in radians.
    * Relative to the horizontal x-axis.
    * Range: [-3.14159, 3.14159).
    * (counter-clockwise positive).
    *             1.5708
    *               |
    *    3.14159 ---o--- 0
    *               |
    *            -1.5708
    */
    template <typename U, type_traits::not_narrowing<T, U> = true>
    U Angle() const {
        return std::atan2(y, x);
    }

    bool IsZero() const {
        return NearlyEqual(x, static_cast<T>(0)) &&
               NearlyEqual(y, static_cast<T>(0));
    }

    void Draw(const Color& color) const {
        impl::DrawPoint(x, y, color);
    }
};

using V2_int = Vector2<int>;
using V2_float = Vector2<float>;
using V2_double = Vector2<double>;
template <typename T>
using Point = Vector2<T>;

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

template <typename T, typename U,
    typename S = typename std::common_type_t<T, U>>
inline Vector2<S> operator+(const Vector2<T>& lhs,
                            const Vector2<U>& rhs) {
    return { lhs.x + rhs.x, lhs.y + rhs.y };
}

template <typename T, typename U,
    typename S = typename std::common_type_t<T, U>>
inline Vector2<S> operator-(const Vector2<T>& lhs,
                            const Vector2<U>& rhs) {
    return { lhs.x - rhs.x, lhs.y - rhs.y };
}

template <typename T, typename U,
    typename S = typename std::common_type_t<T, U>>
inline Vector2<S> operator*(const Vector2<T>& lhs,
                            const Vector2<U>& rhs) {
    return { lhs.x * rhs.x, lhs.y * rhs.y };
}

template <typename T, typename U,
    typename S = typename std::common_type_t<T, U>>
inline Vector2<S> operator/(const Vector2<T>& lhs,
                            const Vector2<U>& rhs) {
    return { lhs.x / rhs.x, lhs.y / rhs.y };
}

template <typename T, typename U,
    type_traits::arithmetic<T> = true,
    typename S = typename std::common_type_t<T, U>>
inline Vector2<S> operator*(T lhs,
                            const Vector2<U>& rhs) {
    return { lhs * rhs.x, lhs * rhs.y };
}

template <typename T, typename U,
    type_traits::arithmetic<U> = true,
    typename S = typename std::common_type_t<T, U>>
inline Vector2<S> operator*(const Vector2<T>& lhs,
                            U rhs) {
    return { lhs.x * rhs, lhs.y * rhs };
}

template <typename T, typename U,
    type_traits::arithmetic<T> = true,
    typename S = typename std::common_type_t<T, U>>
inline Vector2<S> operator/(T lhs,
                            const Vector2<U>& rhs) {
    return { lhs / rhs.x, lhs / rhs.y };
}

template <typename T, typename U,
    type_traits::arithmetic<T> = true,
    typename S = typename std::common_type_t<T, U>>
inline Vector2<S> operator/(const Vector2<T>& lhs,
                            U rhs) {
    return { lhs.x / rhs, lhs.y / rhs };
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