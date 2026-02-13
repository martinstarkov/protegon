#include "runtime/animation/offsets.h"

#include "core/math/transform.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"

namespace ptgn {

namespace impl {

Transform Offsets::GetTotal() const {
	return shake.RelativeTo(bounce).RelativeTo(custom);
}

} // namespace impl

Transform GetRelativeOffset(const Entity& entity) {
	return entity.Has<impl::Offsets>() ? entity.Get<impl::Offsets>().GetTotal() : Transform{};
}

Transform GetOffset(const Entity& entity) {
	return GetRelativeOffset(entity).RelativeTo(
		HasParent(entity) ? GetRelativeOffset(GetParent(entity)) : Transform{}
	);
}

} // namespace ptgn