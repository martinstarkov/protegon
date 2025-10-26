#pragma once

#include <array>
#include <iomanip>
#include <ios>
#include <iterator>
#include <limits>
#include <ostream>
#include <type_traits>

#include "core/util/concepts.h"
#include "debug/runtime/assert.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "serialization/json/fwd.h"

namespace ptgn {

struct Transform;
class Quaternion;

struct Matrix4 {
public:
	constexpr static V2_size size{ 4, 4 };
	constexpr static std::size_t length{ size.x * size.y };

	friend void to_json(json& j, const Matrix4& m);

	friend void from_json(const json& j, Matrix4& m);

private:
	friend class Quaternion;

	/* Column major, indices as follows:
	 * [0,  4,  8, 12]
	 * [1,  5,  9, 13]
	 * [2,  6, 10, 14]
	 * [3,  7, 11, 15]
	 */
	std::array<float, length> m_{};

public:
	constexpr Matrix4() = default;

	constexpr Matrix4(float x, float y, float z, float w) {
		m_[0]  = x;
		m_[5]  = y;
		m_[10] = z;
		m_[15] = w;
	}

	explicit constexpr Matrix4(const std::array<float, length>& m) : m_{ m } {}

	template <Arithmetic... Ts>
	explicit constexpr Matrix4(Ts... args) : m_{ args... } {}

	constexpr Matrix4(
		const Vector4<float>& row0, const Vector4<float>& row1, const Vector4<float>& row2,
		const Vector4<float>& row3
	) {
		m_[0]  = row0.x;
		m_[1]  = row1.x;
		m_[2]  = row2.x;
		m_[3]  = row3.x;
		m_[4]  = row0.y;
		m_[5]  = row1.y;
		m_[6]  = row2.y;
		m_[7]  = row3.y;
		m_[8]  = row0.z;
		m_[9]  = row1.z;
		m_[10] = row2.z;
		m_[11] = row3.z;
		m_[12] = row0.w;
		m_[13] = row1.w;
		m_[14] = row2.w;
		m_[15] = row3.w;
	}

	explicit constexpr Matrix4(float diag) {
		for (std::size_t x{ 0 }; x < size.x; x++) {
			m_[x + x * size.x] = diag;
		}
	}

	[[nodiscard]] constexpr float& operator()(std::size_t x, std::size_t y) {
		PTGN_ASSERT(x < size.x);
		PTGN_ASSERT(y < size.y);
		return m_[x + y * size.x];
	}

	[[nodiscard]] constexpr const float& operator()(std::size_t x, std::size_t y) const {
		PTGN_ASSERT(x < size.x);
		PTGN_ASSERT(y < size.y);
		return m_[x + y * size.x];
	}

	[[nodiscard]] constexpr float& operator[](std::size_t col_major_index) {
		PTGN_ASSERT(col_major_index < length);
		return m_[col_major_index];
	}

	[[nodiscard]] constexpr const float& operator[](std::size_t col_major_index) const {
		PTGN_ASSERT(col_major_index < length);
		return m_[col_major_index];
	}

	[[nodiscard]] constexpr float* Data() noexcept {
		return m_.data();
	}

	[[nodiscard]] constexpr const float* Data() const noexcept {
		return m_.data();
	}

	[[nodiscard]] auto begin() {
		return std::begin(m_);
	}

	[[nodiscard]] auto end() {
		return std::end(m_);
	}

	[[nodiscard]] auto begin() const {
		return std::begin(m_);
	}

	[[nodiscard]] auto end() const {
		return std::end(m_);
	}

	void Scale(const Vector3<float>& axes) {
		*this = Scale(*this, axes);
	}

	void Rotate(float rotation_radians, const Vector3<float>& axes) {
		*this = Rotate(*this, rotation_radians, axes);
	}

	void Translate(const Vector3<float>& translation) {
		*this = Translate(*this, translation);
	}

	[[nodiscard]] Matrix4 Inverse() const;

	[[nodiscard]] static Matrix4 LookAt(
		const Vector3<float>& position, const Vector3<float>& target, const Vector3<float>& up
	);

	[[nodiscard]] static Matrix4 Identity();

	[[nodiscard]] static Matrix4 Orthographic(
		float left, float right, float bottom, float top,
		float near = -std::numeric_limits<float>::infinity(),
		float far  = std::numeric_limits<float>::infinity()
	);

	[[nodiscard]] static Matrix4 Orthographic(
		const V2_float& min, const V2_float& max,
		float near = -std::numeric_limits<float>::infinity(),
		float far  = std::numeric_limits<float>::infinity()
	);

	[[nodiscard]] static Matrix4 MakeTransform(
		const Vector3<float>& position, float rotation_radians, const Vector3<float>& rotation_axis,
		const Vector3<float>& scale
	);

	[[nodiscard]] static Matrix4 MakeTransform(
		const Vector2<float>& position, float rotation_radians, const Vector2<float>& scale
	);

	[[nodiscard]] static Matrix4 MakeTransform(const Transform& transform);

	[[nodiscard]] static Matrix4 MakeInverseTransform(
		const Vector3<float>& position, float rotation_radians, const Vector3<float>& rotation_axis,
		const Vector3<float>& scale
	);

	[[nodiscard]] static Matrix4 MakeInverseTransform(
		const Vector2<float>& position, float rotation_radians, const Vector2<float>& scale
	);

	[[nodiscard]] static Matrix4 MakeInverseTransform(const Transform& transform);

	// Field of view angle fov_x in radians.
	// Example usage: Matrix4 proj = Matrix4::Perspective(DegToRad(45.0f), width / height, 0.1f,
	// 100.0f);
	[[nodiscard]] static Matrix4 Perspective(
		float fov_x_radians, float aspect_ratio, float front, float back
	);

	[[nodiscard]] static Matrix4 Translate(const Matrix4& matrix, const Vector3<float>& axes);

	// Angle in radians.
	[[nodiscard]] static Matrix4 Rotate(
		const Matrix4& matrix, float rotation_radians, const Vector3<float>& axes
	);

	[[nodiscard]] static Matrix4 Scale(const Matrix4& matrix, const Vector3<float>& axes);

	[[nodiscard]] bool IsZero() const;

	[[nodiscard]] bool ExactlyEquals(const Matrix4& o) const;

	friend bool operator==(const Matrix4& a, const Matrix4& b) {
		for (std::size_t i{ 0 }; i < length; i++) {
			if (!NearlyEqual(a.m_[i], b.m_[i])) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] Matrix4 operator+(const Matrix4& rhs);

	[[nodiscard]] Matrix4 operator-(const Matrix4& rhs);

	[[nodiscard]] Matrix4 operator*(const Matrix4& rhs);

	template <Arithmetic U>
	[[nodiscard]] inline Vector4<float> operator*(const Vector4<U>& rhs) {
		Vector4<float> res;

		for (std::size_t row{ 0 }; row < size.x; ++row) {
			for (std::size_t i{ 0 }; i < size.y; ++i) {
				res[row] += m_[row + i * size.x] * rhs[i];
			}
		}
		return res;
	}

	template <Arithmetic U>
	[[nodiscard]] inline Matrix4 operator*(U rhs) {
		Matrix4 res;

		for (std::size_t i{ 0 }; i < res.length; ++i) {
			res[i] = m_[i] * rhs;
		}
		return res;
	}

	template <Arithmetic U>
	[[nodiscard]] inline Matrix4 operator/(U rhs) {
		Matrix4 res;

		for (std::size_t i{ 0 }; i < res.length; ++i) {
			res[i] = m_[i] / static_cast<float>(rhs);
		}
		return res;
	}
};

inline std::ostream& operator<<(std::ostream& os, const ptgn::Matrix4& m) {
	os << "\n";
	os << std::fixed << std::right << std::setprecision(static_cast<std::streamsize>(3))
	   << std::setfill(' ') << "[";
	for (std::size_t i{ 0 }; i < m.size.x; ++i) {
		if (i != 0) {
			os << " ";
		}
		os << "[";
		for (std::size_t j = 0; j < m.size.y; ++j) {
			os << std::setw(9);
			os << m(i, j);
			if (j != static_cast<std::size_t>(m.size.y) - 1) {
				os << ",";
			}
		}
		os << "]";
		if (i != static_cast<std::size_t>(m.size.x) - 1) {
			// os << ",";
			os << "\n";
		}
	}
	os << "]";
	return os;
}

template <Arithmetic U>
[[nodiscard]] inline Matrix4 operator*(U A, const Matrix4& B) {
	return B * A;
}

} // namespace ptgn