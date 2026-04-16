#pragma once

#include <string>
#include <string_view>

#include "commands/editor_command.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Scene;

namespace editor {

class CreateEntityCommand : public EditorCommand {
public:
	CreateEntityCommand(Scene* scene, std::string_view name);

	void Execute() override;
	void Undo() override;

	Entity GetEntity() const;

private:
	Scene* scene_{ nullptr };
	std::string name_;
	Entity entity_;
};

} // namespace editor

} // namespace ptgn