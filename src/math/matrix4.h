#pragma once

#include <array>
#include <cmath>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <limits>
#include <ostream>
#include <type_traits>

#include "math/math.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "utility/debug.h"
#include "utility/type_traits.h"

namespace ptgn {

class Quaternion;

struct Matrix4 {
public:
	constexpr static V2_int size{ 4, 4 };
	constexpr static std::size_t length{ size.x * size.y };

private:
	friend class Quaternion;

	std::array<float, length> m_{};

public:
	constexpr Matrix4() = default;
	~Matrix4()			= default;

	constexpr Matrix4(float x, float y, float z, float w) {
		m_[0]  = x;
		m_[5]  = y;
		m_[10] = z;
		m_[15] = w;
	}

	explicit constexpr Matrix4(const std::array<float, length>& m) : m_{ m } {}

	template <typename... Ts>
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

	[[nodiscard]] static Matrix4 LookAt(
		const Vector3<float>& position, const Vector3<float>& target, const Vector3<float>& up
	) {
		static_assert(std::is_floating_point_v<float>, "Function requires floating point type");
		Vector3<float> dir	 = (target - position).Normalized();
		Vector3<float> right = (dir.Cross(up)).Normalized();
		Vector3<float> up_n	 = right.Cross(dir);

		Matrix4 result{ 1.0f };
		result[0]  = right.x;
		result[1]  = up_n.x;
		result[2]  = -dir.x;
		result[4]  = right.y;
		result[5]  = up_n.y;
		result[6]  = -dir.y;
		result[8]  = right.z;
		result[9]  = up_n.z;
		result[10] = -dir.z;
		result[12] = -right.Dot(position);
		result[13] = -up_n.Dot(position);
		result[14] = dir.Dot(position);

		return result;
	}

	[[nodiscard]] static Matrix4 Identity() {
		return Matrix4{ 1.0f };
	}

	// Example usage: Matrix4 proj = Matrix4::Orthographic(-1.0f, 1.0f, -1.0f, 1.0f);
	[[nodiscard]] static Matrix4 Orthographic(
		float left, float right, float bottom, float top, float near = -1.0f, float far = 1.0f
	) {
		Matrix4 o;

		PTGN_ASSERT(right != left, "Orthographic matrix division by zero");
		PTGN_ASSERT(bottom != top, "Orthographic matrix division by zero");
		PTGN_ASSERT(far != near, "Orthographic matrix division by zero");

		float plane_dist{ far - near };

		o[0]  = 2.0f / (right - left);
		o[5]  = 2.0f / (top - bottom);
		o[10] = -2.0f / plane_dist; // -1 by default
		o[12] = -(right + left) / (right - left);
		o[13] = -(top + bottom) / (top - bottom);
		float plane_sum{ far + near };
		if (std::isnan(plane_sum)) {
			plane_sum = 0.0f;
		}
		o[14] = -plane_sum / plane_dist; // 0 by default
		o[15] = 1.0f;

		PTGN_ASSERT(
			std::invoke([&]() -> bool {
				for (std::size_t i{ 0 }; i < o.length; i++) {
					if (std::isnan(o[i]) || std::isinf(o[i])) {
						return false;
					}
				}
				return true;
			}),
			"Failed to create valid orthographic matrix"
		);

		return o;
	}

	// From:
	// https://github.com/g-truc/glm/blob/33b4a621a697a305bc3a7610d290677b96beb181/glm/detail/func_matrix.inl#L388

	[[nodiscard]] Matrix4 Inverse() const {
		float Coef00{ m_[10] * m_[15] - m_[14] * m_[11] };
		float Coef02{ m_[6] * m_[15] - m_[14] * m_[7] };
		float Coef03{ m_[6] * m_[11] - m_[10] * m_[7] };
		float Coef04{ m_[9] * m_[15] - m_[13] * m_[11] };
		float Coef06{ m_[5] * m_[15] - m_[13] * m_[7] };
		float Coef07{ m_[5] * m_[11] - m_[9] * m_[7] };
		float Coef08{ m_[9] * m_[14] - m_[13] * m_[10] };
		float Coef10{ m_[5] * m_[14] - m_[13] * m_[6] };
		float Coef11{ m_[5] * m_[10] - m_[9] * m_[6] };
		float Coef12{ m_[8] * m_[15] - m_[12] * m_[11] };
		float Coef14{ m_[4] * m_[15] - m_[12] * m_[7] };
		float Coef15{ m_[4] * m_[11] - m_[8] * m_[7] };
		float Coef16{ m_[8] * m_[14] - m_[12] * m_[10] };
		float Coef18{ m_[4] * m_[14] - m_[12] * m_[6] };
		float Coef19{ m_[4] * m_[10] - m_[8] * m_[6] };
		float Coef20{ m_[8] * m_[13] - m_[12] * m_[9] };
		float Coef22{ m_[4] * m_[13] - m_[12] * m_[5] };
		float Coef23{ m_[4] * m_[9] - m_[8] * m_[5] };

		Vector4<float> Fac0(Coef00, Coef00, Coef02, Coef03);
		Vector4<float> Fac1(Coef04, Coef04, Coef06, Coef07);
		Vector4<float> Fac2(Coef08, Coef08, Coef10, Coef11);
		Vector4<float> Fac3(Coef12, Coef12, Coef14, Coef15);
		Vector4<float> Fac4(Coef16, Coef16, Coef18, Coef19);
		Vector4<float> Fac5(Coef20, Coef20, Coef22, Coef23);
		Vector4<float> Vec0(m_[4], m_[0], m_[0], m_[0]);
		Vector4<float> Vec1(m_[5], m_[1], m_[1], m_[1]);
		Vector4<float> Vec2(m_[6], m_[2], m_[2], m_[2]);
		Vector4<float> Vec3(m_[7], m_[3], m_[3], m_[3]);
		Vector4<float> Inv0(Vec1 * Fac0 - Vec2 * Fac1 + Vec3 * Fac2);
		Vector4<float> Inv1(Vec0 * Fac0 - Vec2 * Fac3 + Vec3 * Fac4);
		Vector4<float> Inv2(Vec0 * Fac1 - Vec1 * Fac3 + Vec3 * Fac5);
		Vector4<float> Inv3(Vec0 * Fac2 - Vec1 * Fac4 + Vec2 * Fac5);
		Vector4<float> SignA(+1, -1, +1, -1);
		Vector4<float> SignB(-1, +1, -1, +1);
		Matrix4 Inverse(Inv0 * SignA, Inv1 * SignB, Inv2 * SignA, Inv3 * SignB);
		Vector4<float> Row0(Inverse[0], Inverse[4], Inverse[8], Inverse[12]);
		Vector4<float> Dot0(m_[0] * Row0);
		float Dot1 = (Dot0.x + Dot0.y) + (Dot0.z + Dot0.w);

		float OneOverDeterminant{ 1.0f / Dot1 };

		Matrix4 result{ Inverse * OneOverDeterminant };

		return result;
	}

	// Field of view angle fov_x in radians.
	// Example usage: Matrix4 proj = Matrix4::Perspective(DegToRad(45.0f),
	// (float)game.window.GetSize().x / (float)game.window.GetSize().y, 0.1f, 100.0f);
	[[nodiscard]] static Matrix4 Perspective(
		float fov_x_radians, float aspect_ratio, float front, float back
	) {
		static_assert(std::is_floating_point_v<float>, "Function requires floating point type");
		float tangent = std::tan(fov_x_radians / 2.0f); // tangent of half fovX
		float right	  = front * tangent;				// half width of near plane
		float top	  = right / aspect_ratio;			// half height of near plane

		Matrix4 p;
		p[0]  = front / right;
		p[5]  = front / top;
		p[10] = -(back + front) / (back - front);
		p[11] = -1.0f;
		p[14] = -(2.0f * back * front) / (back - front);
		p[15] = 0.0f;
		return p;
	}

	[[nodiscard]] static Matrix4 Translate(const Matrix4& m, const Vector3<float>& axes) {
		Matrix4 result{ m };
		for (std::size_t i{ 0 }; i < result.size.x; i++) {
			result[i + 12] = m[i] * axes.x + m[i + 4] * axes.y + m[i + 8] * axes.z + m[i + 12];
		}
		return result;
	}

	// Angle in radians.
	[[nodiscard]] static Matrix4 Rotate(
		const Matrix4& matrix, float angle_radians, const Vector3<float>& axes
	) {
		static_assert(std::is_floating_point_v<float>, "Function requires floating point type");
		const float a = angle_radians;
		const float c = std::cos(a);
		const float s = std::sin(a);

		float magnitude{ axes.Dot(axes) };

		Vector3<float> axis;

		if (!NearlyEqual(magnitude, 0.0f)) {
			axis = axes.Normalized();
		}

		float d = 1.0f - c;

		Vector3<float> temp{ d * axis.x, d * axis.y, d * axis.z };

		Matrix4 rotate;

		rotate[0] = c + temp.x * axis.x;
		rotate[1] = temp.y * axis.x - s * axis.z;
		rotate[2] = temp.z * axis.x + s * axis.y;

		rotate[4] = temp.x * axis.y + s * axis.z;
		rotate[5] = c + temp.y * axis.y;
		rotate[6] = temp.z * axis.y - s * axis.x;

		rotate[8]  = temp.x * axis.z - s * axis.y;
		rotate[9]  = temp.y * axis.z + s * axis.x;
		rotate[10] = c + temp.z * axis.z;

		Matrix4 result;

		for (std::size_t i{ 0 }; i < result.size.x; i++) {
			result[i + 0] =
				matrix[i + 0] * rotate[0] + matrix[i + 4] * rotate[4] + matrix[i + 8] * rotate[8];
			result[i + 4] =
				matrix[i + 0] * rotate[1] + matrix[i + 4] * rotate[5] + matrix[i + 8] * rotate[5];
			result[i + 8] =
				matrix[i + 0] * rotate[2] + matrix[i + 4] * rotate[6] + matrix[i + 8] * rotate[10];
			result[i + 12] = matrix[i + 12];
		}
		return result;
	}

	[[nodiscard]] static Matrix4 Scale(const Matrix4& m, const Vector3<float>& axes) {
		Matrix4 result;
		for (std::size_t i{ 0 }; i < result.size.x; i++) {
			result[i + 0]  = m[i + 0] * axes.x;
			result[i + 4]  = m[i + 4] * axes.y;
			result[i + 8]  = m[i + 8] * axes.z;
			result[i + 12] = m[i + 12];
		}
		return result;
	}

	[[nodiscard]] bool IsZero() const {
		for (std::size_t i{ 0 }; i < length; i++) {
			if (!NearlyEqual(m_[i], 0.0f)) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] bool ExactlyEquals(const Matrix4& o) const {
		for (std::size_t i{ 0 }; i < length; i++) {
			if (m_[i] != o[i]) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] inline bool operator==(const Matrix4& o) const {
		for (std::size_t i{ 0 }; i < length; i++) {
			if (!NearlyEqual(m_[i], o.m_[i])) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] inline bool operator!=(const Matrix4& o) const {
		return !operator==(o);
	}

	[[nodiscard]] inline Matrix4 operator+(const Matrix4& rhs) {
		Matrix4 result;
		for (std::size_t i{ 0 }; i < result.length; i++) {
			result[i] = m_[i] + rhs[i];
		}
		return result;
	}

	[[nodiscard]] inline Matrix4 operator-(const Matrix4& rhs) {
		Matrix4 result;
		for (std::size_t i{ 0 }; i < result.length; i++) {
			result[i] = m_[i] - rhs[i];
		}
		return result;
	}

	[[nodiscard]] inline Matrix4 operator*(const Matrix4& rhs) {
		Matrix4 res;

		for (std::size_t col = 0; col < rhs.size.y; ++col) {
			std::size_t res_stride{ col * res.size.x };
			std::size_t B_stride{ col * rhs.size.x };
			for (std::size_t row = 0; row < size.x; ++row) {
				std::size_t res_index{ row + res_stride };
				for (std::size_t i{ 0 }; i < rhs.size.x; ++i) {
					res[res_index] += m_[row + i * size.x] * rhs[i + B_stride];
				}
			}
		}
		return res;
	}

	template <typename U, tt::arithmetic<U> = true>
	[[nodiscard]] inline Vector4<float> operator*(const Vector4<U>& rhs) {
		Vector4<float> res;

		for (std::size_t row = 0; row < size.x; ++row) {
			for (std::size_t i{ 0 }; i < 4; ++i) {
				res[row] += m_[row + i * size.x] * rhs[i];
			}
		}
		return res;
	}

	template <typename U, tt::arithmetic<U> = true>
	[[nodiscard]] inline Matrix4 operator*(U rhs) {
		Matrix4 res;

		for (std::size_t i{ 0 }; i < res.length; ++i) {
			res[i] = m_[i] * rhs;
		}
		return res;
	}

	template <typename U, tt::arithmetic<U> = true>
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

template <typename U, tt::arithmetic<U> = true>
[[nodiscard]] inline Matrix4 operator*(U A, const Matrix4& B) {
	return B * A;
}

} // namespace ptgn