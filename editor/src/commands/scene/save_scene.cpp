#include "commands/scene/save_scene.h"

#include <utility>

#include "core/assert.h"
#include "core/util/file.h"
#include "runtime/scene/scene.h"

namespace ptgn::editor {

SaveSceneCommand::SaveSceneCommand(Scene* scene, path path) :
	scene_{ scene }, path_{ std::move(path) } {}

void SaveSceneCommand::Execute() {
	PTGN_ASSERT(scene_);
	// TODO: Fix scene serialization to file.
	// scene_->SerializeToFile(path_);
}

void SaveSceneCommand::Undo() {
	// Intentionally empty
}

} // namespace ptgn::editor