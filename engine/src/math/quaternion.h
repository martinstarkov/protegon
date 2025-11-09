#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

#include "core/assert.h"
#include "math/matrix4.h"
#include "math/vector3.h"
#include "math/vector4.h"

namespace ptgn {

class Quaternion : public V4_float {
public:
	using V4_float::V4_float;

	constexpr Quaternion() : V4_float{ 0.0f, 0.0f, 0.0f, 1.0f } {}

	explicit constexpr Quaternion(const V4_float& v) : V4_float{ v } {}

	[[nodiscard]] constexpr Quaternion Conjugate() const {
		return Quaternion(-x, -y, -z, w);
	}

	[[nodiscard]] constexpr Quaternion Inverse() const {
		float dot{ Dot(*this) };
		PTGN_ASSERT(dot > 0.0f);
		return Quaternion(Conjugate() / dot);
	}

	// From: https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
	// orientation is (yaw, pitch, roll) in radians.
	[[nodiscard]] static Quaternion FromEuler(const V3_float& orientation) {
		float half_yaw	 = orientation.x * 0.5f;
		float half_pitch = orientation.y * 0.5f;
		float half_roll	 = orientation.z * 0.5f;

		float cr = std::cos(half_roll);
		float sr = std::sin(half_roll);
		float cp = std::cos(half_pitch);
		float sp = std::sin(half_pitch);
		float cy = std::cos(half_yaw);
		float sy = std::sin(half_yaw);

		Quaternion q;
		q.x = sr * cp * cy - cr * sp * sy;
		q.y = cr * sp * cy + sr * cp * sy;
		q.z = cr * cp * sy - sr * sp * cy;
		q.w = cr * cp * cy + sr * sp * sy;

		return q;
	}

	// @return New quaternion rotated by the given angle in radians along the given axes.
	[[nodiscard]] static Quaternion GetAngleAxis(float angle_radians, const V3_float& axes) {
		const float a{ angle_radians * 0.5f };
		const float s = std::sin(a);

		return Quaternion(axes.x * s, axes.y * s, axes.z * s, std::cos(a));
	}

	// @return The euler angle of the quaternion in radians.
	[[nodiscard]] float GetAngle() const {
		if (std::abs(w) > cos_of_half) {
			float a = std::asin(std::sqrt(x * x + y * y + z * z)) * 2.0f;
			if (w < 0.0f) {
				return two_pi<float> - a;
			}
			return a;
		}

		return std::acos(w) * 2.0f;
	}

	[[nodiscard]] V3_float GetAxis() const {
		float tmp1 = 1.0f - w * w;
		if (tmp1 <= 0.0f) {
			return V3_float{ 0, 0, 1 };
		}
		float tmp2 = 1.0f / std::sqrt(tmp1);
		return V3_float{ x * tmp2, y * tmp2, z * tmp2 };
	}

	// Angle in radians.
	[[nodiscard]] float GetRoll() const {
		float b = 2.0f * (x * y + w * z);
		float a = w * w + x * x - y * y - z * z;

		if (NearlyEqual(a, 0.0f) && NearlyEqual(b, 0.0f)) {
			return 0.0f;
		}

		return std::atan2(b, a);
	}

	// Angle in radians.
	[[nodiscard]] float GetPitch() const {
		float b = 2.0f * (y * z + w * x);
		float a = w * w - x * x - y * y + z * z;

		if (NearlyEqual(a, 0.0f) && NearlyEqual(b, 0.0f)) {
			return 2.0f * std::atan2(a, w);
		}

		return std::atan2(b, a);
	}

	// Angle in radians.
	[[nodiscard]] float GetYaw() const {
		return std::asin(std::clamp(-2.0f * (x * z - w * y), -1.0f, 1.0f));
	}

	[[nodiscard]] Matrix4 ToMatrix4() const {
		Matrix4 result;
		float qxx{ x * x };
		float qyy{ y * y };
		float qzz{ z * z };
		float qxz{ x * z };
		float qxy{ x * y };
		float qyz{ y * z };
		float qwx{ w * x };
		float qwy{ w * y };
		float qwz{ w * z };

		result.m_[0] = 1.0f - 2.0f * (qyy + qzz);
		result.m_[1] = 2.0f * (qxy + qwz);
		result.m_[2] = 2.0f * (qxz - qwy);
		result.m_[3] = 0.0f;

		result.m_[4] = 2.0f * (qxy - qwz);
		result.m_[5] = 1.0f - 2.0f * (qxx + qzz);
		result.m_[6] = 2.0f * (qyz + qwx);
		result.m_[7] = 0.0f;

		result.m_[8]  = 2.0f * (qxz + qwy);
		result.m_[9]  = 2.0f * (qyz - qwx);
		result.m_[10] = 1.0f - 2.0f * (qxx + qyy);
		result.m_[11] = 0.0f;

		result.m_[12] = 0.0f;
		result.m_[13] = 0.0f;
		result.m_[14] = 0.0f;
		result.m_[15] = 1.0f;

		return result;
	}

	constexpr friend V3_float operator*(const Quaternion& q, const V3_float& v) {
		const V3_float QuatVector(q.x, q.y, q.z);
		const V3_float uv(QuatVector.Cross(v));
		const V3_float uuv(QuatVector.Cross(uv));

		return v + ((uv * q.w) + uuv) * 2.0f;
	}

	constexpr friend V3_float operator*(const V3_float& v, const Quaternion& q) {
		return q.Inverse() * v;
	}

private:
	// Angle in radians.
	constexpr static float cos_of_half{ 0.877582561890372716130286068203503191f };
};

} // namespace ptgn