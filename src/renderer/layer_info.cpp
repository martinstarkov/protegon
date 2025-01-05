#include "renderer/layer_info.h"

#include <cstdint>

#include "renderer/render_target.h"

namespace ptgn {

LayerInfo::LayerInfo(const RenderTarget& render_target) : render_target_{ render_target } {}

LayerInfo::LayerInfo(std::int32_t render_layer, const RenderTarget& render_target) :
	render_layer_{ render_layer }, render_target_{ render_target } {}

bool LayerInfo::operator==(const LayerInfo& o) const {
	return render_target_ == o.render_target_ && render_layer_ == o.render_layer_;
}

bool LayerInfo::operator!=(const LayerInfo& o) const {
	return !(*this == o);
}

RenderTarget LayerInfo::GetRenderTarget() const {
	return render_target_;
}

std::int32_t LayerInfo::GetRenderLayer() const {
	return render_layer_;
}

} // namespace ptgn