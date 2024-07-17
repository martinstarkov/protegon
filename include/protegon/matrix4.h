#pragma once

#include <ostream>
#include <array>
#include <iomanip>

#include "utility/type_traits.h"
#include "vector2.h"
#include "vector3.h"
#include "math.h"

namespace ptgn {

template <typename T, type_traits::arithmetic<T> = true>
struct Matrix4 {
	constexpr static V2_int size{ 4, 4 };
	constexpr static std::size_t length{ size.x * size.y };
	
	std::array<T, length> m{};

	constexpr Matrix4() = default;
	~Matrix4()			= default;

	constexpr Matrix4(T x, T y, T z, T w) {
		m[0] = x;
		m[5] = y;
		m[10] = z;
		m[15] = w;
	}

	constexpr Matrix4(T diag) {
		for (std::size_t x = 0; x < size.x; x++) {
			m[x + x * size.x] = diag;
		}
	}

	T& operator()(std::size_t x, std::size_t y) {
		PTGN_ASSERT(x < size.x);
		PTGN_ASSERT(y < size.y);
		return m[x + y * size.x];
	}

	const T& operator()(std::size_t x, std::size_t y) const {
		PTGN_ASSERT(x < size.x);
		PTGN_ASSERT(y < size.y);
		return m[x + y * size.x];
	}

	T& operator[](std::size_t col_major_index) {
		PTGN_ASSERT(col_major_index < length);
		return m[col_major_index];
	}

	const T& operator[](std::size_t col_major_index) const {
		PTGN_ASSERT(col_major_index < length);
		return m[col_major_index];
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
		Vector3<T> dir = (target - position).Normalized();
		const Vector3<T> right = (dir.Cross(up)).Normalized();
		const Vector3<T> up_n = right.Cross(dir);

		Matrix4<T> result{ T{ 1 } };
		result[0] = right.x;
		result[1]  = up_n.x;
		result[2] = -dir.x;
		result[4] = right.y;
		result[5]  = up_n.y;
		result[6] = -dir.y;
		result[8] = right.z;
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

		o[0]  = T{ 2 } / (right - left);
		o[5]  = T{ 2 } / (top - bottom);
		o[10] = -T{ 2 } / (far - near); // -1 by default
		o[12] = -(right + left) / (right - left);
		o[13] = -(top + bottom) / (top - bottom);
		o[14] = -(far + near) / (far - near); // 0 by default
		o[15] = T{ 1 };
		return o;
	}

	// fov_x in radians
	// Example usage: M4_float proj = M4_float::Perspective(DegToRad(45.0f), (float)game.window.GetSize().x / (float)game.window.GetSize().y, 0.1f, 100.0f);
	[[nodiscard]] static Matrix4 Perspective(T fov_x, T aspect_ratio, T front, T back
	) {
		T tangent = std::tan(fov_x / T{ 2 }); // tangent of half fovX
		T right	  = front * tangent;		 // half width of near plane
		T top	  = right / aspect_ratio;	 // half height of near plane

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

	[[nodiscard]] static Matrix4 Translate(const Matrix4& m, T x, T y, T z) {
		Matrix4<T> result{ m };
		for (std::size_t i = 0; i < result.size.x; i++) {
			result(i, 3) = m(i, 0) * x + m(i, 1) * y + m(i, 2) * z + m(i, 3);
		}
		return result;
	}

	[[nodiscard]] static Matrix4 Rotate(const Matrix4& m, T angle, T x_axis, T y_axis, T z_axis) {
		const T a = angle;
		const T c = std::cos(a);
		const T s = std::sin(a);

		// m = Dot(axis, axis)
		T magnitude{ x_axis * x_axis + y_axis * y_axis + z_axis * z_axis };

		T axis[3] = { T{ 0 }, T{ 0 }, T{ 0 } };

		if (!NearlyEqual(magnitude, T{ 0 })) {
			// axis = Normalize(axis);
			T m_sqrt = std::sqrt(magnitude);
			axis[0]	 = x_axis / m_sqrt;
			axis[1]	 = y_axis / m_sqrt;
			axis[2]	 = z_axis / m_sqrt;
		}

		T d = T{ 1 } - c;

		T temp[3] = { d * axis[0], d * axis[1], d * axis[2] };

		Matrix4<T> rotate;

		rotate(0, 0) = c + temp[0] * axis[0];
		rotate(0, 1) = temp[0] * axis[1] + s * axis[2];
		rotate(0, 2) = temp[0] * axis[2] - s * axis[1];

		rotate(1, 0) = temp[1] * axis[0] - s * axis[2];
		rotate(1, 1) = c + temp[1] * axis[1];
		rotate(1, 2) = temp[1] * axis[2] + s * axis[0];

		rotate(2, 0) = temp[2] * axis[0] + s * axis[1];
		rotate(2, 1) = temp[2] * axis[1] - s * axis[0];
		rotate(2, 2) = c + temp[2] * axis[2];

		Matrix4<T> result;

		for (std::size_t i = 0; i < result.size.x; i++) {
			result(i, 0) = m(i, 0) * rotate(0, 0) + m(i, 1) * rotate(0, 1) + m(i, 2) * rotate(0, 2);
			result(i, 1) = m(i, 0) * rotate(1, 0) + m(i, 1) * rotate(1, 1) + m(i, 2) * rotate(1, 2);
			result(i, 2) = m(i, 0) * rotate(2, 0) + m(i, 1) * rotate(2, 1) + m(i, 2) * rotate(2, 2);
			result(i, 3) = m(i, 3);
		}
		return result;
	}

	[[nodiscard]] static Matrix4 Scale(const Matrix4& m, T x, T y, T z) {
		Matrix4<T> result;
		for (std::size_t i = 0; i < result.size.x; i++) {
			result(i, 0) = m(i, 0) * x;
			result(i, 1) = m(i, 1) * y;
			result(i, 2) = m(i, 2) * z;
			result(i, 3) = m(i, 3);
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

} // namespace ptgn

template <typename T, ptgn::type_traits::stream_writable<std::ostream, T> = true>
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
			//os << ",";
			os << "\n";
		}
	}
	os << "]";
	return os;
}