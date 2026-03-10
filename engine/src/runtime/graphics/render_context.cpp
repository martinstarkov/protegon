#include "runtime/graphics/render_context.h"

#include "renderer/renderer.h"
#include "runtime/scene/scene.h"

namespace ptgn {

void RenderContext::Init(Scene& scene, Renderer& renderer) {
	scene_	  = &scene;
	renderer_ = &renderer;
}

} // namespace ptgn