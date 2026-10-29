#pragma once

#include <memory>

#include "commands/entity/entity_reference.h"
#include "runtime/ecs/entity.h"
#include "runtime/scripting/variables.h"
#include "runtime/timer/timer.h"

namespace ptgn {

class Scene;

} // namespace ptgn

namespace ptgn::impl {

class Scripts;

} // namespace ptgn::impl

namespace ptgn::editor {

class Editor;
class EditorContext;

namespace inspector {

bool DrawScriptsComponent(EditorContext& ctx, ::ptgn::impl::Scripts& scripts);

// Captures script and event references throughout the scene for undoing a rename.
struct TimerReferenceSceneSnapshot {
	std::shared_ptr<const void> data{};
};

TimerReferenceSceneSnapshot CaptureTimerReferenceSceneSnapshot(Scene& scene);

void RestoreTimerReferenceSceneSnapshot(
	Editor& editor, const EntityReference& anchor_reference,
	const TimerReferenceSceneSnapshot& snapshot
);

bool RenameTimerReferences(Entity timer_entity, const TimerKey& old_key, const TimerKey& new_key);
bool RenameCounterReferences(Entity entity, const CounterKey& old_key, const CounterKey& new_key);
bool RenameBooleanReferences(Entity entity, const BooleanKey& old_key, const BooleanKey& new_key);

} // namespace inspector

} // namespace ptgn::editor
