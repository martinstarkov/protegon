#include "components/offsets.h"

#include "components/transform.h"
#include "core/entity.h"

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