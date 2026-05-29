#include "runtime/graphics/fx/inverse_color.h"

#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

void InverseColor::Draw(DrawContext& ctx, Entity) {
	ctx.Pass([](auto& pass) -> RenderPassHandle { return pass.Apply("inverse_color"); });
}

} // namespace ptgn