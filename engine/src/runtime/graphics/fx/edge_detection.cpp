#include "runtime/graphics/fx/edge_detection.h"

#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

void EdgeDetection::Draw(DrawContext& ctx, Entity) {
	ctx.Pass([](auto& pass) -> RenderPassHandle { return pass.Apply("edge_detection"); });
}

} // namespace ptgn