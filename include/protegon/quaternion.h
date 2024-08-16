#pragma once

#include "matrix4.h"
#include "vector3.h"
#include "vector4.h"

namespace ptgn {

class Quaternion : public V4_float {
public:
	using V4_float::V4_float;

	constexpr Quaternion() : V4_float{ 0.0f, 0.0f, 0.0f, 1.0f } {}

	constexpr Quaternion(const V4_float& v) : V4_float{ v } {}

	[[nodiscard]] constexpr Quaternion Conjugate() const {
		return Quaternion(-x, -y, -z, w);
	}

	[[nodiscard]] Quaternion Inverse() const {
		float dot{ Dot(*this) };
		PTGN_ASSERT(!NearlyEqual(dot, 0.0f));
		return Quaternion(Conjugate() / dot);
	}

	[[nodiscard]] static Quaternion GetAngleAxis(float angle, const V3_float& v) {
		float h = angle * 0.5f;
		float s = std::sin(h);
		return { std::cos(h), v.x * s, v.y * s, v.z * s };
	}

	[[nodiscard]] float GetAngle() const {
		if (std::abs(w) > cos_one_over_two) {
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

	[[nodiscard]] float GetRoll() const {
		float b = 2.0f * (x * y + w * z);
		float a = w * w + x * x - y * y - z * z;

		if (NearlyEqual(a, 0.0f) && NearlyEqual(b, 0.0f)) {
			return 0.0f;
		}

		return std::atan2(b, a);
	}

	[[nodiscard]] float GetPitch() const {
		float b = 2.0f * (y * z + w * x);
		float a = w * w - x * x - y * y + z * z;

		if (NearlyEqual(a, 0.0f) && NearlyEqual(b, 0.0f)) {
			return 2.0f * std::atan2(a, w);
		}

		return std::atan2(b, a);
	}

	[[nodiscard]] float GetYaw() const {
		return std::asin(std::clamp(-2.0f * (x * z - w * y), -1.0f, 1.0f));
	}

	M4_float ToMatrix4() const {
		M4_float result;
		float qxx{ x * x };
		float qyy{ y * y };
		float qzz{ z * z };
		float qxz{ x * z };
		float qxy{ x * y };
		float qyz{ y * z };
		float qwx{ w * x };
		float qwy{ w * y };
		float qwz{ w * z };

		result.m[0] = 1.0f - 2.0f * (qyy + qzz);
		result.m[1] = 2.0f * (qxy + qwz);
		result.m[2] = 2.0f * (qxz - qwy);
		result.m[3] = 0.0f;

		result.m[4] = 2.0f * (qxy - qwz);
		result.m[5] = 1.0f - 2.0f * (qxx + qzz);
		result.m[6] = 2.0f * (qyz + qwx);
		result.m[7] = 0.0f;

		result.m[8]	 = 2.0f * (qxz + qwy);
		result.m[9]	 = 2.0f * (qyz - qwx);
		result.m[10] = 1.0f - 2.0f * (qxx + qyy);
		result.m[11] = 0.0f;

		result.m[12] = 0.0f;
		result.m[13] = 0.0f;
		result.m[14] = 0.0f;
		result.m[15] = 1.0f;

		return result;
	}

private:
	constexpr static float cos_one_over_two{ 0.877582561890372716130286068203503191f };
};

constexpr inline V3_float operator*(const Quaternion& q, const V3_float& v) {
	const V3_float QuatVector(q.x, q.y, q.z);
	const V3_float uv(QuatVector.Cross(v));
	const V3_float uuv(QuatVector.Cross(uv));

	return v + ((uv * q.w) + uuv) * 2.0f;
}

constexpr inline V3_float operator*(const V3_float& v, const Quaternion& q) {
	return q.Inverse() * v;
}

} // namespace ptgn