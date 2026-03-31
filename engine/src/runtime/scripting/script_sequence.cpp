#include "runtime/scripting/script_sequence.h"

#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

#include "core/time/time.h"
#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/game_object.h"
#include "runtime/scene/scene.h"

namespace ptgn {

static void VisitSequenceFunction(const SequenceFunction& func, Entity entity) {
	std::visit(
		[&]<typename T>(const T& func_variant) {
			if constexpr (std::is_same_v<T, std::function<void()>>) {
				func_variant();
			} else if constexpr (std::is_same_v<T, std::function<void(Entity)>>) {
				auto parent{ GetParent(entity) };
				func_variant(parent);
			}
		},
		func
	);
}

impl::ScriptSequenceData::ScriptSequenceData(GameObject<Tween> tween) : tween{ std::move(tween) } {}

ScriptSequence& ScriptSequence::During(milliseconds duration, SequenceFunction func) {
	auto& instance{ Get<impl::ScriptSequenceData>() };
	instance.tween.During(duration).OnProgress([f = std::move(func)](Entity e, float) {
		VisitSequenceFunction(f, e);
	});
	return *this;
}

ScriptSequence& ScriptSequence::Then(SequenceFunction func) {
	auto& instance{ Get<impl::ScriptSequenceData>() };
	instance.tween.During(milliseconds{ 0 }).OnPointComplete([f = std::move(func)](Entity e) {
		VisitSequenceFunction(f, e);
	});
	return *this;
}

ScriptSequence& ScriptSequence::Wait(milliseconds duration) {
	auto& instance{ Get<impl::ScriptSequenceData>() };
	instance.tween.During(duration);
	return *this;
}

ScriptSequence& ScriptSequence::Repeat(std::int64_t repeats) {
	auto& instance{ Get<impl::ScriptSequenceData>() };
	instance.tween.Repeat(repeats);
	return *this;
}

ScriptSequence& ScriptSequence::MoveOn() {
	auto& instance{ Get<impl::ScriptSequenceData>() };
	instance.tween.IncrementPoint();
	return *this;
}

void ScriptSequence::Start(bool force) {
	auto& instance{ Get<impl::ScriptSequenceData>() };
	instance.tween.Start(force);
}

ScriptSequence CreateScriptSequence(Scene& scene, bool destroy_on_complete) {
	ScriptSequence sequence{ scene.CreateEntity() };

	auto tween{ CreateTween(scene) };
	AddChild(sequence, tween, "tween");

	auto& instance{ sequence.Add<impl::ScriptSequenceData>(GameObject{ std::move(tween) }) };

	if (destroy_on_complete) {
		instance.tween.During(milliseconds{ 0 }).OnComplete([](Entity e) {
			GetParent(e).Destroy();
		});
	}

	return sequence;
}

void After(Scene& scene, milliseconds duration, const SequenceFunction& func) {
	auto script_sequence{ CreateScriptSequence(scene) };
	script_sequence.Wait(duration);
	script_sequence.Then(func);
	script_sequence.Start();
}

void During(Scene& scene, milliseconds duration, const SequenceFunction& func) {
	auto script_sequence{ CreateScriptSequence(scene) };
	script_sequence.During(duration, func);
	script_sequence.Start();
}

} // namespace ptgn