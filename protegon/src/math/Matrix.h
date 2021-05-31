#pragma once

// Some basic matrix implementations. Can be expanded upon in the future.

#include <cstdlib> // std::size_t

#include "math/Vector2.h"
#include "utils/TypeTraits.h"

namespace ptgn {

namespace type_traits {

// Returns true if the matrix contains an equal amount of rows and columns.
template <std::size_t ROWS, std::size_t COLUMNS>
using is_square_matrix_e = std::enable_if_t<ROWS == COLUMNS, bool>;

// Returns true if the matrix's INPUT_ROWS and INPUT_COLUMNS match the CONDITION_ROWS and CONDITION_COLUMNS respectively.
template <std::size_t INPUT_ROWS, std::size_t INPUT_COLUMNS, std::size_t CONDITION_ROWS, std::size_t CONDITION_COLUMNS>
using is_matrix_e = std::enable_if_t<INPUT_ROWS == CONDITION_ROWS && INPUT_COLUMNS == CONDITION_COLUMNS, bool>;

} // namespace type_traits

namespace internal {

static constexpr const char MATRIX_LEFT_DELIMETER{ '(' };
static constexpr const char MATRIX_CENTER_DELIMETER{ ',' };
static constexpr const char MATRIX_RIGHT_DELIMETER{ ')' };

} // namespace internal

template <typename T, std::size_t ROWS, std::size_t COLUMNS,
	type_traits::is_number_e<T> = true>
struct Matrix {
	template <type_traits::is_matrix_e<ROWS, COLUMNS, 2, 2> = true>
	Matrix(T m_00, T m_01, T m_10, T m_11) {
		matrix[0][0] = m_00;
		matrix[0][1] = m_01;
		matrix[1][0] = m_10;
		matrix[1][1] = m_11;
	}
	Matrix() {
		for (auto i{ 0 }; i < ROWS; ++i) {
			for (auto j{ 0 }; j < COLUMNS; ++j) {
				this->matrix[i][j] = T{ 0 };
			}
		}
	}
	Matrix(T matrix[ROWS][COLUMNS]) {
		for (auto i{ 0 }; i < ROWS; ++i) {
			for (auto j{ 0 }; j < COLUMNS; ++j) {
				this->matrix[i][j] = matrix[i][j];
			}
		}
	}

	template <type_traits::is_matrix_e<ROWS, COLUMNS, 2, 2> = true>
	void SetRotationMatrix(double radians) {
		auto c{ std::cos(radians) };
		auto s{ std::sin(radians) };

		matrix[0][0] = c; 
		matrix[0][1] = -s;
		matrix[1][0] = s;
		matrix[1][1] = c;
	}
	template <type_traits::is_matrix_e<ROWS, COLUMNS, 2, 2> = true>
	Matrix Transpose() {
		return { matrix[0][0], matrix[1][0], matrix[0][1], matrix[1][1] };
	}
	T matrix[ROWS][COLUMNS];
};

// Bitshift / stream operators.

template <typename T, std::size_t ROWS, std::size_t COLUMNS>
std::ostream& operator<<(std::ostream& os, const Matrix<T, ROWS, COLUMNS>& obj) {
	os << internal::MATRIX_LEFT_DELIMETER << " ";
	for (auto i{ 0 }; i < ROWS; ++i) {
		os << internal::MATRIX_LEFT_DELIMETER;
		for (auto j{ 0 }; j < COLUMNS; ++j) {
			os << obj.matrix[i][j];
			if (j != COLUMNS) {
				os << ",";
			}
		}
		os << internal::MATRIX_RIGHT_DELIMETER;
		if (i != ROWS) {
			os << ",";
		}
	}
	os << " " << internal::MATRIX_RIGHT_DELIMETER;
	return os;
}

// Multiply 2x2 Matrix by Vector2.
template <typename T, typename U, 
	type_traits::is_number_e<U> = true, 
	typename S = typename std::common_type<T, U>::type>
Vector2<S> operator*(Matrix<T, 2, 2> m, const Vector2<U>& v) {
	return { m.matrix[0][0] * v.x + m.matrix[0][1] * v.y, m.matrix[1][0] * v.x + m.matrix[1][1] * v.y };
}

} // namespace ptgn