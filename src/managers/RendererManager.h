#pragma once

#include "managers/SDLManager.h"
#include "renderer/Renderer.h"

namespace ptgn {

namespace managers {

class RendererManager : public SDLManager<Renderer> {
public:
	id GetFirstTargetRenderer();
	const id GetTargetRendererId() const;
	void SetTargetRenderer(const id window);
private:
	id target_renderer_{ 0 };
};

} // namespace managers

} // namespace ptgn