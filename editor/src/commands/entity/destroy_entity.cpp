#include "commands/entity/destroy_entity.h"

#include "core/assert.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"

namespace ptgn::editor {

DeleteEntityCommand::DeleteEntityCommand(Scene* scene, Entity entity) :
	scene_{ scene }, entity_{ entity } {}

void DeleteEntityCommand::Execute() {
	PTGN_ASSERT(scene_);
	// TODO: Fix entity component serialization.
	// backup_ = scene_->SerializeEntity(entity_);
	entity_.Destroy();
}

void DeleteEntityCommand::Undo() {
	PTGN_ASSERT(scene_);
	// TODO: Fix entity component deserialization.
	// entity_ = scene_->DeserializeEntity(backup_);
}

} // namespace ptgn::editor