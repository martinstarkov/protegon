#include "commands/entity/create_entity.h"

#include <string>
#include <utility>

#include "core/assert.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"

namespace ptgn::editor {

CreateEntityCommand::CreateEntityCommand(Scene* scene, std::string name) :
	scene_{ scene }, name_{ std::move(name) } {}

void CreateEntityCommand::Execute() {
	PTGN_ASSERT(scene_);
	entity_ = scene_->CreateEntity();
	// TODO: Use actual name component.
	entity_.Add<std::string>(name_);
}

void CreateEntityCommand::Undo() {
	PTGN_ASSERT(scene_);
	entity_.Destroy();
}

Entity CreateEntityCommand::GetEntity() const {
	return entity_;
}

} // namespace ptgn::editor