#pragma once

#include "core/util/file.h"

namespace ptgn {

class Scene;

namespace editor {

class SaveSceneCommand {
public:
	SaveSceneCommand(Scene* scene, path file_path);
	void Execute();

private:
	Scene* scene_{ nullptr };
	path file_path_;
};

} // namespace editor

} // namespace ptgn
