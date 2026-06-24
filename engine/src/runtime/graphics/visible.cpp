#include "runtime/graphics/visible.h"

#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/scene/scene_event.h"

namespace ptgn {

void SetVisible(Entity entity, bool visible, bool emit_visibility_event) {
	if (visible) {
		if (entity.Has<impl::Visible>()) {
			return;
		}
		entity.Add<impl::Visible>();
		if (emit_visibility_event && entity.HasScene()) {
			PushEvent<event::EntityShow>(entity, entity);
		}
	} else {
		if (!entity.Has<impl::Visible>()) {
			return;
		}
		entity.Remove<impl::Visible>();
		if (emit_visibility_event && entity.HasScene()) {
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

bool IsLocallyVisible(Entity entity) {
	return entity.Has<impl::Visible>();
}

bool IsVisible(Entity entity) {
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

void IgoreParentVisibility(Entity entity, bool ignore) {
	if (ignore) {
		entity.Add<impl::IgnoreParentVisibility>();
	} else {
		entity.Remove<impl::IgnoreParentVisibility>();
	}
}

} // namespace ptgn