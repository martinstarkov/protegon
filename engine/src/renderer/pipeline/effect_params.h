#pragma once

#include <functional>

namespace ptgn {

class DrawContext;

namespace impl {

struct EffectParams {
	std::function<void(DrawContext&)> draw_callback;

	int margin{ 0 };
};

} // namespace impl

} // namespace ptgn