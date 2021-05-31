#include "HitboxComponent.h"

#include "components/TagComponent.h"

namespace ptgn {

void HitboxComponent::Resolve(ecs::Entity& target, const Manifold& manifold) {
	if (resolution_function != nullptr) {
		resolution_function(target, manifold);
	}
}

bool HitboxComponent::CanCollideWith(const ecs::Entity& entity) {
	if (ignored_tags.size() > 0 && entity.HasComponent<TagComponent>()) {
		const auto id{ entity.GetComponent<TagComponent>().id };
		for (const auto tag : ignored_tags) {
			if (id == tag) {
				return false;
			}
		}
	}
	return true;
}

} // namespace ptgn