#include "renderer/render_target.h"

#include "renderer/color.h"

namespace ptgn {

Color RenderTarget::GetClearColor() const {
	return clear_color_;
}

BlendMode RenderTarget::GetBlendMode() const {
	return blend_mode_;
}

} // namespace ptgn