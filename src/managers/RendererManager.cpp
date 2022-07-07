#include "RendererManager.h"

#include "debugging/Debug.h"

namespace ptgn {

namespace managers {

const id RendererManager::GetTargetRendererId() const {
	assert(Has(target_renderer_) && "Could not find a valid target renderer");
	return target_renderer_;
}

void RendererManager::SetTargetRenderer(const id renderer) {
	assert(Has(renderer) && "Cannot set target renderer to nonexistent window");
	target_renderer_ = renderer;
}

id RendererManager::GetFirstTargetRenderer() {
	return target_renderer_;
}

} // namespace managers

} // namespace ptgn