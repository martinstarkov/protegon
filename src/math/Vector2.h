#pragma once

#include <cstdlib>   // std::size_t
#include <cstdint>   // std::int32_t
#include <algorithm> // std::swap
#include <iostream>  // std::ostream, std::istream
#include <cmath>     // std::round
#include <cassert>   // assert

#include "math/Math.h"
#include "utility/TypeTraits.h"

namespace ptgn {

namespace math {

template <typename T>
struct Vector2 {
    static_assert(std::is_arithmetic_v<T>, "Cannot construct Vector2 with non-arithmetic type");
    T x{ 0 };
    T y{ 0 };

    Vector2() = default;
    ~Vector2() noexcept = default;
    Vector2(const Vector2&) = default;
    Vector2& operator=(const Vector2&) = default;
    Vector2(Vector2&&) noexcept = default;
    Vector2& operator=(Vector2&&) noexcept = default;

    template <typename U, typename V,
        tt::arithmetic<U> = true,
        tt::convertible<U, T> = true,
        tt::not_narrowing<U, T> = true,
        tt::arithmetic<V> = true,
        tt::convertible<V, T> = true,
        tt::not_narrowing<V, T> = true>
    constexpr Vector2(U x, V y) :
        x{ static_cast<T>(x) }, 
        y{ static_cast<T>(y) } {}

    template <typename U,
        tt::convertible<U, T> = true>
    Vector2& operator+=(const Vector2<U>& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    template <typename U,
        tt::convertible<U, T> = true>
    Vector2& operator-=(const Vector2<U>& rhs) {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    template <typename U,
        tt::convertible<U, T> = true>
    Vector2& operator*=(const Vector2<U>& rhs) {
        x *= rhs.x;
        y *= rhs.y;
        return *this;
    }

    template <typename U,
        tt::convertible<U, T> = true>
    Vector2& operator/=(const Vector2<U>& rhs) {
        if constexpr (std::is_integral_v<T>)
            assert(!math::Compare(rhs.x, T{ 0 }) &&
                   !math::Compare(rhs.y, T{ 0 }) && "Vector2 division by zero");
        x /= rhs.x;
        y /= rhs.y;
        return *this;
    }

    template <typename U,
        tt::arithmetic<U> = true,
        tt::convertible<U, T> = true>
    Vector2& operator*=(U rhs) {
        x *= rhs;
        y *= rhs;
        return *this;
    }

    template <typename U,
        tt::arithmetic<U> = true,
        tt::convertible<U, T> = true>
    Vector2& operator/=(U rhs) {
        if constexpr (std::is_integral_v<T>)
            assert(!math::Compare(rhs, T{ 0 }) && "Vector2 division by zero");
        x /= rhs;
        y /= rhs;
        return *this;
    }

    // Access vector elements by index, 0 for x, 1 for y.
    T& operator[](std::size_t idx) {
        assert(idx < 2 && "Vector2 subscript out of range");
        if (idx == 0)
            return x;
        return y; // idx == 1
    }

    // Access vector elements by index, 0 for x, 1 for y.
    T operator[](std::size_t idx) const {
        assert(idx < 2 && "Vector2 subscript out of range");
        if (idx == 0)
            return x;
        return y; // idx == 1
    }

    /*template <typename U,
        tt::arithmetic<U> = true,
        tt::convertible<T, U> = true>
    operator Vector2<U>() const {
        return Vector2<U>{ x, y };
    }*/

    template <typename To,
        tt::arithmetic<To> = true,
        tt::convertible<T, To> = true>
        // Adding the line below makes only non-narrowing static casts allowed.
        //tt::not_narrowing<T, To> = true>
    operator Vector2<To>() const {
        return Vector2<To>{ static_cast<To>(x), static_cast<To>(y) };
    }

    bool IsZero() const {
        if constexpr (std::is_floating_point_v<T>)
            return math::Compare(x, T{ 0 }) && math::Compare(y, T{ 0 });
        return x == 0 && y == 0;
    }

    bool HasZero() const {
        if constexpr (std::is_floating_point_v<T>)
            return math::Compare(x, T{ 0 }) || math::Compare(y, T{ 0 });
        return x == 0 || y == 0;
    }

    bool IsEqual() const {
        if constexpr (std::is_floating_point_v<T>)
            return math::Compare(x, y);
        return x == y;
    }

    Vector2 operator-() const {
        return { -x, -y };
    }

    // Flip signs of both vector components.
    Vector2 Opposite() const {
        return -(*this);
    }

    // Both components will be either 0, 1 or -1.
    Vector2 Identity() const {
        return { math::Sign(x), math::Sign(y) };
    }

    Vector2 Tangent() const {
        return { y, -x };
    }

    // Flip places of both vector components.
    Vector2 Flip() const {
        return { y, x };
    }

    friend void swap(Vector2& lhs, Vector2& rhs) {
        std::swap(lhs.x, rhs.x);
        std::swap(lhs.y, rhs.y);
    }

    // Returns 2D vector projection (dot product).
    template <typename U,
        tt::arithmetic<U> = true,
        tt::convertible<U, T> = true,
        typename S = typename std::common_type_t<T, U>>
    S Dot(const Vector2<U>& rhs) const {
        return x * rhs.x + y * rhs.y;
    }

    // Return area of parallelogram formed by x and y component sides.
    template <typename U,
        tt::arithmetic<U> = true,
        tt::convertible<U, T> = true,
        typename S = typename std::common_type_t<T, U>>
    S Cross(const Vector2<U>& rhs) const {
        return x * rhs.y - y * rhs.x;
    }

    T MagnitudeSquared() const {
        return Dot(*this);
    }

    template <typename U = float,
        tt::arithmetic<U> = true,
        tt::convertible<T, U> = true,
        typename S = typename std::common_type_t<T, U, float>>
    S Magnitude() const {
        return std::sqrtf(MagnitudeSquared());
    }

    template <typename U,
        tt::arithmetic<U> = true,
        tt::convertible<U, T> = true,
        typename S = typename std::common_type_t<T, U>>
    S DistanceSquared(const Vector2<U>& rhs) const {
        const Vector2<U> n{ x - rhs.x, y - rhs.y };
        return n.x * n.x + n.y * n.y;
    }

    template <typename U,
        tt::arithmetic<U> = true,
        tt::convertible<U, T> = true,
        typename S = typename std::common_type_t<T, U, float>>
    S Distance(const Vector2<U>& rhs) const {
        return std::sqrtf(DistanceSquared(rhs));
    }

    // Returns a unit vector (magnitude = 1) except for zero vectors (magnitude = 0).
    template <typename U = float,
        tt::arithmetic<U> = true,
        tt::convertible<T, U> = true,
        typename S = typename std::common_type_t<T, U, float>>
    Vector2<S> Normalize() const {
        T m{ MagnitudeSquared() };
        if (Compare(m, T{ 0 }) || Compare(m, T{ 1 }))
            return *this;
        return *this / std::sqrtf(m);
    }

    // Returns a new vector rotated by the radian angle in the clockwise direction.
    // See https://en.wikipedia.org/wiki/Rotation_matrix for details
    template <typename U = float,
        tt::floating_point<U> = true,
        tt::convertible<T, U> = true>
    Vector2<U> Rotate(U rad) const {
        return { x * std::cos(rad) - y * std::sin(rad),
                 x * std::sin(rad) + y * std::cos(rad) };
    }

    // Returns angle between x and y components relative to the x-axis (clockwise positive).
    template <typename U = float,
        tt::arithmetic<U> = true,
        tt::convertible<T, U> = true>
    U Angle() const {
        return std::atan2(y, x);
    }

    bool operator==(const Vector2& rhs) const {
        if constexpr (std::is_floating_point_v<T>)
            return math::Compare(x, rhs.x) && math::Compare(y, rhs.y);
        return x == rhs.x && y == rhs.y;
    }

    bool operator!=(const Vector2& rhs) const {
        return !operator==(rhs);
    }
};

} // namespace math

// Common Vector2 aliases.

using V2_int = math::Vector2<int>;
using V2_lint = math::Vector2<long int>;
using V2_uint = math::Vector2<unsigned int>;
using V2_double = math::Vector2<double>;
using V2_float = math::Vector2<float>;

} // namespace ptgn

template <typename T>
inline std::ostream& operator<<(std::ostream& os,
                                const ptgn::math::Vector2<T>& obj) {
    os << '(' << obj.x << ',' << obj.y << ')';
    return os;
}

template <typename T, typename U,
    typename S = typename std::common_type_t<T, U>>
inline bool operator==(const ptgn::math::Vector2<T>& lhs,
                       const ptgn::math::Vector2<U>& rhs) {
    if constexpr (std::is_floating_point_v<S>)
        return ptgn::math::Compare(lhs.x, rhs.x) &&
               ptgn::math::Compare(lhs.y, rhs.y);
    return lhs.x == rhs.x &&
           lhs.y == rhs.y;
}

template <typename T, typename U>
inline bool operator!=(const ptgn::math::Vector2<T>& lhs,
                       const ptgn::math::Vector2<U>& rhs) {
    return !operator==(lhs, rhs);
}

template <typename T, typename U,
    typename S = typename std::common_type_t<T, U>>
inline ptgn::math::Vector2<S> operator+(const ptgn::math::Vector2<T>& lhs,
                                        const ptgn::math::Vector2<U>& rhs) {
    return { lhs.x + rhs.x, lhs.y + rhs.y };
}

template <typename T, typename U,
    typename S = typename std::common_type_t<T, U>>
inline ptgn::math::Vector2<S> operator-(const ptgn::math::Vector2<T>& lhs,
                                        const ptgn::math::Vector2<U>& rhs) {
    return { lhs.x - rhs.x, lhs.y - rhs.y };
}

template <typename T, typename U,
    typename S = typename std::common_type_t<T, U>>
inline ptgn::math::Vector2<S> operator*(const ptgn::math::Vector2<T>& lhs,
                                        const ptgn::math::Vector2<U>& rhs) {
    return { lhs.x * rhs.x, lhs.y * rhs.y };
}

template <typename T, typename U,
    typename S = typename std::common_type_t<T, U>>
inline ptgn::math::Vector2<S> operator/(const ptgn::math::Vector2<T>& lhs,
                                        const ptgn::math::Vector2<U>& rhs) {
    if constexpr (std::is_integral_v<S>)
        assert(rhs.x != 0 && rhs.y != 0 && "Vector2 division by zero");
    return { lhs.x / rhs.x, lhs.y / rhs.y };
}

template <typename T, typename U,
    ptgn::tt::arithmetic<T> = true, 
    typename S = typename std::common_type_t<T, U>>
inline ptgn::math::Vector2<S> operator*(T lhs,
                                        const ptgn::math::Vector2<U>& rhs) {
    return { lhs * rhs.x, lhs * rhs.y };
}

template <typename T, typename U,
    ptgn::tt::arithmetic<U> = true,
    typename S = typename std::common_type_t<T, U>>
inline ptgn::math::Vector2<S> operator*(const ptgn::math::Vector2<T>& lhs,
                                        U rhs) {
    return { lhs.x * rhs, lhs.y * rhs };
}

template <typename T, typename U,
    ptgn::tt::arithmetic<T> = true,
    typename S = typename std::common_type_t<T, U>>
inline ptgn::math::Vector2<S> operator/(T lhs,
                                        const ptgn::math::Vector2<U>& rhs) {
    if constexpr (std::is_integral_v<S>)
        assert(rhs.x != 0 && rhs.y != 0 && "Vector2 division by zero");
    return { lhs / rhs.x, lhs / rhs.y };
}

template <typename T, typename U,
    ptgn::tt::arithmetic<T> = true,
    typename S = typename std::common_type_t<T, U>>
inline ptgn::math::Vector2<S> operator/(const ptgn::math::Vector2<T>& lhs,
                                        U rhs) {
    if constexpr (std::is_integral_v<S>)
        assert(rhs != 0 && "Vector2 division by zero");
    return { lhs.x / rhs, lhs.y / rhs };
}

namespace ptgn {

namespace math {

template <typename T, typename U,
    typename S = typename std::common_type_t<T, U>>
inline S DistanceSquared(const Vector2<T>& lhs,
                         const Vector2<U>& rhs) {
    return lhs.DistanceSquared(rhs);
}

template <typename T, typename U,
    typename S = typename std::common_type_t<T, U, float>>
inline S Distance(const Vector2<T>& lhs,
                  const Vector2<U>& rhs) {
    return lhs.Distance(rhs);
}

// Return minimum component of vector.
template <typename T>
inline T& Min(Vector2<T>& vector) {
    return (vector.y < vector.x) ? vector.y : vector.x;
}

// Return maximum component of vector.
template <typename T>
inline T& Max(Vector2<T>& vector) {
    return (vector.x < vector.y) ? vector.y : vector.x;
}

// Return a vector composed of the minimum components of two vectors.
template <typename T>
inline Vector2<T> Min(const Vector2<T>& a,
                      const Vector2<T>& b) {
    return { std::min(a.x, b.x), std::min(a.y, b.y) };
}

// Return a vector composed of the maximum components of two vectors.
template <typename T>
inline Vector2<T> Max(const Vector2<T>& a,
                      const Vector2<T>& b) {
    return { std::max(a.x, b.x), std::max(a.y, b.y) };
}

template <typename T>
inline Vector2<T> Abs(const Vector2<T>& vector) {
    return { std::abs(vector.x), std::abs(vector.y) };
}

template <typename T>
inline Vector2<T> FastAbs(const Vector2<T>& vector) {
    return { math::FastAbs(vector.x), math::FastAbs(vector.y) };
}

template <typename T>
inline Vector2<T> Round(const Vector2<T>& vector) {
    return { std::round(vector.x), std::round(vector.y) };
}

template <typename T>
inline Vector2<T> Ceil(const Vector2<T>& vector) {
    return { std::ceil(vector.x), std::ceil(vector.y) };
}

template <typename T>
inline Vector2<T> FastCeil(const Vector2<T>& vector) {
    return { math::FastCeil(vector.x), math::FastCeil(vector.y) };
}

template <typename T>
inline Vector2<T> Floor(const Vector2<T>& vector) {
    return { std::floor(vector.x), std::floor(vector.y) };
}

template <typename T>
inline Vector2<T> FastFloor(const Vector2<T>& vector) {
    return { math::FastFloor(vector.x), math::FastFloor(vector.y) };
}

// Clamp both vector components within a vector range.
template <typename T>
inline Vector2<T> Clamp(const Vector2<T>& value,
                        const Vector2<T>& low,
                        const Vector2<T>& high) {
    return { std::clamp(value.x, low.x, high.x), std::clamp(value.y, low.y, high.y) };
}

// Get the sign of both vector components.
template <typename T>
inline Vector2<T> Sign(const Vector2<T>& value) {
    return { math::Sign(value.x), math::Sign(value.y) };
}

// Linearly interpolates both vector components by the given value.
template <typename T, typename U,
    tt::floating_point<U> = true>
inline Vector2<U> Lerp(const Vector2<T>& a,
                       const Vector2<T>& b,
                       U amount) {
    return { std::lerp(a.x, b.x, amount), std::lerp(a.y, b.y, amount) };
}

} // namespace math

} // namespace ptgn

namespace std {

// Custom hashing function for Vector class.
// This allows for use of unordered maps and sets with vectors as keys.
template <typename T>
struct hash<ptgn::math::Vector2<T>> {
    std::size_t operator()(const ptgn::math::Vector2<T>& k) const noexcept {
        // Hashing combination algorithm from:
        // https://stackoverflow.com/a/17017281
        std::size_t hash{ 17 };
        hash = hash * 31 + std::hash<T>()(k.x);
        hash = hash * 31 + std::hash<T>()(k.y);
        return hash;
    }
};

} // namespace std