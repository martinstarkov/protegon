#include "math/matrix4.h"

#include <cmath>
#include <type_traits>

#include "common/assert.h"
#include "math/math.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "serialization/json.h"

namespace ptgn {

void to_json(json& nlohmann_json_j, const Matrix4& nlohmann_json_t) {
	if constexpr (ptgn::impl::has_equality_v<
					  std::remove_reference_t<decltype(nlohmann_json_t.m_)>,
					  std::remove_reference_t<decltype(Matrix4{}.m_)>> &&
				  std::is_default_constructible_v<Matrix4>) {
		if (!(nlohmann_json_t.m_ == Matrix4{}.m_)) {
			nlohmann_json_j = nlohmann_json_t.m_;
		}
	} else {
		nlohmann_json_j = nlohmann_json_t.m_;
	}
}

void from_json(const json& nlohmann_json_j, Matrix4& nlohmann_json_t) {
	if (nlohmann_json_j.empty()) {
		nlohmann_json_t = {};
	} else {
		nlohmann_json_j.get_to(nlohmann_json_t.m_);
	}
}

Matrix4 Matrix4::LookAt(
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

Matrix4 Matrix4::Identity() {
	return Matrix4{ 1.0f };
}

Matrix4 Matrix4::Orthographic(
	float left, float right, float bottom, float top, float near, float far
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

Matrix4 Matrix4::Inverse() const {
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

Matrix4 Matrix4::Perspective(float fov_x_radians, float aspect_ratio, float front, float back) {
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

Matrix4 Matrix4::Translate(const Matrix4& m, const Vector3<float>& axes) {
	Matrix4 result{ m };
	for (std::size_t i{ 0 }; i < result.size.x; i++) {
		result[i + 12] = m[i] * axes.x + m[i + 4] * axes.y + m[i + 8] * axes.z + m[i + 12];
	}
	return result;
}

Matrix4 Matrix4::Rotate(const Matrix4& matrix, float angle_radians, const Vector3<float>& axes) {
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

Matrix4 Matrix4::Scale(const Matrix4& m, const Vector3<float>& axes) {
	Matrix4 result;
	for (std::size_t i{ 0 }; i < result.size.x; i++) {
		result[i + 0]  = m[i + 0] * axes.x;
		result[i + 4]  = m[i + 4] * axes.y;
		result[i + 8]  = m[i + 8] * axes.z;
		result[i + 12] = m[i + 12];
	}
	return result;
}

bool Matrix4::IsZero() const {
	for (std::size_t i{ 0 }; i < length; i++) {
		if (!NearlyEqual(m_[i], 0.0f)) {
			return false;
		}
	}
	return true;
}

bool Matrix4::ExactlyEquals(const Matrix4& o) const {
	for (std::size_t i{ 0 }; i < length; i++) {
		if (m_[i] != o[i]) {
			return false;
		}
	}
	return true;
}

bool Matrix4::operator==(const Matrix4& o) const {
	for (std::size_t i{ 0 }; i < length; i++) {
		if (!NearlyEqual(m_[i], o.m_[i])) {
			return false;
		}
	}
	return true;
}

bool Matrix4::operator!=(const Matrix4& o) const {
	return !operator==(o);
}

Matrix4 Matrix4::operator+(const Matrix4& rhs) {
	Matrix4 result;
	for (std::size_t i{ 0 }; i < result.length; i++) {
		result[i] = m_[i] + rhs[i];
	}
	return result;
}

Matrix4 Matrix4::operator-(const Matrix4& rhs) {
	Matrix4 result;
	for (std::size_t i{ 0 }; i < result.length; i++) {
		result[i] = m_[i] - rhs[i];
	}
	return result;
}

Matrix4 Matrix4::operator*(const Matrix4& rhs) {
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

} // namespace ptgn
