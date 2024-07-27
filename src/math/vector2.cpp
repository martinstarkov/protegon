#include "protegon/vector2.h"

#include "SDL.h"
#include "protegon/circle.h"
#include "protegon/game.h"
#include "protegon/line.h"

namespace ptgn {

template <>
Vector2<int>::operator SDL_Point() const {
	return SDL_Point{ x, y };
}

} // namespace ptgn