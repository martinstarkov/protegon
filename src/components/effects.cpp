#include "components/effects.h"

#include "components/draw.h"
#include "utility/span.h"

namespace ptgn {

Entity& AddPostFX(Entity& entity, Entity post_fx) {
	Hide(post_fx);
	auto& post_fx_list{ entity.TryAdd<impl::PostFX>().post_fx_ };
	PTGN_ASSERT(
		!VectorContains(post_fx_list, post_fx),
		"Cannot add the same post fx entity to an entity more than once"
	);
	post_fx_list.emplace_back(post_fx);
	return entity;
}

Entity& AddPreFX(Entity& entity, Entity pre_fx) {
	Hide(pre_fx);
	auto& pre_fx_list{ entity.TryAdd<impl::PreFX>().pre_fx_ };
	PTGN_ASSERT(
		!VectorContains(pre_fx_list, pre_fx),
		"Cannot add the same pre fx entity to an entity more than once"
	);
	pre_fx_list.emplace_back(pre_fx);
	return entity;
}

} // namespace ptgn