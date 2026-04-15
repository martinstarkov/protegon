#pragma once

#include <string>

#include "commands/editor_command.h"
#include "core/util/file.h"
#include "serialization/json/json.h"

namespace ptgn {

class Scene;

namespace editor {

class LoadSceneCommand : public EditorCommand {
public:
	LoadSceneCommand(Scene* scene, path path);

	void Execute() override;
	void Undo() override;

private:
	Scene* scene_ = nullptr;
	path path_;

	json previous_scene_data_; // serialized backup
};

} // namespace editor

} // namespace ptgn