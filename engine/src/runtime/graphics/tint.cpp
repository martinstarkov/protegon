#include "runtime/graphics/tint.h"

#include "core/graphics/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"

namespace ptgn {

void SetTint(Entity entity, Color color) {
	entity.Add<impl::Tint>(color);
}

Color GetTint(Entity entity) {
	Color tint{ entity.GetOrDefault<impl::Tint>() };

	ForEachParent(
		entity, [](Entity e) { return e.Has<impl::IgnoreParentTint>(); },
		[&tint](Entity parent) {
			tint = Color::Multiply(parent.GetOrDefault<impl::Tint>(), tint);
			return true;
		}
	);

	return tint;
}

void IgnoreParentTint(Entity entity, bool ignore_parent_tint) {
	if (ignore_parent_tint) {
		entity.Add<impl::IgnoreParentTint>();
	} else {
		entity.Remove<impl::IgnoreParentTint>();
	}
}

} // namespace ptgn