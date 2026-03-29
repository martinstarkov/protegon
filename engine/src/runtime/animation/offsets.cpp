#include "runtime/animation/offsets.h"

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"

namespace ptgn {

namespace impl {

Transform Offsets::GetTotal() const {
	return shake.RelativeTo(bounce).RelativeTo(custom);
}

} // namespace impl

Transform GetRelativeOffset(Entity entity) {
	return entity.Has<impl::Offsets>() ? entity.Get<impl::Offsets>().GetTotal() : Transform{};
}

Transform GetOffset(Entity entity) {
	return GetRelativeOffset(entity).RelativeTo(
		HasParent(entity) ? GetRelativeOffset(GetParent(entity)) : Transform{}
	);
}

void SetDrawOffset(Entity entity, V2_float offset) {
	entity.TryAdd<impl::Offsets>().custom.SetPosition(offset);
}

} // namespace ptgn