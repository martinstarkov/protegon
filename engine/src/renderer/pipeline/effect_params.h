#pragma once

#include <functional>
#include <utility>

#include "core/log.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

class DrawContext;

namespace impl {

constexpr static TextureFormat GetEffectTextureFormat(bool hdr) {
	return hdr ? kDefaultHDRFormat : kDefaultSDRFormat;
}

struct EffectParams {
	std::function<void(DrawContext&)> draw_callback;
	bool hdr{ false };
	int margin{ 0 };
};

} // namespace impl

} // namespace ptgn