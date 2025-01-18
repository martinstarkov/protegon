#include "renderer/layer_info.h"

#include <cstdint>

#include "core/game.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "scene/scene_manager.h"

namespace ptgn {

LayerInfo::LayerInfo() : LayerInfo{ RenderTarget{} } {}

LayerInfo::LayerInfo(const RenderTarget& render_target) {
	if (render_target.IsValid()) {
		render_target_ = render_target;
		return;
	}
	if (game.scene.HasCurrent()) {
		RenderTarget scene_target{ game.scene.GetCurrent().GetRenderTarget() };
		PTGN_ASSERT(scene_target.IsValid(), "Scene render target is invalid or uninitialized");
		render_target_ = scene_target;
		return;
	}
	PTGN_ASSERT(
		game.renderer.screen_target_.IsValid(),
		"Renderer must be initialized before drawing render targets"
	);
	render_target_ = game.renderer.screen_target_;
}

LayerInfo::LayerInfo(std::int32_t render_layer, const RenderTarget& render_target) :
	LayerInfo{ render_target } {
	render_layer_ = render_layer;
}

bool LayerInfo::operator==(const LayerInfo& o) const {
	return render_target_ == o.render_target_ && render_layer_ == o.render_layer_;
}

bool LayerInfo::operator!=(const LayerInfo& o) const {
	return !(*this == o);
}

bool LayerInfo::HasCustomRenderTarget() const {
	return render_target_.IsValid();
}

RenderTarget LayerInfo::GetRenderTarget() const {
	PTGN_ASSERT(render_target_.IsValid(), "Failed to find a valid render target");
	return render_target_;
}

std::int32_t LayerInfo::GetRenderLayer() const {
	return render_layer_;
}

LayerInfo::LayerInfo(const ScreenLayer&) {}

} // namespace ptgn