#include "runtime/graphics/drawable.h"

#include <list>
#include <utility>

#include "core/assert.h"
#include "renderer/pipeline/draw_context.h"
#include "runtime/ecs/entity.h"

namespace ptgn::impl {

void InvokeDrawable(DrawContext& ctx, const Entity& entity) {
	PTGN_ASSERT(entity.Has<impl::IDrawable>(), "Cannot render entity without drawable component");

	const auto& drawable{ entity.Get<impl::IDrawable>() };

	const auto& drawable_functions{ impl::IDrawable::data() };

	PTGN_ASSERT(drawable_functions.contains(drawable.hash), "Failed to identify drawable hash");

	const auto& draw_function{ drawable_functions.find(drawable.hash)->second };

	draw_function(ctx, entity);
}

} // namespace ptgn::impl