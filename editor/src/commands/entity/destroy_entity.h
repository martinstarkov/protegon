#pragma once

#include "commands/editor_command.h"
#include "runtime/ecs/entity.h"
#include "serialization/json/json.h"

namespace ptgn {

class Scene;

namespace editor {

class DeleteEntityCommand : public EditorCommand {
public:
	DeleteEntityCommand(Scene* scene, Entity entity);

	void Execute() override;
	void Undo() override;

private:
	Scene* scene_{ nullptr };
	Entity entity_;
	json backup_;
};

} // namespace editor

} // namespace ptgn