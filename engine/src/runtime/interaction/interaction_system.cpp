#pragma once

#include "core/event/event.h"
#include "core/math/vector2.h"

namespace ptgn {

namespace event {

struct GameResized : public Event<GameResized> {
	GameResized() = default;

	explicit GameResized(V2_int game_size) : size{ game_size } {}

	V2_int size;
};

} // namespace event

namespace impl {

namespace event {

struct InternalGameResized : public Event<InternalGameResized> {
	InternalGameResized() = default;

	explicit InternalGameResized(V2_int game_size) : size{ game_size } {}

	V2_int size;
};

struct InternalDisplayResized : public Event<InternalDisplayResized> {
	InternalDisplayResized() = default;

	explicit InternalDisplayResized(V2_int size) : size{ size } {}

	V2_int size;
};

} // namespace event

} // namespace impl

} // namespace ptgn