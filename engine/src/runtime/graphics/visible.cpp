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

void SetVisible(Entity entity, bool visible) {
	if (entity.Has<impl::Visible>() && entity.Get<impl::Visible>().visible == visible) {
		return;
	}

	auto& visibility{ entity.TryAdd<impl::Visible>() };
	visibility.visible = visible;

	if (entity.HasScene()) {
		if (visible) {
			PushEvent<event::EntityShow>(entity, entity);
		} else {
			PushEvent<event::EntityHide>(entity, entity);
		}
	}
}

void Show(Entity entity) {
	SetVisible(entity, true);
}

void Hide(Entity entity) {
	SetVisible(entity, false);
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