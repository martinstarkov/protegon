#pragma once

#include <algorithm> // std::swap
#include <iostream> // std::ostream, std::istream
#include <type_traits> // std::is_arithmetic_v, std::is_convertible_v, std::enable_if_t, std::common_type
#include <random> // std::minstd_rand, std::uniform_real_distribution, std::uniform_int_distribution
#include <string> // std::string
#include <limits> // std::numeric_limits
#include <cmath> // std::round
#include <cstdint> // std::int32_t, etc
#include <cstdlib> // std::size_t
#include <cassert> // assert

#include "math/Math.h"
#include "math/RNG.h"

// TODO: Write tests for clamp.
// TODO: Write tests for lerp.

namespace engine {

namespace internal {

// Vector stream output / input delimeters, allow for consistent serialization / deserialization.

static constexpr const char VECTOR_LEFT_DELIMETER{ '(' };
static constexpr const char VECTOR_CENTER_DELIMETER{ ',' };
static constexpr const char VECTOR_RIGHT_DELIMETER{ ')' };

} // namespace internal

} // namespace engine

/*
* @tparam T Type contained in vector.
*/
template <typename T, 
    engine::type_traits::is_number<T> = true
>
struct Vector2 {

    // Zero construction by default.

    T x{ 0 };
    T y{ 0 };

    Vector2() = default;

    ~Vector2() = default;
    
    // Allow construction from two different types, cast to the vector type.
    template <typename U, typename V, 
        engine::type_traits::is_number<U> = true, 
        engine::type_traits::is_number<V> = true
    >
    Vector2(U x, V y) : x{ static_cast<T>(x) }, y{ static_cast<T>(y) } {}

    // Constructing vector from string used for deserializing vectors.
    // Important: we assume that the string has already been filtered down to just the vector characters (no whitespace around).
    Vector2(const std::string& s) {
        assert(s.at(0) == engine::internal::VECTOR_LEFT_DELIMETER && "Vector2 construction string must start with VECTOR_LEFT_DELIMETER, check for possible whitespace");
        assert(s.at(s.size() - 1) == engine::internal::VECTOR_RIGHT_DELIMETER && "Vector2 construction string must end with VECTOR_RIGHT_DELIMETER, check for possible whitespace");
        // Find the index of the center delimeter from the string.
        auto center_delimeter = s.find(engine::internal::VECTOR_CENTER_DELIMETER);
        assert(center_delimeter != std::string::npos && "Vector2 construction string must contain VECTOR_CENTER_DELIMETER");
        assert(s.find('\n') == std::string::npos && "Vector2 construction string cannot contain any newlines");
        assert(s.find(' ') == std::string::npos && "Vector2 construction string cannot contain any whitespace");
        // Find the substring from after the left delimeter to right before the center delimeter.
        // Then turn into double and cast to the appropriate type.
        x = static_cast<T>(std::stod(std::move(s.substr(1, center_delimeter - 1))));
        // Find the substring from after the center delimeter until the end of the string, excluding the right delimeter.
        // Then turn into double and cast to the appropriate type.
        y = static_cast<T>(std::stod(std::move(s.substr(center_delimeter + 1, s.size() - 2))));
    }

    // Copy / assignment construction.

    Vector2(const Vector2& vector) = default;
    Vector2(Vector2&& vector) = default;
    Vector2& operator=(const Vector2& rhs) = default;
    Vector2& operator=(Vector2&& rhs) = default;

    // Unary increment / decrement / minus operators.

    Vector2& operator++() {
        ++x; ++y;
        return *this;
    }

    Vector2 operator++(int) {
        Vector2 tmp(*this);
        operator++();
        return tmp;
    }

    Vector2& operator--() {
        --x; --y;
        return *this;
    }

    Vector2 operator--(int) {
        Vector2 tmp(*this);
        operator--();
        return tmp;
    }

    Vector2 operator-() const {
        return { -x, -y };
    }

    // Arithmetic operators between vectors.

    template <typename U,
        engine::type_traits::convertible<U, T> = true
    >
    Vector2& operator+=(const Vector2<U>& rhs) {
        x += static_cast<T>(rhs.x);
        y += static_cast<T>(rhs.y);
        return *this;
    }

    template <typename U, 
        engine::type_traits::convertible<U, T> = true
    >
    Vector2& operator-=(const Vector2<U>& rhs) {
        x -= static_cast<T>(rhs.x);
        y -= static_cast<T>(rhs.y);
        return *this;
    }

    template <typename U, 
        engine::type_traits::convertible<U, T> = true
    >
    Vector2& operator*=(const Vector2<U>& rhs) {
        x *= static_cast<T>(rhs.x);
        y *= static_cast<T>(rhs.y);
        return *this;
    }

    template <typename U, 
        engine::type_traits::convertible<U, T> = true
    >
    Vector2& operator/=(const Vector2<U>& rhs) {
        if (rhs.x) {
            x /= static_cast<T>(rhs.x);
        } else {
            x = std::numeric_limits<T>::infinity();
        }
        if (rhs.y) {
            y /= static_cast<T>(rhs.y);
        } else {
            y = std::numeric_limits<T>::infinity();
        }
        return *this;
    }

    // Arithmetic operators with basic types.

    template <typename U,
        engine::type_traits::is_number<U> = true,
        engine::type_traits::convertible<U, T> = true
    >
    Vector2& operator+=(U rhs) {
        x += static_cast<T>(rhs);
        y += static_cast<T>(rhs);
        return *this;
    }

    template <typename U, 
        engine::type_traits::is_number<U> = true, 
        engine::type_traits::convertible<U, T> = true
    >
    Vector2& operator-=(U rhs) {
        x -= static_cast<T>(rhs);
        y -= static_cast<T>(rhs);
        return *this;
    }

    template <typename U,
        engine::type_traits::is_number<U> = true,
        engine::type_traits::convertible<U, T> = true
    >
    Vector2& operator*=(U rhs) {
        x *= static_cast<T>(rhs);
        y *= static_cast<T>(rhs);
        return *this;
    }

    template <typename U,
        engine::type_traits::is_number<U> = true,
        engine::type_traits::convertible<U, T> = true
    >
    Vector2& operator/=(U rhs) {
        if (rhs) {
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
        return {
            x % rhs,
            y % rhs
        };
    }

    // Accessor operators.

    // Access vector elements by index, 0 for x, 1 for y. (reference)
    T& operator[](std::size_t idx) {
        assert(idx < 2 && "Vector2 subscript out of range");
        if (idx == 0) {
            return x;
        }
        return y; // idx == 1
    }
    // Access vector elements by index, 0 for x, 1 for y. (constant)
    T operator[](std::size_t idx) const {
        assert(idx < 2 && "Vector2 subscript out of range");
        if (idx == 0) {
            return x;
        }
        return y; // idx == 1
    }

    // Explicit conversion from this vector to other arithmetic type vectors.
    /*template <typename U, engine::type_traits::is_number<U> = true, engine::type_traits::convertible<T, U> = true>
    explicit operator Vector2<U>() const { return Vector2<U>{ static_cast<U>(x), static_cast<U>(y) }; }*/

    // Implicit conversion from this vector to other arithmetic types which are less wide.

    operator Vector2<int>() const { return Vector2<int>{ static_cast<int>(x), static_cast<int>(y) };
    }
    operator Vector2<double>() const { return Vector2<double>{ static_cast<double>(x), static_cast<double>(y) }; }
    operator Vector2<float>() const { return Vector2<float>{ static_cast<float>(x), static_cast<float>(y) }; }
    operator Vector2<unsigned int>() const { return Vector2<unsigned int>{ static_cast<unsigned int>(x), static_cast<unsigned int>(y) }; }

    // Vector specific utility functions start here.

    // Return true if both vector components equal 0.
    inline bool IsZero() const {
        return !x && !y;
    }
    // Return true if either vector component equals 0.
    inline bool HasZero() const {
        return !x || !y;
    }

    friend inline void Swap(Vector2& lhs, Vector2& rhs) {
        std::swap(lhs.x, rhs.x);
        std::swap(lhs.y, rhs.y);
    }

    // Return true if both vector components equal numeric limits infinity.
    inline bool IsInfinite() const {
        if constexpr (std::is_floating_point_v<T>) {
            return x == std::numeric_limits<T>::infinity() && y == std::numeric_limits<T>::infinity();
        }
        return false;
    }
    // Return true if either vector component equals numeric limits infinity.
    inline bool HasInfinity() const {
        if constexpr (std::is_floating_point_v<T>) {
            return x == std::numeric_limits<T>::infinity() || y == std::numeric_limits<T>::infinity();
        }
        return false;
    }
    // Return a vector with numeric_limit::infinity() set for both components
    static Vector2 Infinite() {
        static_assert(std::is_floating_point_v<T>, "Cannot create infinite vector for integer type. Must use floating points.");
        return {
            std::numeric_limits<T>::infinity(),
            std::numeric_limits<T>::infinity()
        };
    }

    // Return a vector with std::numeric_limits<T>::max() set for both components
    static Vector2 Maximum() {
        return { 
            std::numeric_limits<T>::max(),
            std::numeric_limits<T>::max()
        };
    }
    // Return a vector with std::numeric_limits<T>::min() set for both components
    static Vector2 Minimum() {
        return {
            std::numeric_limits<T>::min(),
            std::numeric_limits<T>::min()
        };
    }
    // Return a vector with both components randomized in the given ranges.
    static Vector2 Random(T min_x = 0.0, T max_x = 1.0, T min_y = 0.0, T max_y = 1.0) {
        assert(min_x < max_x && "Minimum random value must be less than maximum random value");
        assert(min_y < max_y && "Minimum random value must be less than maximum random value");
        engine::RNG<T> rng_x{ min_x, max_x };
        engine::RNG<T> rng_y{ min_y, max_y };
        // Vary distribution type based on template parameter type.
        return {
            rng_x(),
            rng_y()
        };
    }
    // Return 2D vector projection (dot product).
    template <typename U, typename S = typename std::common_type<T, U>::type>
    inline S DotProduct(const Vector2<U>& other) const {
        return static_cast<S>(x) * static_cast<S>(other.x) + static_cast<S>(y) * static_cast<S>(other.y);
    }
    // Return area of cross product between x and y components.
    template <typename U, typename S = typename std::common_type<T, U>::type>
    inline S CrossProduct(const Vector2<U>& other) const {
        return static_cast<S>(x) * static_cast<S>(other.y) - static_cast<S>(y) * static_cast<S>(other.x);
    }

    template <typename U, typename S = typename std::common_type<T, U>::type>
    inline Vector2<S> CrossProduct(U value) {
        return { static_cast<S>(value) * static_cast<S>(y), -static_cast<S>(value) * static_cast<S>(x) };
    }

    // Return a unit vector (normalized).
    inline auto Unit() const {
        // Cache magnitude calculation.
        auto m{ Magnitude<T>() };
        // Avoid division by zero error for zero magnitude vectors.
        if (m > 0) {
            return *this / m;
        }
        return Vector2<T>{};
    }

    // Return normalized (unit) vector.
    inline auto Normalized() const {
        return Unit();
    }
    // Return identity vector, both components must be 0, 1 or -1.
    inline Vector2 Identity() const {
        return { engine::math::Sign(x), engine::math::Sign(y) };
    }
    // Return tangent vector, (x, y) -> (y, -x).
    inline Vector2 Tangent() const {
        return { y, -x };
    }
    // Flip signs of both vector components, (x, y) -> (-x, -y).
    inline Vector2 Opposite() const {
        return -(*this);
    }
    // Return magnitude squared, x * x + y * y.
    inline T MagnitudeSquared() const {
        return x * x + y * y;
    }
    // Return magnitude, sqrt(x * x + y * y).
    template <typename T = double, 
        engine::type_traits::is_number<T> = true>
    inline T Magnitude() const {
        return static_cast<T>(std::sqrt(MagnitudeSquared()));
    }
    template <typename S = T,
        engine::type_traits::is_floating_point<S> = true>
    Vector2<T> Fraction() const {
        return *this - Floor(*this);
    }
};

// Common vector aliases.

using V2_int = Vector2<int>;
using V2_uint = Vector2<unsigned int>;
using V2_double = Vector2<double>;
using V2_float = Vector2<float>;

// Bitshift / stream operators.

template <typename T>
std::ostream& operator<<(std::ostream& os, const Vector2<T>& obj) {
    os << engine::internal::VECTOR_LEFT_DELIMETER << obj.x << engine::internal::VECTOR_CENTER_DELIMETER << obj.y << engine::internal::VECTOR_RIGHT_DELIMETER;
    return os;
}
template <typename T>
std::istream& operator>>(std::istream& is, Vector2<T>& obj) {
    std::string tmp;
    // Pass istream into string.
    is >> tmp;
    // Construct object from string.
    obj = std::move(Vector2<T>{ tmp });
    return is;
}

// Comparison operators.

template <typename T, typename U, typename S = typename std::common_type<T, U>::type>
inline bool operator==(const Vector2<T>& lhs, const Vector2<U>& rhs) {
    return static_cast<S>(lhs.x) == static_cast<S>(rhs.x) && static_cast<S>(lhs.y) == static_cast<S>(rhs.y);
}
template <typename T, typename U>
inline bool operator!=(const Vector2<T>& lhs, const Vector2<U>& rhs) {
    return !operator==(lhs, rhs);
}
template <typename T, typename U, engine::type_traits::is_number<U> = true, typename S = typename std::common_type<T, U>::type>
inline bool operator==(const Vector2<T>& lhs, U rhs) {
    return static_cast<S>(lhs.x) == static_cast<S>(rhs) && static_cast<S>(lhs.y) == static_cast<S>(rhs);
}
template <typename T, typename U, engine::type_traits::is_number<U> = true>
inline bool operator!=(const Vector2<T>& lhs, U rhs) {
    return !operator==(lhs, rhs);
}
template <typename T, typename U, engine::type_traits::is_number<U> = true>
inline bool operator==(U lhs, const Vector2<T>& rhs) {
    return operator==(rhs, lhs);
}
template <typename T, typename U, engine::type_traits::is_number<U> = true>
inline bool operator!=(U lhs, const Vector2<T>& rhs) {
    return !operator==(rhs, lhs);
}
/*
// Possibly include these comparisons in the future.
template <typename T>
inline bool operator< (const Vector2<T>& lhs, const Vector2<T>& rhs) { return lhs.x < rhs.x && lhs.y < rhs.y; }
template <typename T>
inline bool operator> (const Vector2<T>& lhs, const Vector2<T>& rhs) { return  operator< (rhs, lhs); }
template <typename T>
inline bool operator<=(const Vector2<T>& lhs, const Vector2<T>& rhs) { return !operator> (lhs, rhs); }
template <typename T>
inline bool operator>=(const Vector2<T>& lhs, const Vector2<T>& rhs) { return !operator< (lhs, rhs); }
*/

// Binary arithmetic operators between two vectors.

template <typename T, typename U>
Vector2<typename std::common_type<T, U>::type> operator+(const Vector2<T>& lhs, const Vector2<U>& rhs) {
    return { lhs.x + rhs.x, lhs.y + rhs.y };
}
template <typename T, typename U>
Vector2<typename std::common_type<T, U>::type> operator-(const Vector2<T>& lhs, const Vector2<U>& rhs) {
    return { lhs.x - rhs.x, lhs.y - rhs.y };
}
template <typename T, typename U>
Vector2<typename std::common_type<T, U>::type> operator*(const Vector2<T>& lhs, const Vector2<U>& rhs) {
    return { lhs.x * rhs.x, lhs.y * rhs.y };
}
template <typename T, typename U, typename S = typename std::common_type<T, U>::type>
Vector2<S> operator/(const Vector2<T>& lhs, const Vector2<U>& rhs) {
    Vector2<S> vector;
    if (rhs.x) {
        vector.x = lhs.x / rhs.x;
    } else {
        vector.x = std::numeric_limits<S>::infinity();
    }
    if (rhs.y) {
        vector.y = lhs.y / rhs.y;
    } else {
        vector.y = std::numeric_limits<S>::infinity();
    }
    return vector;
}

// Binary arithmetic operators with a basic type.

template <typename T, typename U, engine::type_traits::is_number<T> = true>
Vector2<typename std::common_type<T, U>::type> operator+(T lhs, const Vector2<U>& rhs) {
    return { lhs + rhs.x, lhs + rhs.y };
}
template <typename T, typename U, engine::type_traits::is_number<T> = true>
Vector2<typename std::common_type<T, U>::type> operator-(T lhs, const Vector2<U>& rhs) {
    return { lhs - rhs.x, lhs - rhs.y };
}
template <typename T, typename U, engine::type_traits::is_number<T> = true>
Vector2<typename std::common_type<T, U>::type> operator*(T lhs, const Vector2<U>& rhs) {
    return { lhs * rhs.x, lhs * rhs.y };
}
template <typename T, typename U, engine::type_traits::is_number<T> = true, typename S = typename std::common_type<T, U>::type>
Vector2<S> operator/(T lhs, const Vector2<U>& rhs) {
    Vector2<S> vector;
    if (rhs.x) {
        vector.x = lhs / rhs.x;
    } else {
        vector.x = std::numeric_limits<S>::infinity();
    }
    if (rhs.y) {
        vector.y = lhs / rhs.y;
    } else {
        vector.y = std::numeric_limits<S>::infinity();
    }
    return vector;
}

template <typename T, typename U, engine::type_traits::is_number<U> = true>
Vector2<typename std::common_type<T, U>::type> operator+(const Vector2<T>& lhs, U rhs) {
    return { lhs.x + rhs, lhs.y + rhs };
}
template <typename T, typename U, engine::type_traits::is_number<U> = true>
Vector2<typename std::common_type<T, U>::type> operator-(const Vector2<T>& lhs, U rhs) {
    return { lhs.x - rhs, lhs.y - rhs };
}
template <typename T, typename U, engine::type_traits::is_number<U> = true>
Vector2<typename std::common_type<T, U>::type> operator*(const Vector2<T>& lhs, U rhs) {
    return { lhs.x * rhs, lhs.y * rhs };
}
template <typename T, typename U, engine::type_traits::is_number<U> = true, typename S = typename std::common_type<T, U>::type>
Vector2<S> operator/(const Vector2<T>& lhs, U rhs) {
    Vector2<S> vector;
    if (rhs) {
        vector.x = lhs.x / rhs;
        vector.y = lhs.y / rhs;
    } else {
        vector.x = std::numeric_limits<S>::infinity();
        vector.y = std::numeric_limits<S>::infinity();
    }
    return vector;
}

namespace engine {

// Special functions.

// Cross product function between a number and a vector.
template <typename T, typename U, 
    typename S = typename std::common_type<T, U>::type>
inline Vector2<S> CrossProduct(U value, const Vector2<T>& vector) {
    return { 
        -static_cast<S>(value) * static_cast<S>(vector.y), 
        static_cast<S>(value) * static_cast<S>(vector.x) 
    };
}

// Return the distance squared between two vectors.
template <typename T, typename U, 
    typename S = typename std::common_type<T, U>::type>
inline S DistanceSquared(const Vector2<T>& lhs, const Vector2<U>& rhs) {
    S x = lhs.x - rhs.x;
    S y = lhs.y - rhs.y;
    return x * x + y * y;
}
// Return the distance between two vectors.
template <typename T, typename U, 
    typename S = typename std::common_type<T, U>::type>
inline S Distance(const Vector2<T>& lhs, const Vector2<U>& rhs) {
    return engine::math::Sqrt<S>(DistanceSquared(lhs, rhs));
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

// Return the absolute value of both vectors components.
template <typename T>
inline Vector2<T> Abs(const Vector2<T>& vector) {
    return {
        engine::math::Abs(vector.x),
        engine::math::Abs(vector.y)
    };
}
// Return both vector components rounded to the closest integer.
template <typename T = int, typename S>
inline Vector2<T> Round(const Vector2<S>& vector) {
    return {
        engine::math::Round<T>(vector.x),
        engine::math::Round<T>(vector.y)
    };
}
// Return both vector components ceiled to the closest integer.
template <typename T = int, typename S>
inline Vector2<T> Ceil(const Vector2<S>& vector) {
    return {
        engine::math::Ceil<T>(vector.x),
        engine::math::Ceil<T>(vector.y) 
    };
}
// Return both vector components floored to the closest integer.
template <typename T = int, typename S>
inline Vector2<T> Floor(const Vector2<S>& vector) {
    return { 
        engine::math::Floor<T>(vector.x), 
        engine::math::Floor<T>(vector.y) 
    };
}

// Clamp both vectors value within a vector range.
template <typename T>
inline Vector2<T> Clamp(const Vector2<T>& value, const Vector2<T>& low, const Vector2<T>& high) {
    return { 
        engine::math::Clamp(value.x, low.x, high.x), 
        engine::math::Clamp(value.y, low.y, high.y) 
    };
}

// Linearly interpolates both vector components by the given value.
template <typename T, typename U>
inline Vector2<U> Lerp(const Vector2<T>& a, const Vector2<T>& b, U amount) {
    return { 
        engine::math::Lerp(a.x, b.x, amount), 
        engine::math::Lerp(a.y, b.y, amount) 
    };
}

// Return both vector components smooth stepped.
template <typename T>
inline Vector2<T> SmoothStep(const Vector2<T>& vector) {
    return {
        engine::math::SmoothStep(vector.x),
        engine::math::SmoothStep(vector.y) 
    };
}

} // namespace engine