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
	const auto& instance{ Get<impl::ScriptSequenceInstance>() };
	Tween{ instance.tween }.During(duration).OnProgress(std::move(wrapped));
	return *this;
}

ScriptSequence& ScriptSequence::Then(std::function<void(Entity)> func) {
	auto wrapped = [f = std::move(func)](Entity e) {
		f(GetParent(e));
	};
	const auto& instance{ Get<impl::ScriptSequenceInstance>() };
	Tween{ instance.tween }.During(milliseconds{ 0 }).OnPointComplete(std::move(wrapped));
	return *this;
}

ScriptSequence& ScriptSequence::Wait(milliseconds duration) {
	const auto& instance{ Get<impl::ScriptSequenceInstance>() };
	Tween{ instance.tween }.During(duration);
	return *this;
}

ScriptSequence& ScriptSequence::Repeat(std::int64_t repeats) {
	const auto& instance{ Get<impl::ScriptSequenceInstance>() };
	Tween{ instance.tween }.Repeat(repeats);
	return *this;
}

ScriptSequence& ScriptSequence::MoveOn() {
	const auto& instance{ Get<impl::ScriptSequenceInstance>() };
	Tween{ instance.tween }.IncrementPoint();
	return *this;
}

void ScriptSequence::Start(bool force) {
	const auto& instance{ Get<impl::ScriptSequenceInstance>() };
	Tween{ instance.tween }.Start(force);
}

ScriptSequence CreateScriptSequence(Scene& scene, bool destroy_on_complete) {
	ScriptSequence sequence{ scene.CreateEntity() };

	auto tween{ CreateTween(scene) };
	SetParent(tween, sequence);

	const auto& instance{ sequence.Add<impl::ScriptSequenceInstance>(tween) };

	if (destroy_on_complete) {
		Tween{ instance.tween }.OnComplete([](Entity e) { GetParent(e).Destroy(); });
	}

	return sequence;
}

} // namespace ptgn