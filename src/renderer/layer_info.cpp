#include "renderer/layer_info.h"

#include <cstdint>

#include "core/game.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "scene/scene_manager.h"

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

RenderTarget LayerInfo::GetActiveTarget() const {
	if (render_target_ != RenderTarget{}) {
		return render_target_;
	}

	auto scene{ game.scene.currently_updating_ };
	if (scene != nullptr) {
		return scene->GetRenderTarget();
	}

	return game.renderer.screen_target_;
}

std::int32_t LayerInfo::GetRenderLayer() const {
	return render_layer_;
}

} // namespace ptgn