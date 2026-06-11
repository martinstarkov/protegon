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
	Transform offset{ GetRelativeOffset(entity) };

	ForEachParent(
		entity, [](Entity e) { return e.Has<impl::IgnoreParentOffset>(); },
		[&offset](Entity parent) {
			offset = offset.RelativeTo(GetRelativeOffset(parent));
			return true;
		}
	);

	return offset;
}

void IgnoreParentOffset(Entity entity, bool ignore_parent_offset) {
	if (ignore_parent_offset) {
		entity.Add<impl::IgnoreParentOffset>();
	} else {
		entity.Remove<impl::IgnoreParentOffset>();
	}
}

void SetDrawOffset(Entity entity, V2_float offset) {
	entity.TryAdd<impl::Offsets>().custom.position = offset;
}

} // namespace ptgn