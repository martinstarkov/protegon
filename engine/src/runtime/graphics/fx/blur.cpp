#include "runtime/graphics/fx/blur.h"

#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

void Blur::Draw(DrawContext& ctx, Entity) {
	ctx.Pass([](auto& pass) { return pass.Apply("blur"); });
}

} // namespace ptgn