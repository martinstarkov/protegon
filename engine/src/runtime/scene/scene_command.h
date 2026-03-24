#pragma once

#include <functional>
#include <memory>

#include "runtime/scene/scene_transition.h"

namespace ptgn {

class Scene;

namespace impl {

enum class SceneCommandType {
	Enter,
	Exit,
	ReEnter
};

struct SceneCommand {
	SceneCommandType type{ SceneCommandType::Enter };

	std::size_t from_scene_key{ 0 };
	std::size_t to_scene_key{ 0 };

	std::size_t priority{ 0 };

	std::function<std::unique_ptr<Scene>()> scene_factory;

	std::unique_ptr<SceneTransition> transition_in;
	std::unique_ptr<SceneTransition> transition_out;
};

} // namespace impl

} // namespace ptgn