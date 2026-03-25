#pragma once

#include <functional>
#include <memory>

#include "runtime/scene/scene_transition.h"

namespace ptgn {

class Scene;

struct ScenePriority {
	explicit ScenePriority() = default;

	explicit ScenePriority(std::size_t value) : value{ value } {}

	std::size_t value{ 0 };
};

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

	ScenePriority priority;

	std::function<std::unique_ptr<Scene>()> scene_factory;

	std::unique_ptr<SceneTransition> transition_out;
	std::unique_ptr<SceneTransition> transition_in;
};

} // namespace impl

} // namespace ptgn