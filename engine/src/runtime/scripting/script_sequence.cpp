#include "runtime/scripting/script_sequence.h"

#include <chrono>
#include <functional>
#include <optional>
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

impl::ScriptSequenceData::ScriptSequenceData(GameObject<Tween> tween) : tween{ std::move(tween) } {}

ScriptSequence& ScriptSequence::During(milliseconds duration, DuringSequenceFunction func) {
	auto& instance{ Get<impl::ScriptSequenceData>() };
	instance.tween.During(duration).OnProgress([f = std::move(func)](auto p) {
		std::visit(
			[&]<typename T>(const T& func_variant) {
				if constexpr (std::is_same_v<T, std::function<void()>>) {
					func_variant();
				} else if constexpr (std::is_same_v<T, std::function<void(SequenceInfo)>>) {
					func_variant({ p.parent, p.tween, p.progress });
				} else {
					static_assert(false, "Incomplete variant visitor");
				}
			},
			f
		);
	});
	return *this;
}

ScriptSequence& ScriptSequence::Then(SequenceFunction func) {
	auto& instance{ Get<impl::ScriptSequenceData>() };
	instance.tween.During(0ms).OnPointComplete([f = std::move(func)](auto p) {
		std::visit(
			[&]<typename T>(const T& func_variant) {
				if constexpr (std::is_same_v<T, std::function<void()>>) {
					func_variant();
				} else if constexpr (std::is_same_v<T, std::function<void(ScriptSequence)>>) {
					func_variant(ScriptSequence{ p.parent });
				} else {
					static_assert(false, "Incomplete variant visitor");
				}
			},
			f
		);
	});
	return *this;
}

ScriptSequence& ScriptSequence::Wait(milliseconds duration) {
	auto& instance{ Get<impl::ScriptSequenceData>() };
	instance.tween.During(duration);
	return *this;
}

ScriptSequence& ScriptSequence::Repeat(std::optional<std::size_t> repeats) {
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
		instance.tween.During(0ms).OnComplete([](auto e) { e.parent.Destroy(); });
	}

	return sequence;
}

void After(Scene& scene, milliseconds duration, const SequenceFunction& func) {
	auto script_sequence{ CreateScriptSequence(scene) };
	script_sequence.Wait(duration);
	script_sequence.Then(func);
	script_sequence.Start();
}

void During(Scene& scene, milliseconds duration, const DuringSequenceFunction& func) {
	auto script_sequence{ CreateScriptSequence(scene) };
	script_sequence.During(duration, func);
	script_sequence.Start();
}

} // namespace ptgn