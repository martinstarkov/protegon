

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/math/angle.h"
#include "core/math/vector2.h"
#include "runtime/graphics/shape.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

struct ArcScene : public Scene {
	static constexpr float arc_radius{ 25.0f };

	void CreateArcWithCircleBg(
		V2_float pos, Degrees start_angle_deg, Degrees end_angle_deg, bool clockwise, Color color,
		float radius = arc_radius
	) {
		CreateCircle(*this, pos, radius, color::LightGray);
		CreateArc(*this, pos, radius, start_angle_deg, end_angle_deg, clockwise, color);
	}

	void OnEnter() override {
		float separation{ arc_radius * 2.0f + 2.5f };

		V2_float offset{ 0, separation };

		CreateArcWithCircleBg({ offset.x, -6 * offset.y }, 0.0f, 0.0f, false, color::Red);
		CreateArcWithCircleBg({ offset.x, -5 * offset.y }, 0.0f, 45.0f, false, color::Red);
		CreateArcWithCircleBg({ offset.x, -4 * offset.y }, 45.0f, 90.0f, false, color::Red);
		CreateArcWithCircleBg({ offset.x, -3 * offset.y }, 90.0f, 135.0f, false, color::Red);
		CreateArcWithCircleBg({ offset.x, -2 * offset.y }, 135.0f, 180.0f, false, color::Red);
		CreateArcWithCircleBg({ offset.x, -1 * offset.y }, 180.0f, 225.0f, false, color::Red);
		CreateArcWithCircleBg({ offset.x, 0 * offset.y }, 225.0f, 270.0f, false, color::Red);
		CreateArcWithCircleBg({ offset.x, 1 * offset.y }, 270.0f, 315.0f, false, color::Red);
		CreateArcWithCircleBg({ offset.x, 2 * offset.y }, 315.0f, 360.0f, false, color::Red);
		CreateArcWithCircleBg({ offset.x, 3 * offset.y }, 360.0f, 45.0f, false, color::Red);
		CreateArcWithCircleBg({ offset.x, 4 * offset.y }, 360.0f, 0.0f, false, color::Red);
		CreateArcWithCircleBg({ offset.x, 5 * offset.y }, 0.0f, 360.0f, false, color::Red);
		CreateArcWithCircleBg({ offset.x, 6 * offset.y }, 360.0f, 360.0f, false, color::Red);

		offset.x += separation;

		CreateArcWithCircleBg({ offset.x, -6 * offset.y }, 0.0f, 0.0f, true, color::Green);
		CreateArcWithCircleBg({ offset.x, -5 * offset.y }, 0.0f, 45.0f, true, color::Green);
		CreateArcWithCircleBg({ offset.x, -4 * offset.y }, 45.0f, 90.0f, true, color::Green);
		CreateArcWithCircleBg({ offset.x, -3 * offset.y }, 90.0f, 135.0f, true, color::Green);
		CreateArcWithCircleBg({ offset.x, -2 * offset.y }, 135.0f, 180.0f, true, color::Green);
		CreateArcWithCircleBg({ offset.x, -1 * offset.y }, 180.0f, 225.0f, true, color::Green);
		CreateArcWithCircleBg({ offset.x, 0 * offset.y }, 225.0f, 270.0f, true, color::Green);
		CreateArcWithCircleBg({ offset.x, 1 * offset.y }, 270.0f, 315.0f, true, color::Green);
		CreateArcWithCircleBg({ offset.x, 2 * offset.y }, 315.0f, 360.0f, true, color::Green);
		CreateArcWithCircleBg({ offset.x, 3 * offset.y }, 360.0f, 45.0f, true, color::Green);
		CreateArcWithCircleBg({ offset.x, 4 * offset.y }, 360.0f, 0.0f, true, color::Green);
		CreateArcWithCircleBg({ offset.x, 5 * offset.y }, 0.0f, 360.0f, true, color::Green);
		CreateArcWithCircleBg({ offset.x, 6 * offset.y }, 360.0f, 360.0f, true, color::Green);
	}
};

int main(int, char**) {
	Application app{ "ArcScene" };
	app.StartWith<ArcScene>();
}