#pragma once

#include <string_view>
#include <utility>

#include "core/log.h"
#include "serialization/serialize.h"

namespace ptgn {

enum class ToneMappingOperator {
	None, // No tone mapping.
	Exposure,
	Reinhard,
	ACES
};
PTGN_SERIALIZE_ENUM(ToneMappingOperator)

struct ToneMappingSettings {
	ToneMappingOperator op{ ToneMappingOperator::None };
	/// @brief Only used by Exposure and ACES operators. Higher values will result in a brighter
	/// image.
	float exposure{ 1.0f };

	PTGN_SERIALIZE(ToneMappingSettings, op, exposure)
};

struct RenderSettings {
	ToneMappingSettings tone_mapping;
	/// @brief Gamma value to use for gamma correction. This is applied after tone mapping and
	/// should be set to 2.2 for correct sRGB output. Setting this to 1.0 will disable gamma
	/// correction.
	float gamma{ 2.2f };

	PTGN_SERIALIZE(RenderSettings, tone_mapping, gamma)
};

namespace impl {

/// @return The name of the shader to use for the given tone mapping operator. The shader will also
/// apply gamma correction based on the presentation settings gamma value.
constexpr std::string_view GetGammaAndToneMappingShader(ToneMappingOperator op) {
	switch (op) {
		using enum ToneMappingOperator;
		case None:	   return "linear_to_srgb";
		case Exposure: return "tone_mapping_exposure";
		case Reinhard: return "tone_mapping_reinhard";
		case ACES:	   return "tone_mapping_aces";
		default:	   PTGN_ERROR("Unknown ToneMappingOperator: ", std::to_underlying(op));
	}
}

constexpr bool RequiresHDRInput(ToneMappingOperator op) {
	switch (op) {
		using enum ToneMappingOperator;

		case None:	   return false;

		case Exposure: [[fallthrough]];
		case Reinhard: [[fallthrough]];
		case ACES:	   return true;

		default:	   PTGN_ERROR("Unknown ToneMappingOperator: ", std::to_underlying(op));
	}
}

} // namespace impl

} // namespace ptgn