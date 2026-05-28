#include "runtime/graphics/fx/sharpen.h"

#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

void Sharpen::Draw(DrawContext& ctx, Entity) {
	ctx.Pass([](auto& pass) { return pass.Apply("sharpen"); });
}

} // namespace ptgn