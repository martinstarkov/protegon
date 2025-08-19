#pragma once

#include <cstdint>
#include <ctime>
#include <functional>

#include "core/entity.h"
#include "core/game_object.h"

namespace ptgn {

class Scene;

namespace impl {

struct ScriptSequenceInstance {
	ScriptSequenceInstance(const Entity& entity);
	GameObject tween;
};

} // namespace impl

class ScriptSequence : public Entity {
public:
	// TODO: Readd once you add a wrapper script class that gives GetParent(entity) to the script.
	// Add a script that runs for duration
	// template <typename TScript, typename... Args>
	// ScriptSequence& During(milliseconds duration, Args&&... args) {
	//	tween_.During(duration).AddScript<TScript>(std::forward<Args>(args)...);
	//	return *this;
	//}

	// Add a function that runs continuously during the tween point (progress float ignored).
	ScriptSequence& During(milliseconds duration, std::function<void(Entity)> func);

	// Instantaneous step triggered on progress
	ScriptSequence& Then(std::function<void(Entity)> func);

	// Wait for a duration without running any script
	ScriptSequence& Wait(milliseconds duration);

	// Repeat the last added tween point count times, -1 for infinite repeats.
	ScriptSequence& Repeat(std::int64_t count);

	// Move onto the next sequence element, skipping the current one.
	ScriptSequence& MoveOn();

	// Start the sequence
	void Start(bool force = true);
};

ScriptSequence CreateScriptSequence(Scene& scene, bool destroy_on_complete = true);

} // namespace ptgn