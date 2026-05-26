#include "runtime/graphics/fx/grayscale.h"

#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

void Grayscale::Draw(DrawContext& ctx, Entity) {
	ctx.Pass([](auto& pass) { return pass.Apply("grayscale"); });
}

} // namespace ptgn