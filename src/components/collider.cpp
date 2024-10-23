#include "components/collider.h"

#include "components/sprite.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "math/geometry/polygon.h"

namespace ptgn {

Rect BoxCollider::GetAbsoluteRect() const {
	PTGN_ASSERT(parent.IsAlive());
	PTGN_ASSERT(parent.Has<Transform>());

	Transform transform = parent.Get<Transform>();

	// If parent has an animation, use coordinate relative to top left.
	if (parent.Has<Animation>()) {
		const Animation& anim = parent.Get<Animation>();
		Rect r{ transform.position, anim.sprite_size, anim.origin };
		transform.position = r.Min();
	} else if (parent.Has<Sprite>()) { // Prioritize animations over sprites.
		const Sprite& sprite = parent.Get<Sprite>();
		Rect source{ sprite.GetSource() };
		source.position	   = transform.position;
		transform.position = source.Min();
	}

	Rect rect	   = GetRelativeRect();
	rect.position += transform.position;
	rect.rotation += transform.rotation;
	rect.size	  *= transform.scale;
	return rect;
}

} // namespace ptgn