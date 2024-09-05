#pragma once

#include <array>
#include <functional>
#include <iomanip>
#include <ostream>

#include "protegon/math.h"
#include "protegon/vector2.h"
#include "protegon/vector3.h"
#include "protegon/vector4.h"
#include "utility/type_traits.h"

namespace ptgn {

class Quaternion;

template <typename T, tt::arithmetic<T> = true>
struct Matrix4 {
public:
	constexpr static V2_int size{ 4, 4 };
	constexpr static std::size_t length{ size.x * size.y };

private:
	friend class Quaternion;

	std::array<T, length> m{};

public:
	constexpr Matrix4() = default;
	~Matrix4()			= default;

	constexpr Matrix4(T x, T y, T z, T w) {
		m[0]  = x;
		m[5]  = y;
		m[10] = z;
		m[15] = w;
	}

	constexpr Matrix4(const std::array<T, length>& m) : m{ m } {}

	template <typename... Ts>
	constexpr Matrix4(Ts... args) : m{ args... } {}

	constexpr Matrix4(
		const Vector4<T>& row0, const Vector4<T>& row1, const Vector4<T>& row2,
		const Vector4<T>& row3
	) {
		m[0]  = row0.x;
		m[1]  = row1.x;
		m[2]  = row2.x;
		m[3]  = row3.x;
		m[4]  = row0.y;
		m[5]  = row1.y;
		m[6]  = row2.y;
		m[7]  = row3.y;
		m[8]  = row0.z;
		m[9]  = row1.z;
		m[10] = row2.z;
		m[11] = row3.z;
		m[12] = row0.w;
		m[13] = row1.w;
		m[14] = row2.w;
		m[15] = row3.w;
	}

	constexpr Matrix4(T diag) {
		for (std::size_t x = 0; x < size.x; x++) {
			m[x + x * size.x] = diag;
		}
	}

	constexpr T& operator()(std::size_t x, std::size_t y) {
		PTGN_ASSERT(x < size.x);
		PTGN_ASSERT(y < size.y);
		return m[x + y * size.x];
	}

	constexpr const T& operator()(std::size_t x, std::size_t y) const {
		PTGN_ASSERT(x < size.x);
		PTGN_ASSERT(y < size.y);
		return m[x + y * size.x];
	}

	constexpr T& operator[](std::size_t col_major_index) {
		PTGN_ASSERT(col_major_index < length);
		return m[col_major_index];
	}

	constexpr const T& operator[](std::size_t col_major_index) const {
		PTGN_ASSERT(col_major_index < length);
		return m[col_major_index];
	}

	constexpr T* Data() noexcept {
		return m.data();
	}

	constexpr const T* Data() const noexcept {
		return m.data();
	}

	auto begin() {
		return std::begin(m);
	}

	auto end() {
		return std::end(m);
	}

	auto begin() const {
		return std::begin(m);
	}

	auto end() const {
		return std::end(m);
	}

	[[nodiscard]] static Matrix4 LookAt(
		const Vector3<T>& position, const Vector3<T>& target, const Vector3<T>& up
	) {
		static_assert(std::is_floating_point_v<T>, "Function requires floating point type");
		Vector3<T> dir	 = (target - position).Normalized();
		Vector3<T> right = (dir.Cross(up)).Normalized();
		Vector3<T> up_n	 = right.Cross(dir);

		Matrix4<T> result{ T{ 1 } };
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
		return { T{ 1 } };
	}

	// Example usage: M4_float proj = M4_float::Orthographic(-1.0f, 1.0f, -1.0f, 1.0f);
	[[nodiscard]] static Matrix4 Orthographic(
		T left, T right, T bottom, T top, T near = T{ -1 }, T far = T{ 1 }
	) {
		Matrix4<T> o;

		PTGN_ASSERT(right != left, "Orthographic matrix division by zero");
		PTGN_ASSERT(bottom != top, "Orthographic matrix division by zero");
		PTGN_ASSERT(far != near, "Orthographic matrix division by zero");

		o[0]  = T{ 2 } / (right - left);
		o[5]  = T{ 2 } / (top - bottom);
		o[10] = -T{ 2 } / (far - near); // -1 by default
		o[12] = -(right + left) / (right - left);
		o[13] = -(top + bottom) / (top - bottom);
		o[14] = -(far + near) / (far - near); // 0 by default
		o[15] = T{ 1 };

		PTGN_ASSERT(
			std::invoke([&]() -> bool {
				for (std::size_t i = 0; i < o.length; i++) {
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
		T Coef00 = m[10] * m[15] - m[14] * m[11];
		T Coef02 = m[6] * m[15] - m[14] * m[7];
		T Coef03 = m[6] * m[11] - m[10] * m[7];

		T Coef04 = m[9] * m[15] - m[13] * m[11];
		T Coef06 = m[5] * m[15] - m[13] * m[7];
		T Coef07 = m[5] * m[11] - m[9] * m[7];

		T Coef08 = m[9] * m[14] - m[13] * m[10];
		T Coef10 = m[5] * m[14] - m[13] * m[6];
		T Coef11 = m[5] * m[10] - m[9] * m[6];

		T Coef12 = m[8] * m[15] - m[12] * m[11];
		T Coef14 = m[4] * m[15] - m[12] * m[7];
		T Coef15 = m[4] * m[11] - m[8] * m[7];

		T Coef16 = m[8] * m[14] - m[12] * m[10];
		T Coef18 = m[4] * m[14] - m[12] * m[6];
		T Coef19 = m[4] * m[10] - m[8] * m[6];

		T Coef20 = m[8] * m[13] - m[12] * m[9];
		T Coef22 = m[4] * m[13] - m[12] * m[5];
		T Coef23 = m[4] * m[9] - m[8] * m[5];

		Vector4<T> Fac0(Coef00, Coef00, Coef02, Coef03);
		Vector4<T> Fac1(Coef04, Coef04, Coef06, Coef07);
		Vector4<T> Fac2(Coef08, Coef08, Coef10, Coef11);
		Vector4<T> Fac3(Coef12, Coef12, Coef14, Coef15);
		Vector4<T> Fac4(Coef16, Coef16, Coef18, Coef19);
		Vector4<T> Fac5(Coef20, Coef20, Coef22, Coef23);

		Vector4<T> Vec0(m[4], m[0], m[0], m[0]);
		Vector4<T> Vec1(m[5], m[1], m[1], m[1]);
		Vector4<T> Vec2(m[6], m[2], m[2], m[2]);
		Vector4<T> Vec3(m[7], m[3], m[3], m[3]);

		Vector4<T> Inv0(Vec1 * Fac0 - Vec2 * Fac1 + Vec3 * Fac2);
		Vector4<T> Inv1(Vec0 * Fac0 - Vec2 * Fac3 + Vec3 * Fac4);
		Vector4<T> Inv2(Vec0 * Fac1 - Vec1 * Fac3 + Vec3 * Fac5);
		Vector4<T> Inv3(Vec0 * Fac2 - Vec1 * Fac4 + Vec2 * Fac5);

		Vector4<T> SignA(+1, -1, +1, -1);
		Vector4<T> SignB(-1, +1, -1, +1);
		Matrix4<T> Inverse(Inv0 * SignA, Inv1 * SignB, Inv2 * SignA, Inv3 * SignB);

		Vector4<T> Row0(Inverse[0], Inverse[4], Inverse[8], Inverse[12]);

		Vector4<T> Dot0(m[0] * Row0);
		T Dot1 = (Dot0.x + Dot0.y) + (Dot0.z + Dot0.w);

		T OneOverDeterminant = T{ 1 } / Dot1;

		return Inverse * OneOverDeterminant;
	}

	// Field of view angle fov_x in radians.
	// Example usage: M4_float proj = M4_float::Perspective(DegToRad(45.0f),
	// (float)game.window.GetSize().x / (float)game.window.GetSize().y, 0.1f, 100.0f);
	[[nodiscard]] static Matrix4 Perspective(T fov_x_radians, T aspect_ratio, T front, T back) {
		static_assert(std::is_floating_point_v<T>, "Function requires floating point type");
		T tangent = std::tan(fov_x_radians / T{ 2 }); // tangent of half fovX
		T right	  = front * tangent;				  // half width of near plane
		T top	  = right / aspect_ratio;			  // half height of near plane

		// params: left, right, bottom, top, near(front), far(back)
		Matrix4<T> p;
		p[0]  = front / right;
		p[5]  = front / top;
		p[10] = -(back + front) / (back - front);
		p[11] = T{ -1 };
		p[14] = -(T{ 2 } * back * front) / (back - front);
		p[15] = T{ 0 };
		return p;
	}

	[[nodiscard]] static Matrix4 Translate(const Matrix4& m, const Vector3<T>& axes) {
		Matrix4<T> result{ m };
		for (std::size_t i = 0; i < result.size.x; i++) {
			result[i + 12] = m[i] * axes.x + m[i + 4] * axes.y + m[i + 8] * axes.z + m[i + 12];
		}
		return result;
	}

	// Angle in radians.
	[[nodiscard]] static Matrix4 Rotate(
		const Matrix4& matrix, T angle_radians, const Vector3<T>& axes
	) {
		static_assert(std::is_floating_point_v<T>, "Function requires floating point type");
		const T a = angle_radians;
		const T c = std::cos(a);
		const T s = std::sin(a);

		T magnitude{ axes.Dot(axes) };

		Vector3<T> axis;

		if (!NearlyEqual(magnitude, T{ 0 })) {
			// axis = Normalize(axis);
			axis = axes.Normalized();
		}

		T d = T{ 1 } - c;

		Vector3<T> temp{ d * axis.x, d * axis.y, d * axis.z };

		Matrix4<T> rotate;

		rotate[0] = c + temp.x * axis.x;
		rotate[1] = temp.y * axis.x - s * axis.z;
		rotate[2] = temp.z * axis.x + s * axis.y;

		rotate[4] = temp.x * axis.y + s * axis.z;
		rotate[5] = c + temp.y * axis.y;
		rotate[6] = temp.z * axis.y - s * axis.x;

		rotate[8]  = temp.x * axis.z - s * axis.y;
		rotate[9]  = temp.y * axis.z + s * axis.x;
		rotate[10] = c + temp.z * axis.z;

		Matrix4<T> result;

		for (std::size_t i = 0; i < result.size.x; i++) {
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

	[[nodiscard]] static Matrix4 Scale(const Matrix4& m, const Vector3<T>& axes) {
		Matrix4<T> result;
		for (std::size_t i = 0; i < result.size.x; i++) {
			result[i + 0]  = m[i + 0] * axes.x;
			result[i + 4]  = m[i + 4] * axes.y;
			result[i + 8]  = m[i + 8] * axes.z;
			result[i + 12] = m[i + 12];
		}
		return result;
	}

	[[nodiscard]] bool IsZero() const {
		for (std::size_t i = 0; i < length; i++) {
			if (!NearlyEqual(m[i], T{ 0 })) {
				return false;
			}
		}
		return true;
	}
};

using M4_int	= Matrix4<int>;
using M4_uint	= Matrix4<unsigned int>;
using M4_float	= Matrix4<float>;
using M4_double = Matrix4<double>;

template <typename T, typename U, typename S = typename std::common_type_t<T, U>>
inline Matrix4<S> operator+(const Matrix4<T>& lhs, const Matrix4<U>& rhs) {
	Matrix4<S> result;
	for (std::size_t i = 0; i < result.length; i++) {
		result[i] = lhs[i] + rhs[i];
	}
	return result;
}

template <typename T, typename U, typename S = typename std::common_type_t<T, U>>
inline Matrix4<S> operator-(const Matrix4<T>& lhs, const Matrix4<U>& rhs) {
	Matrix4<S> result;
	for (std::size_t i = 0; i < result.length; i++) {
		result[i] = lhs[i] - rhs[i];
	}
	return result;
}

template <typename T, typename U, typename S = typename std::common_type_t<T, U>>
inline Matrix4<S> operator*(const Matrix4<T>& A, const Matrix4<U>& B) {
	Matrix4<S> res;

	for (std::size_t col = 0; col < B.size.y; ++col) {
		std::size_t res_stride{ col * res.size.x };
		std::size_t B_stride{ col * B.size.x };
		for (std::size_t row = 0; row < A.size.x; ++row) {
			std::size_t res_index{ row + res_stride };
			for (std::size_t i = 0; i < B.size.x; ++i) {
				res[res_index] += A[row + i * A.size.x] * B[i + B_stride];
			}
		}
	}
	return res;
}

template <typename T, typename U, typename S = typename std::common_type_t<T, U>>
inline Vector4<S> operator*(const Matrix4<T>& A, const Vector4<U>& B) {
	Vector4<S> res;

	for (std::size_t row = 0; row < A.size.x; ++row) {
		for (std::size_t i = 0; i < 4; ++i) {
			res[row] += A[row + i * A.size.x] * B[i];
		}
	}
	return res;
}

template <typename T, typename U, typename S = typename std::common_type_t<T, U>>
inline Matrix4<S> operator*(const Matrix4<T>& A, U B) {
	Matrix4<S> res;

	for (std::size_t i = 0; i < A.length; ++i) {
		res[i] = A[i] * B;
	}
	return res;
}

template <typename T, typename U, typename S = typename std::common_type_t<T, U>>
inline Matrix4<S> operator*(U A, const Matrix4<T>& B) {
	return B * A;
}

template <typename T, typename U, typename S = typename std::common_type_t<T, U>>
inline Matrix4<S> operator/(const Matrix4<T>& A, U B) {
	Matrix4<S> res;

	for (std::size_t i = 0; i < res.length; ++i) {
		res[i] = static_cast<S>(A[i]) / static_cast<S>(B);
	}
	return res;
}

template <typename T>
inline bool operator==(const Matrix4<T>& lhs, const Matrix4<T>& rhs) {
	for (std::size_t i = 0; i < lhs.length; i++) {
		if (!NearlyEqual(lhs[i], rhs[i])) {
			return false;
		}
	}
	return true;
}

template <typename T>
inline bool operator!=(const Matrix4<T>& lhs, const Matrix4<T>& rhs) {
	return !operator==(lhs, rhs);
}

template <typename T, ptgn::tt::stream_writable<std::ostream, T> = true>
inline std::ostream& operator<<(std::ostream& os, const ptgn::Matrix4<T>& m) {
	os << "\n";
	os << std::fixed << std::right << std::setprecision(static_cast<std::streamsize>(3))
	   << std::setfill(' ') << "[";
	for (std::size_t i = 0; i < m.size.x; ++i) {
		if (i != 0) {
			os << " ";
		}
		os << "[";
		for (std::size_t j = 0; j < m.size.y; ++j) {
			os << std::setw(9);
			os << m(i, j);
			if (j != m.size.y - 1) {
				os << ",";
			}
		}
		os << "]";
		if (i != m.size.x - 1) {
			// os << ",";
			os << "\n";
		}
	}
	os << "]";
	return os;
}

} // namespace ptgn