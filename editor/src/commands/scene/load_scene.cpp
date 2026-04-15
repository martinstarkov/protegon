#include "commands/scene/load_scene.h"

#include <utility>

#include "core/assert.h"
#include "core/util/file.h"
#include "runtime/scene/scene.h"

namespace ptgn::editor {

LoadSceneCommand::LoadSceneCommand(Scene* scene, path path) :
	scene_{ scene }, path_{ std::move(path) } {}

void LoadSceneCommand::Execute() {
	PTGN_ASSERT(scene_);
	// TODO: Fix scene serialization to string.
	// previous_scene_data_ = scene_->SerializeToString();

	// TODO: Fix scene deserialization from file.
	// Load new scene
	// scene_->DeserializeFromFile(path_);
}

void LoadSceneCommand::Undo() {
	PTGN_ASSERT(scene_);
	// TODO: Fix scene deserialization from string.
	// scene_->DeserializeFromString(previous_scene_data_);
}

} // namespace ptgn::editor