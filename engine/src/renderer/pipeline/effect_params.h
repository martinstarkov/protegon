#pragma once

#include <functional>
#include <utility>

#include "core/log.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

class DrawContext;

namespace impl {

enum class ColorRange {
	SDR,
	HDR,
};

template <typename T>
struct EffectTraits {
	static constexpr bool requires_hdr{ false };
	static constexpr ColorRange color_range{ ColorRange::SDR };
};

constexpr static TextureFormat GetTextureFormat(ColorRange color_range) {
	switch (color_range) {
		using enum ColorRange;
		case SDR: return kDefaultSDRFormat;
		case HDR: return kDefaultHDRFormat;
		default:  PTGN_ERROR("Unknown ColorRange: ", std::to_underlying(color_range));
	}
}

struct EffectParams {
	std::function<void(DrawContext&)> draw_callback;
	ColorRange color_range{ ColorRange::SDR };
	int margin{ 0 };
};

} // namespace impl

} // namespace ptgn