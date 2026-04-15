#pragma once

#include <string>

#include "commands/editor_command.h"
#include "core/util/file.h"

namespace ptgn {

class Scene;

namespace editor {

class SaveSceneCommand : public EditorCommand {
public:
	SaveSceneCommand(Scene* scene, path path);

	void Execute() override;
	void Undo() override; // no-op

private:
	Scene* scene_{ nullptr };
	path path_;
};

} // namespace editor

} // namespace ptgn