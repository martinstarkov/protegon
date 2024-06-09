#include "protegon/vector2.h"

#include <SDL.h>

#include "protegon/line.h"
#include "protegon/circle.h"

#include "core/game.h"

namespace ptgn {

Vector2<int>::operator SDL_Point() const {
	return SDL_Point{ x, y };
}

namespace impl {

void DrawPointWrapper(int x, int y, const Color& color) {
	impl::DrawPoint(x, y, color);
}

void DrawSolidCircleWrapper(int x, int y, int r, const Color& color) {
	impl::DrawSolidCircle(x, y, r, color);
}

} // namespace impl

} // namespace ptgn