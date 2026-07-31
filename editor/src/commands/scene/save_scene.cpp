#include "commands/scene/save_scene.h"

#include <utility>

#include "runtime/scene/scene.h"
#include "runtime/scene/scene_file.h"

namespace ptgn::editor {

SaveSceneCommand::SaveSceneCommand(Scene* scene, path file_path) :
	scene_{ scene }, file_path_{ std::move(file_path) } {}

void SaveSceneCommand::Execute() {
	if (scene_) {
		SaveSceneFile(file_path_, CaptureScene(*scene_));
	}
}

} // namespace ptgn::editor
