#include "Utilities.h"

#include "Vec2D.h"
#include "AABB.h"
#include "SDL.h"

namespace Util {
	double truncate(double value, int digits) {
		std::stringstream stream;
		stream << std::fixed << std::setprecision(digits) << value;
		return std::stod(stream.str());
	}

	SDL_Rect RectFromPoints(double x1, double y1, double x2, double y2) {
		return SDL_Rect{ static_cast<int>(round(x1)), static_cast<int>(round(y1)), static_cast<int>(round(x2)), static_cast<int>(round(y2)) };
	}

	SDL_Rect RectFromVec(const Vec2D& position, const Vec2D& size) {
		return SDL_Rect{ static_cast<int>(round(position.x)), static_cast<int>(round(position.y)), static_cast<int>(round(size.x)), static_cast<int>(round(size.y)) };
	}

	SDL_Rect RectFromAABB(const AABB& aabb) {
		return { static_cast<int>(round(aabb.position.x)), static_cast<int>(round(aabb.position.y)), static_cast<int>(round(aabb.size.x)), static_cast<int>(round(aabb.size.y)) };
	}
}