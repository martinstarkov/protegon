#include "runtime/graphics/tint.h"

#include "core/graphics/color.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

void SetTint(Entity entity, Color color) {
	if (color != impl::Tint{}) {
		entity.Add<impl::Tint>(color);
	} else {
		entity.Remove<impl::Tint>();
	}
}

Color GetTint(Entity entity) {
	return entity.GetOrDefault<impl::Tint>();
}

} // namespace ptgn