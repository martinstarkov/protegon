#include "math/matrix4.h"

#include <cmath>
#include <functional>

#include "common/assert.h"
#include "components/transform.h"
#include "math/tolerance.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "serialization/fwd.h"
#include "serialization/json.h"

namespace ptgn {

void to_json(json& j, const Matrix4& matrix) {
	if (matrix != Matrix4{}) {
		j = matrix.m_;
	}
}

void from_json(const json& j, Matrix4& matrix) {
	if (j.empty()) {
		matrix = {};
	} else {
		j.get_to(matrix.m_);
	}
}

Matrix4 Matrix4::LookAt(
	const Vector3<float>& position, const Vector3<float>& target, const Vector3<float>& up
) {
	Vector3<float> dir{ (target - position).Normalized() };
	Vector3<float> right{ (dir.Cross(up)).Normalized() };
	Vector3<float> up_n{ right.Cross(dir) };

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

Matrix4 Matrix4::Inverse() const {
	// From:
	// https://github.com/g-truc/glm/blob/33b4a621a697a305bc3a7610d290677b96beb181/glm/detail/func_matrix.inl#L388

	float coef00{ m_[10] * m_[15] - m_[14] * m_[11] };
	float coef02{ m_[6] * m_[15] - m_[14] * m_[7] };
	float coef03{ m_[6] * m_[11] - m_[10] * m_[7] };
	float coef04{ m_[9] * m_[15] - m_[13] * m_[11] };
	float coef06{ m_[5] * m_[15] - m_[13] * m_[7] };
	float coef07{ m_[5] * m_[11] - m_[9] * m_[7] };
	float coef08{ m_[9] * m_[14] - m_[13] * m_[10] };
	float coef10{ m_[5] * m_[14] - m_[13] * m_[6] };
	float coef11{ m_[5] * m_[10] - m_[9] * m_[6] };
	float coef12{ m_[8] * m_[15] - m_[12] * m_[11] };
	float coef14{ m_[4] * m_[15] - m_[12] * m_[7] };
	float coef15{ m_[4] * m_[11] - m_[8] * m_[7] };
	float coef16{ m_[8] * m_[14] - m_[12] * m_[10] };
	float coef18{ m_[4] * m_[14] - m_[12] * m_[6] };
	float coef19{ m_[4] * m_[10] - m_[8] * m_[6] };
	float coef20{ m_[8] * m_[13] - m_[12] * m_[9] };
	float coef22{ m_[4] * m_[13] - m_[12] * m_[5] };
	float coef23{ m_[4] * m_[9] - m_[8] * m_[5] };

	Vector4 fac0{ coef00, coef00, coef02, coef03 };
	Vector4 fac1{ coef04, coef04, coef06, coef07 };
	Vector4 fac2{ coef08, coef08, coef10, coef11 };
	Vector4 fac3{ coef12, coef12, coef14, coef15 };
	Vector4 fac4{ coef16, coef16, coef18, coef19 };
	Vector4 fac5{ coef20, coef20, coef22, coef23 };

	Vector4 vec0{ m_[4], m_[0], m_[0], m_[0] };
	Vector4 vec1{ m_[5], m_[1], m_[1], m_[1] };
	Vector4 vec2{ m_[6], m_[2], m_[2], m_[2] };
	Vector4 vec3{ m_[7], m_[3], m_[3], m_[3] };

	Vector4 inv0{ vec1 * fac0 - vec2 * fac1 + vec3 * fac2 };
	Vector4 inv1{ vec0 * fac0 - vec2 * fac3 + vec3 * fac4 };
	Vector4 inv2{ vec0 * fac1 - vec1 * fac3 + vec3 * fac5 };
	Vector4 inv3{ vec0 * fac2 - vec1 * fac4 + vec2 * fac5 };

	Vector4 signA{ +1, -1, +1, -1 };
	Vector4 signB{ -1, +1, -1, +1 };

	Matrix4 inverse{ inv0 * signA, inv1 * signB, inv2 * signA, inv3 * signB };

	Vector4 row0{ inverse[0], inverse[4], inverse[8], inverse[12] };

	Vector4 dot0{ m_[0] * row0 };
	float determinant{ dot0.x + dot0.y + dot0.z + dot0.w };

	Matrix4 result{ inverse * 1.0f / determinant };

	return result;
}

Matrix4 Matrix4::MakeTransform(
	const Vector3<float>& position, float rotation_radians, const Vector3<float>& rotation_axis,
	const Vector3<float>& scale
) {
	Matrix4 m{ Matrix4::Identity() };

	m = Matrix4::Scale(m, scale);
	m = Matrix4::Rotate(m, rotation_radians, rotation_axis);
	m = Matrix4::Translate(m, position);

	return m;
}

Matrix4 Matrix4::MakeTransform(
	const Vector2<float>& position, float rotation_radians, const Vector2<float>& scale
) {
	return MakeTransform(
		{ position.x, position.y, 0.0f }, rotation_radians, { 0.0f, 0.0f, 1.0f },
		{ scale.x, scale.y, 1.0f }
	);
}

Matrix4 Matrix4::MakeTransform(const Transform& transform) {
	return MakeTransform(transform.GetPosition(), transform.GetRotation(), transform.GetScale());
}

Matrix4 Matrix4::MakeInverseTransform(
	const Vector3<float>& position, float rotation_radians, const Vector3<float>& rotation_axis,
	const Vector3<float>& scale
) {
	PTGN_ASSERT(!scale.HasZero(), "Cannot get inverse transform with zero scale");

	Matrix4 m{ Matrix4::Identity() };

	m = Matrix4::Scale(m, 1.0f / scale);
	m = Matrix4::Rotate(m, -rotation_radians, rotation_axis);
	m = Matrix4::Translate(m, -position);

	return m;
}

Matrix4 Matrix4::MakeInverseTransform(
	const Vector2<float>& position, float rotation_radians, const Vector2<float>& scale
) {
	return MakeInverseTransform(
		{ position.x, position.y, 0.0f }, rotation_radians, { 0.0f, 0.0f, 1.0f },
		{ scale.x, scale.y, 1.0f }
	);
}

Matrix4 Matrix4::MakeInverseTransform(const Transform& transform) {
	return MakeInverseTransform(
		transform.GetPosition(), transform.GetRotation(), transform.GetScale()
	);
}

Matrix4 Matrix4::Orthographic(
	float left, float right, float bottom, float top, float near, float far
) {
	Matrix4 o;

	float depth{ far - near };
	float horizontal{ right - left };
	float vertical{ top - bottom };

	PTGN_ASSERT(!NearlyEqual(depth, 0.0f), "Orthographic matrix depth cannot be zero");
	PTGN_ASSERT(!NearlyEqual(horizontal, 0.0f), "Orthographic matrix horizontal cannot be zero");
	PTGN_ASSERT(!NearlyEqual(vertical, 0.0f), "Orthographic matrix vertical cannot be zero");

	o[0]  = 2.0f / horizontal;
	o[5]  = 2.0f / vertical;
	o[10] = -2.0f / depth; // -1 by default
	o[12] = -(right + left) / horizontal;
	o[13] = -(top + bottom) / vertical;
	float plane_sum{ far + near };

	if (std::isnan(plane_sum)) {
		plane_sum = 0.0f;
	}

	o[14] = -plane_sum / depth; // 0 by default
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

Matrix4 Matrix4::Orthographic(const V2_float& min, const V2_float& max, float near, float far) {
	return Orthographic(min.x, max.x, max.y, min.y, near, far);
}

Matrix4 Matrix4::Perspective(float fov_x_radians, float aspect_ratio, float front, float back) {
	float tangent{ std::tan(fov_x_radians / 2.0f) }; // tangent of half fovX
	float right{ front * tangent };					 // half width of near plane
	PTGN_ASSERT(!NearlyEqual(aspect_ratio, 0.0f), "Perspective matrix aspect ratio cannot be zero");
	float top{ right / aspect_ratio };				 // half height of near plane

	float depth{ back - front };

	PTGN_ASSERT(!NearlyEqual(depth, 0.0f), "Perspective matrix depth cannot be zero");

	Matrix4 p;
	p[0]  = front / right;
	p[5]  = front / top;
	p[10] = -(back + front) / depth;
	p[11] = -1.0f;
	p[14] = -(2.0f * back * front) / depth;
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

Matrix4 Matrix4::Rotate(const Matrix4& matrix, float rotation_radians, const Vector3<float>& axes) {
	const float c{ std::cos(rotation_radians) };
	const float s{ std::sin(rotation_radians) };

	float magnitude{ axes.Dot(axes) };

	Vector3<float> axis;

	if (!NearlyEqual(magnitude, 0.0f)) {
		axis = axes.Normalized();
	}

	float d = 1.0f - c;

	Vector3 temp{ d * axis.x, d * axis.y, d * axis.z };

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