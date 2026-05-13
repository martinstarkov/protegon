#include "runtime/graphics/fx/effect.h"

#include "renderer/pipeline/draw_context.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene_render_graph.h"

namespace ptgn {

void Effect::Draw(DrawContext& draw, Entity entity) {
	draw.GetGraphBuilder().ApplyEffect(entity);
}

} // namespace ptgn