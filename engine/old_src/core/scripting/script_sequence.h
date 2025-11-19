#pragma once

#include <concepts>
#include <cstdint>
#include <ctime>
#include <functional>

#include "ecs/entity.h"
#include "ecs/game_object.h"
#include "core/scripting/script.h"
#include "core/scripting/script_interfaces.h"
#include "tween/tween.h"

namespace ptgn {

class Scene;

namespace impl {

struct ScriptSequenceInstance {
	explicit ScriptSequenceInstance(const Entity& entity);
	GameObject<Tween> tween;
};

} // namespace impl

class ScriptSequence : public Entity {
public:
	// Add a script that runs for the given duration.
	template <std::derived_from<TweenScript> TScript, typename... TArgs>
		requires std::constructible_from<TScript, TArgs...>
	ScriptSequence& During(milliseconds duration, TArgs&&... args) {
		auto& instance{ Get<impl::ScriptSequenceInstance>() };
		auto& sequence{ instance.tween.During(duration) };
		auto& script{ sequence.GetLastTweenPoint().script_container_.AddScript<TScript>(
			std::forward<TArgs>(args)...
		) };
		script.entity = *this;
		return *this;
	}

	// Add a function that runs continuously during the specified duration.
	ScriptSequence& During(milliseconds duration, std::function<void(Entity)> func);

	// Instantaneous function trigger.
	ScriptSequence& Then(std::function<void(Entity)> func);

	// Wait for a duration without running any functions.
	ScriptSequence& Wait(milliseconds duration);

	// Repeat the last added function repeats times, -1 for infinite repeats.
	ScriptSequence& Repeat(std::int64_t repeats);

	// Move onto the next sequence element, skipping the current one.
	ScriptSequence& MoveOn();

	// Start the sequence.
	void Start(bool force = true);
};

ScriptSequence CreateScriptSequence(Scene& scene, bool destroy_on_complete = true);

void After(Scene& scene, milliseconds duration, const std::function<void(Entity)>& func);

void During(Scene& scene, milliseconds duration, const std::function<void(Entity)>& func);

} // namespace ptgn