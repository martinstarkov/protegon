#include "runtime/graphics/fx/grayscale.h"

#include "renderer/draw_context.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

void Grayscale::Draw(DrawContext& ctx, Entity) {
	ctx.Pass([](auto& pass) -> RenderPassHandle { return pass.Apply("grayscale"); });
}

} // namespace ptgn