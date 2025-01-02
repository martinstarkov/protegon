#include "renderer/render_target.h"

#include "renderer/color.h"
#include "renderer/layer_info.h"

namespace ptgn {

RenderTarget::RenderTarget(const Color& clear_color, BlendMode blend_mode) {
	// TODO: Add all the initialization stuff.
	Create();
}

Color RenderTarget::GetClearColor() const {
	PTGN_ASSERT(IsValid(), "Cannot get clear color of invalid or uninitialized render target");
	return Get().clear_color_;
}

BlendMode RenderTarget::GetBlendMode() const {
	PTGN_ASSERT(IsValid(), "Cannot get blend mode of invalid or uninitialized render target");
	return Get().blend_mode_;
}

void RenderTarget::Draw(const Rect& destination) {
	// TODO: Add
}

void RenderTarget::Draw(const Rect& destination, const LayerInfo& layer_info) {
	// TODO: Add
}

} // namespace ptgn
