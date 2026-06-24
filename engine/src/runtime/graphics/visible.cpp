#include "runtime/graphics/visible.h"

#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/scene/scene_event.h"

namespace ptgn {

namespace {

bool IsLocallyVisible(Entity entity) {
	return !entity.Has<impl::Visible>() || entity.Get<impl::Visible>().visible;
}

} // namespace

void SetVisible(Entity entity, bool visible, bool emit_visibility_event) {
	auto& visibility{ entity.TryAdd<impl::Visible>() };

	if (visibility.visible == visible) {
		return;
	}

	visibility.visible = visible;

	if (emit_visibility_event && entity.HasScene()) {
		if (visible) {
			PushEvent<event::EntityShow>(entity, entity);
		} else {
			PushEvent<event::EntityHide>(entity, entity);
		}
	}
}

void Show(Entity entity, bool emit_visibility_event) {
	SetVisible(entity, true, emit_visibility_event);
}

void Hide(Entity entity, bool emit_visibility_event) {
	SetVisible(entity, false, emit_visibility_event);
}

bool IsVisible(Entity entity, bool check_parent_visibility) {
	if (!check_parent_visibility) {
		return IsLocallyVisible(entity);
	}

	if (!IsLocallyVisible(entity)) {
		return false;
	}

	bool visible{ true };

	ForEachParent(
		entity, [](Entity e) { return e.Has<impl::IgnoreParentVisibility>(); },
		[&visible](Entity parent) {
			if (!IsLocallyVisible(parent)) {
				visible = false;
				return false;
			}

			return true;
		}
	);

	return visible;
}

void IgnoreParentVisibility(Entity entity, bool ignore) {
	if (ignore) {
		entity.Add<impl::IgnoreParentVisibility>();
	} else {
		entity.Remove<impl::IgnoreParentVisibility>();
	}
}

} // namespace ptgn