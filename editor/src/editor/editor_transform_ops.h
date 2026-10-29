#pragma once

#include <optional>

#include "core/math/transform.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/slider.h"

namespace ptgn::editor {

// A single authored transform edit can also change managed button visuals. Preserve
// both the ordinary Transform and those visuals so undo restores the actual drawing.
struct EditorEntityTransformSnapshot {
	Transform local{};
	Transform world{};
	std::optional<ButtonBackgroundVisuals> backgrounds{};
	std::optional<ButtonBorderVisuals> borders{};
	std::optional<ButtonTextVisuals> texts{};
	std::optional<ButtonSpriteVisuals> sprites{};
};

template <typename T>
[[nodiscard]] std::optional<T> CaptureEditorTransformVisual(Entity entity) {
	if (entity && entity.Has<T>()) {
		return entity.Get<T>();
	}
	return std::nullopt;
}

[[nodiscard]] inline EditorEntityTransformSnapshot CaptureEditorEntityTransform(Entity entity) {
	EditorEntityTransformSnapshot snapshot{};
	if (!entity || !entity.Has<Transform>()) {
		return snapshot;
	}
	snapshot.local		 = GetTransform(entity);
	snapshot.world		 = GetWorldTransform(entity);
	snapshot.backgrounds = CaptureEditorTransformVisual<ButtonBackgroundVisuals>(entity);
	snapshot.borders	 = CaptureEditorTransformVisual<ButtonBorderVisuals>(entity);
	snapshot.texts		 = CaptureEditorTransformVisual<ButtonTextVisuals>(entity);
	snapshot.sprites	 = CaptureEditorTransformVisual<ButtonSpriteVisuals>(entity);
	return snapshot;
}

template <typename T>
inline void RestoreEditorTransformVisual(Entity entity, const std::optional<T>& value) {
	if (value && entity.Has<T>()) {
		entity.Get<T>() = *value;
	}
}

inline void RefreshEditorTransformDependents(Entity entity) {
	if (!entity) {
		return;
	}

	if (Entity parent{ GetParent(entity) }; parent && parent.Has<::ptgn::impl::ButtonData>()) {
		auto& data{ parent.Get<::ptgn::impl::ButtonData>() };
		if (entity.Has<ButtonBackgroundVisuals>()) {
			data.dirty |= ::ptgn::impl::ButtonDirty::Background;
		}
		if (entity.Has<ButtonBorderVisuals>()) {
			data.dirty |= ::ptgn::impl::ButtonDirty::Border;
		}
		if (entity.Has<ButtonTextVisuals>()) {
			data.dirty |= ::ptgn::impl::ButtonDirty::Text;
		}
		if (entity.Has<ButtonSpriteVisuals>()) {
			data.dirty |= ::ptgn::impl::ButtonDirty::Sprite;
		}
	}

	::ptgn::impl::SliderSystem::SynchronizeEntity(entity);
}

inline void ApplyEditorEntityTransform(Entity entity, const EditorEntityTransformSnapshot& value) {
	if (!entity || !entity.Has<Transform>()) {
		return;
	}
	SetTransform(entity, value.local);
	RestoreEditorTransformVisual(entity, value.backgrounds);
	RestoreEditorTransformVisual(entity, value.borders);
	RestoreEditorTransformVisual(entity, value.texts);
	RestoreEditorTransformVisual(entity, value.sprites);
	RefreshEditorTransformDependents(entity);
}

template <typename Visuals>
inline void TranslateEditorVisualState(std::optional<Visuals>& visuals, V2_float local_delta) {
	if (!visuals) {
		return;
	}
	for (auto& state : visuals->states) {
		if (state.transform) {
			state.transform->position += local_delta;
		}
	}
}

// Computes each update from the drag starting snapshot rather than from the prior
// frame. This prevents accumulated drift and keeps managed visuals in sync with
// an entity's local transform when its parent is rotated, scaled, or also moved.
inline void MoveEditorEntityFromSnapshot(
	Entity entity, const EditorEntityTransformSnapshot& initial, V2_float world_delta
) {
	if (!entity || !entity.Has<Transform>()) {
		return;
	}
	EditorEntityTransformSnapshot next{ initial };
	next.world.position += world_delta;
	SetWorldTransform(entity, next.world);

	V2_float local_delta{ GetTransform(entity).position - initial.local.position };
	TranslateEditorVisualState(next.backgrounds, local_delta);
	TranslateEditorVisualState(next.borders, local_delta);
	TranslateEditorVisualState(next.texts, local_delta);
	TranslateEditorVisualState(next.sprites, local_delta);

	RestoreEditorTransformVisual(entity, next.backgrounds);
	RestoreEditorTransformVisual(entity, next.borders);
	RestoreEditorTransformVisual(entity, next.texts);
	RestoreEditorTransformVisual(entity, next.sprites);
	RefreshEditorTransformDependents(entity);
}

} // namespace ptgn::editor
