#include "components/draw.h"

#include <string_view>
#include <utility>

#include "core/game.h"
#include "ecs/ecs.h"
#include "math/math.h"
#include "math/vector2.h"
#include "transform.h"
#include "utility/assert.h"
#include "utility/time.h"
#include "utility/tween.h"
#include "utility/utility.h"

namespace ptgn {

ecs::Entity CreateSprite(
	ecs::Manager& manager, std::string_view texture_key, const V2_float& position
) {
	PTGN_ASSERT(
		game.texture.Has(texture_key),
		"Cannot create sprite with texture key that has not been loaded in the texture manager"
	);
	auto sprite = manager.CreateEntity();
	sprite.Add<Transform>(position);
	sprite.Add<Sprite>(texture_key);
	sprite.Add<Visible>();
	return sprite;
}

Animation::Animation(
	ecs::Entity parent, std::string_view texture_key, std::size_t frame_count,
	const V2_float& frame_size, milliseconds animation_duration, const V2_float& start_pixel,
	std::size_t starting_frame
) :
	impl::SpriteSheet{ texture_key, frame_count, frame_size, start_pixel } {
	duration	= animation_duration;
	start_frame = starting_frame;
	parent_		= parent;

	current_frame = start_frame;
	repeat		  = 0;

	PTGN_ASSERT(start_frame < GetCount(), "Start frame must be within sprite sheet frame count");

	milliseconds frame_duration{ duration / GetCount() };

	entity_.Destroy();
	entity_ = parent.GetManager().CreateEntity();

	entity_.Add<Tween>()
		.During(frame_duration)
		.Repeat(-1)
		.OnStart([=]() {
			auto& anim{ parent_.Get<Animation>() };
			Invoke(anim.on_start);
		})
		.OnRepeat([=]() {
			auto& anim{ parent_.Get<Animation>() };
			Invoke(anim.on_repeat);
			++anim.repeat;
			++anim.current_frame;
			anim.current_frame = Mod(anim.current_frame, GetCount());
		})
		.OnReset([=]() {
			auto& anim{ parent_.Get<Animation>() };
			anim.current_frame = anim.start_frame;
		})
		.OnUpdate([=](float t) {
			auto& anim{ parent_.Get<Animation>() };
			Invoke(anim.on_update, t);
		});
}

Animation::Animation(Animation&& other) noexcept :
	on_start{ std::exchange(other.on_start, {}) },
	on_repeat{ std::exchange(other.on_repeat, {}) },
	on_update{ std::exchange(other.on_update, {}) },
	duration{ std::exchange(other.duration, milliseconds{ 0 }) },
	repeat{ std::exchange(other.repeat, 0) },
	current_frame{ std::exchange(other.current_frame, 0) },
	start_frame{ std::exchange(other.start_frame, 0) },
	entity_{ std::exchange(other.entity_, {}) } {}

Animation& Animation::operator=(Animation&& other) noexcept {
	if (this != &other) {
		on_start	  = std::exchange(other.on_start, {});
		on_repeat	  = std::exchange(other.on_repeat, {});
		on_update	  = std::exchange(other.on_update, {});
		duration	  = std::exchange(other.duration, milliseconds{ 0 });
		repeat		  = std::exchange(other.repeat, 0);
		current_frame = std::exchange(other.current_frame, 0);
		start_frame	  = std::exchange(other.start_frame, 0);
		entity_		  = std::exchange(other.entity_, {});
	}
	return *this;
}

Animation::~Animation() {
	entity_.Destroy();
}

} // namespace ptgn