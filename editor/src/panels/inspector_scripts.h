#pragma once

#include <memory>

#include "commands/entity/entity_reference.h"
#include "runtime/ecs/entity.h"
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

// Opaque snapshot used by the timer inspector when a timer rename also updates
// script/event references throughout the scene.
struct TimerReferenceSceneSnapshot {
	std::shared_ptr<const void> data{};
};

[[nodiscard]] TimerReferenceSceneSnapshot CaptureTimerReferenceSceneSnapshot(Scene& scene);
void RestoreTimerReferenceSceneSnapshot(
	Editor& editor, const EntityReference& anchor_reference,
	const TimerReferenceSceneSnapshot& snapshot
);
[[nodiscard]] bool RenameTimerReferences(
	Entity timer_entity, const TimerKey& old_key, const TimerKey& new_key
);

} // namespace inspector

} // namespace ptgn::editor
