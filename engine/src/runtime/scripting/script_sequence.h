#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <variant>

#include "core/time/time.h"
#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/scripting/script.h"

namespace ptgn {

class Scene;

namespace impl {

struct ScriptSequenceData {
	explicit ScriptSequenceData(GameObject<Tween> tween);
	GameObject<Tween> tween;
};

} // namespace impl

using SequenceFunction = std::variant<std::function<void()>, std::function<void(Entity)>>;

class ScriptSequence : public Entity {
public:
	/// @brief Add a script that runs for the given duration.
	template <ScriptType TScript, typename... TArgs>
		requires std::constructible_from<TScript, TArgs...>
	ScriptSequence& During(milliseconds duration, TArgs&&... args) {
		const auto& instance{ Get<impl::ScriptSequenceData>() };
		auto& sequence{ Tween{ instance.tween }.During(duration) };
		auto& script{ sequence.GetLastTweenPoint().script_container_.Add<TScript>(
			*this, std::forward<TArgs>(args)...
		) };
		return *this;
	}

	/// @brief Add a function that runs continuously during the specified duration.
	ScriptSequence& During(milliseconds duration, SequenceFunction func);

	/// @brief Instantaneous function trigger.
	ScriptSequence& Then(SequenceFunction func);

	/// @brief Wait for a duration without running any functions.
	ScriptSequence& Wait(milliseconds duration);

	/// @brief Repeat the last added function repeats times, -1 for infinite repeats.
	ScriptSequence& Repeat(std::int64_t repeats);

	/// @brief Move onto the next sequence element, skipping the current one.
	ScriptSequence& MoveOn();

	/// @brief Start the sequence.
	void Start(bool force = true);
};

ScriptSequence CreateScriptSequence(Scene& scene, bool destroy_on_complete = true);

void After(Scene& scene, milliseconds duration, const SequenceFunction& func);

void During(Scene& scene, milliseconds duration, const SequenceFunction& func);

} // namespace ptgn