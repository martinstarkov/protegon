#include "runtime/ecs/entity_creation_context.h"

#include <utility>

#include "core/assert.h"
#include "runtime/ecs/manager.h"
#include "runtime/ecs/uuid.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

EntityCreationContext::EntityCreationContext(Scene& scene) : scene_{ &scene } {}

EntityCreationContext::EntityCreationContext(
	Manager& manager, AssetManager* assets, Renderer* renderer
) :
	manager_{ &manager }, assets_{ assets }, renderer_{ renderer } {}

AssetManager* EntityCreationContext::GetAssetManager() const {
	return assets_ ? assets_ : (scene_ ? &scene_->ctx().asset : nullptr);
}

Renderer* EntityCreationContext::GetRenderer() const {
	return renderer_ ? renderer_ : (scene_ ? &scene_->ctx().renderer : nullptr);
}

Entity EntityCreationContext::CreateEntity(Tag tag) const {
	if (scene_) {
		return scene_->CreateEntity(std::move(tag));
	}

	PTGN_ASSERT(manager_, "Entity creation requires a manager or a Scene");

	Entity entity{ manager_->CreateEntity() };
	entity.Add<Tag>(std::move(tag));
	entity.TryAdd<UUID>();
	return entity;
}

} // namespace ptgn
