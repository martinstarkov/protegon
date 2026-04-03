#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

#include "core/assert.h"
#include "core/math/angle.h"
#include "core/math/math_utils.h"
#include "core/math/matrix4.h"
#include "core/math/tolerance.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"

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

	/// @brief From: https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
	[[nodiscard]] static Quaternion FromEuler(Radians yaw, Radians pitch, Radians roll) {
		auto half_yaw	= yaw * 0.5f;
		auto half_pitch = pitch * 0.5f;
		auto half_roll	= roll * 0.5f;

		float sr = half_roll.Sin();
		float cr = half_roll.Cos();
		float sp = half_pitch.Sin();
		float cp = half_pitch.Cos();
		float sy = half_yaw.Sin();
		float cy = half_yaw.Cos();

		Quaternion q;
		q.x = sr * cp * cy - cr * sp * sy;
		q.y = cr * sp * cy + sr * cp * sy;
		q.z = cr * cp * sy - sr * sp * cy;
		q.w = cr * cp * cy + sr * sp * sy;

		return q;
	}

	/// @return New quaternion rotated by the given angle along the given axes.
	static Quaternion GetAngleAxis(Radians angle, const V3_float& axes) {
		const auto a{ angle * 0.5f };
		const float s = a.Sin();

		return Quaternion(axes.x * s, axes.y * s, axes.z * s, a.Cos());
	}

	/// @return The euler angle of the quaternion.
	Radians GetAngle() const {
		if (std::abs(w) > cos_of_half) {
			Radians a{ std::asin(std::sqrt(x * x + y * y + z * z)) * 2.0f };
			if (w < 0.0f) {
				return Radians{ kTwoPi } - a;
			}
			return a;
		}

		return Radians{ std::acos(w) * 2.0f };
	}

	V3_float GetAxis() const {
		float tmp1 = 1.0f - w * w;
		if (tmp1 <= 0.0f) {
			return V3_float{ 0, 0, 1 };
		}
		float tmp2 = 1.0f / std::sqrt(tmp1);
		return V3_float{ x * tmp2, y * tmp2, z * tmp2 };
	}

	Radians GetRoll() const {
		float b = 2.0f * (x * y + w * z);
		float a = w * w + x * x - y * y - z * z;

		if (NearlyEqual(a, 0.0f) && NearlyEqual(b, 0.0f)) {
			return Radians{ 0.0f };
		}

		return Radians{ std::atan2(b, a) };
	}

	Radians GetPitch() const {
		float b = 2.0f * (y * z + w * x);
		float a = w * w - x * x - y * y + z * z;

		if (NearlyEqual(a, 0.0f) && NearlyEqual(b, 0.0f)) {
			return Radians{ 2.0f * std::atan2(a, w) };
		}

		return Radians{ std::atan2(b, a) };
	}

	Radians GetYaw() const {
		return Radians{ std::asin(std::clamp(-2.0f * (x * z - w * y), -1.0f, 1.0f)) };
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
	/// @brief Equivalent to cos(0.5 rad).
	constexpr static float cos_of_half{ 0.877582561890372716130286068203503191f };
};

} // namespace ptgn