#pragma once

#include <ostream>
#include <array>

#include "type_traits.h"

namespace ptgn {

template <typename T, type_traits::arithmetic<T> = true>
struct Matrix4 {
	constexpr static V2_int size{ 4, 4 };
	constexpr static std::size_t length{ size.x * size.y };

	std::array<T, length> m{};

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

	constexpr Matrix4() = default;
	~Matrix4()			= default;

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

	[[nodiscard]] static Matrix4 Identity() {
		Matrix4 identity;
		for (std::size_t x = 0; x < size.x; x++) {
			identity[x + x * size.x] = static_cast<T>(1);
		}
		return identity;
	}

	[[nodiscard]] bool IsZero() const {
		for (std::size_t i = 0; i < length; i++) {
			if (!NearlyEqual(m[i], static_cast<T>(0))) {
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
	os << "[";
	for (std::size_t x = 0; x < m.size.x; x++) {
		bool last_row = x == m.size.x - 1;
		// Extra space before [ for row alignment purposes.
		if (x != 0) {
			os << " ";
		}
		os << "[";
		for (std::size_t y = 0; y < m.size.y; y++) {
			bool last_col = y == m.size.y - 1;
			const T& val  = m[x + y * m.size.x];
			// Extra space for negative sign alignment purposes.
			if (val >= 0) {
				os << " ";
			}
			os << val;
			if (!last_col) {
				os << ", ";
			}
		}
		os << "]";
		if (!last_row) {
			os << ",\n";
		}
	}
	os << "]";
	return os;
}