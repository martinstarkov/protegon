#include "core/script_sequence.h"

#include <cstdint>
#include <ctime>
#include <functional>
#include <utility>

#include "core/entity.h"
#include "entity_hierarchy.h"
#include "scene/scene.h"
#include "tweens/tween.h"

namespace ptgn {

impl::ScriptSequenceInstance::ScriptSequenceInstance(const Entity& entity) : tween{ entity } {}

ScriptSequence& ScriptSequence::During(milliseconds duration, std::function<void(Entity)> func) {
	// Wrap func to ignore float progress.
	auto wrapped = [f = std::move(func)](Entity e, float) {
		f(GetParent(e));
	};
	auto& instance{ Get<impl::ScriptSequenceInstance>() };
	Tween{ instance.tween }.During(duration).OnProgress(std::move(wrapped));
	return *this;
}

// Instantaneous step triggered on progress
ScriptSequence& ScriptSequence::Then(std::function<void(Entity)> func) {
	auto wrapped = [f = std::move(func)](Entity e) {
		f(GetParent(e));
	};
	auto& instance{ Get<impl::ScriptSequenceInstance>() };
	Tween{ instance.tween }.During(milliseconds{ 0 }).OnPointComplete(std::move(wrapped));
	return *this;
}

// Wait for a duration without running any script
ScriptSequence& ScriptSequence::Wait(milliseconds duration) {
	auto& instance{ Get<impl::ScriptSequenceInstance>() };
	Tween{ instance.tween }.During(duration);
	return *this;
}

// Repeat the last added tween point count times, -1 for infinite repeats.
ScriptSequence& ScriptSequence::Repeat(std::int64_t count) {
	auto& instance{ Get<impl::ScriptSequenceInstance>() };
	Tween{ instance.tween }.Repeat(count);
	return *this;
}

// Move onto the next sequence element, skipping the current one.
ScriptSequence& ScriptSequence::MoveOn() {
	auto& instance{ Get<impl::ScriptSequenceInstance>() };
	Tween{ instance.tween }.IncrementPoint();
	return *this;
}

// Start the sequence
void ScriptSequence::Start(bool force) {
	auto& instance{ Get<impl::ScriptSequenceInstance>() };
	Tween{ instance.tween }.Start(force);
}

ScriptSequence CreateScriptSequence(Scene& scene, bool destroy_on_complete) {
	ScriptSequence sequence{ scene.CreateEntity() };

	auto tween{ CreateTween(scene) };
	SetParent(tween, sequence);

	auto& instance{ sequence.Add<impl::ScriptSequenceInstance>(tween) };

	if (destroy_on_complete) {
		Tween{ instance.tween }.OnComplete([](Entity e) { GetParent(e).Destroy(); });
	}

	return sequence;
}

} // namespace ptgn
