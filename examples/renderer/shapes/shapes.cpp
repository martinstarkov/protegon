
#include <chrono>
#include <cmath>
#include <vector>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/angle.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/math_utils.h"
#include "core/math/vector2.h"
#include "runtime/graphics/render_queue.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

struct ShapeScene : public Scene {
	std::vector<V2_float> GetStarVertices(int count, float outer_radius, float inner_radius) const {
		std::vector<V2_float> vertices;
		float angleStep = kPi / static_cast<float>(count); // Half angle between full points

		for (int i = 0; i < 2 * count; ++i) {
			float r = (i % 2 == 0) ? outer_radius : inner_radius;
			// Rotate so the first point is at the top
			float theta = static_cast<float>(i) * angleStep - kHalfPi;
			float x		= r * cos(theta);
			float y		= r * sin(theta);
			vertices.push_back({ x, y });
		}

		return vertices;
	}

	void OnUpdate() override {
		ctx().render_queue.DrawPoint({ -350, -300 }, color::Red);

		ctx().render_queue.DrawLine(
			{ -300, -300 }, { -250, -325 }, color::Orange, { .fill_style = 1.0f }
		);
		ctx().render_queue.DrawLine(
			{ -200, -325 }, { -150, -300 }, color::Yellow, { .fill_style = 5.0f }
		);

		ctx().render_queue.DrawLines(
			std::vector<V2_float>{ { -375, -375 }, { -350, -350 }, { -325, -375 } }, color::Beige,
			{ .fill_style = 1.0f }, false
		);
		ctx().render_queue.DrawLines(
			std::vector<V2_float>{ { -300, -375 }, { -275, -350 }, { -250, -375 } }, color::Beige,
			{ .fill_style = 5.0f }, false
		);
		ctx().render_queue.DrawLines(
			std::vector<V2_float>{ { -225, -375 }, { -200, -350 }, { -175, -375 } }, color::Beige,
			{ .fill_style = 1.0f }, true
		);

		ctx().render_queue.DrawShape(
			{}, Capsule{ { -275, -250 }, { -175, -250 }, 12.0f }, color::Yellow,
			{ .fill_style = 1.0f }
		);
		ctx().render_queue.DrawShape(
			{}, Capsule{ { -300, -300 + 150 }, { -250, -350 + 150 }, 12.0f }, color::Orange,
			{ .fill_style = 5.0f }
		);
		ctx().render_queue.DrawShape(
			{}, Capsule{ { -200, -350 + 150 }, { -150, -300 + 150 }, 12.0f }, color::LightGold,
			{ .fill_style = Solid{} }
		);

		constexpr Degrees start_angle1{ 0.0f };
		constexpr Degrees end_angle1{ 180.0f };
		constexpr Degrees start_angle2{ 180.0f };
		constexpr Degrees end_angle2{ 0.0f };
		constexpr Degrees start_angle3{ -180.0f };
		constexpr Degrees end_angle3{ 90.0f };
		constexpr Degrees start_angle4{ -90.0f };
		constexpr Degrees end_angle4{ 269.0f };

		float arc_radius{ 20.0f };
		bool clockwise{ true };

		float arc_y{ -10 };

		ctx().render_queue.DrawShape(
			V2_float{ -50, arc_y }, Arc{ arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightGreen, { .fill_style = 1.0f }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 0, arc_y }, Arc{ arc_radius, start_angle2, end_angle2, clockwise },
			color::BrightGreen, { .fill_style = 1.0f }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 50, arc_y }, Arc{ arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightGreen, { .fill_style = 1.0f }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 100, arc_y }, Arc{ arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightGreen, { .fill_style = 1.0f }
		);

		ctx().render_queue.DrawShape(
			V2_float{ -50, arc_y + 50.0f }, Arc{ arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightPink, { .fill_style = 5.0f }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 0, arc_y + 50.0f }, Arc{ arc_radius, start_angle3, end_angle2, clockwise },
			color::BrightPink, { .fill_style = 5.0f }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 50, arc_y + 50.0f }, Arc{ arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightPink, { .fill_style = 5.0f }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 100, arc_y + 50.0f }, Arc{ arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightPink, { .fill_style = 5.0f }
		);

		ctx().render_queue.DrawShape(
			V2_float{ -50, arc_y + 100.0f }, Arc{ arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightYellow, { .fill_style = Solid{} }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 0, arc_y + 100.0f }, Arc{ arc_radius, start_angle3, end_angle2, clockwise },
			color::BrightYellow, { .fill_style = Solid{} }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 50, arc_y + 100.0f }, Arc{ arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightYellow, { .fill_style = Solid{} }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 100, arc_y + 100.0f }, Arc{ arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightYellow, { .fill_style = Solid{} }
		);

		clockwise = false;

		ctx().render_queue.DrawShape(
			V2_float{ -50, arc_y + 150.0f }, Arc{ arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightGreen, { .fill_style = 1.0f }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 0, arc_y + 150.0f }, Arc{ arc_radius, start_angle2, end_angle2, clockwise },
			color::BrightGreen, { .fill_style = 1.0f }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 50, arc_y + 150.0f }, Arc{ arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightGreen, { .fill_style = 1.0f }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 100, arc_y + 150.0f }, Arc{ arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightGreen, { .fill_style = 1.0f }
		);

		ctx().render_queue.DrawShape(
			V2_float{ -50, arc_y + 200.0f }, Arc{ arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightPink, { .fill_style = 5.0f }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 0, arc_y + 200.0f }, Arc{ arc_radius, start_angle3, end_angle2, clockwise },
			color::BrightPink, { .fill_style = 5.0f }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 50, arc_y + 200.0f }, Arc{ arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightPink, { .fill_style = 5.0f }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 100, arc_y + 200.0f }, Arc{ arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightPink, { .fill_style = 5.0f }
		);

		ctx().render_queue.DrawShape(
			V2_float{ -50, arc_y + 250.0f }, Arc{ arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightYellow, { .fill_style = Solid{} }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 0, arc_y + 250.0f }, Arc{ arc_radius, start_angle3, end_angle2, clockwise },
			color::BrightYellow, { .fill_style = Solid{} }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 50, arc_y + 250.0f }, Arc{ arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightYellow, { .fill_style = Solid{} }
		);
		ctx().render_queue.DrawShape(
			V2_float{ 100, arc_y + 250.0f }, Arc{ arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightYellow, { .fill_style = Solid{} }
		);

		ctx().render_queue.DrawShape(
			V2_int{ -50, -325 }, Rect{ 50, 25 }, color::Blue, { .fill_style = 1.0f }
		);
		ctx().render_queue.DrawShape(
			V2_int{ 0, -325 }, Rect{ 50, 25 }, color::LightBlue,
			{ .fill_style = Solid{}, .origin = Origin::TopLeft }
		);
		ctx().render_queue.DrawShape(
			V2_int{ 100, -325 }, Rect{ 50, 25 }, color::DarkBlue, { .fill_style = 5.0f }
		);

		float time{ static_cast<float>(ctx().TimeSinceStart().count()) };

		Radians rotation{ Degrees{ time / 10.0f } };

		ctx().render_queue.DrawShape(
			{ { -50, -250 }, rotation }, Rect{ 50, 25 }, color::Blue, { .fill_style = 1.0f }
		);
		ctx().render_queue.DrawShape(
			{ { 0, -250 }, rotation }, Rect{ 50, 25 }, color::LightBlue,
			{ .fill_style = Solid{}, .origin = Origin::TopLeft }

		);
		ctx().render_queue.DrawShape(
			{ { 100, -250 }, rotation }, Rect{ 50, 25 }, color::DarkBlue, { .fill_style = 5.0f }

		);

		ctx().render_queue.DrawShape(
			V2_int{ -50, -175 }, RoundedRect{ { 50, 25 }, 12.0f }, color::Blue,
			{ .fill_style = 1.0f }
		);
		ctx().render_queue.DrawShape(
			V2_int{ 0, -175 }, RoundedRect{ { 50, 25 }, 12.0f }, color::LightBlue,
			{ .fill_style = Solid{}, .origin = Origin::TopLeft }
		);
		ctx().render_queue.DrawShape(
			{ { 100, -175 }, rotation }, RoundedRect{ { 50, 25 }, 12.0f }, color::DarkBlue,
			{ .fill_style = 5.0f }
		);
		ctx().render_queue.DrawShape(
			{ { -50, -100 }, rotation }, RoundedRect{ { 50, 25 }, 12.0f }, color::Blue,
			{ .fill_style = 1.0f }

		);
		ctx().render_queue.DrawShape(
			{ { 0, -100 }, rotation }, RoundedRect{ { 50, 25 }, 12.0f }, color::LightBlue,
			{ .fill_style = Solid{}, .origin = Origin::TopLeft }

		);
		ctx().render_queue.DrawShape(
			{ { 100, -100 }, rotation }, RoundedRect{ { 50, 25 }, 12.0f }, color::DarkBlue,
			{ .fill_style = 5.0f }

		);

		ctx().render_queue.DrawShape(
			V2_int{ 200, -325 }, Circle{ 25.0f }, color::Gold, { .fill_style = 1.0f }
		);
		ctx().render_queue.DrawShape(
			V2_int{ 275, -325 }, Circle{ 25.0f }, color::DarkYellow, { .fill_style = 5.0f }
		);
		ctx().render_queue.DrawShape(
			V2_int{ 350, -325 }, Circle{ 25.0f }, color::LightYellow, { .fill_style = Solid{} }
		);

		ctx().render_queue.DrawShape(
			V2_int{ 200, -250 }, Ellipse{ V2_int{ 25, 12 } }, color::Purple, { .fill_style = 1.0f }
		);
		ctx().render_queue.DrawShape(
			V2_int{ 275, -250 }, Ellipse{ V2_int{ 25, 12 } }, color::Magenta, { .fill_style = 5.0f }
		);
		ctx().render_queue.DrawShape(
			V2_int{ 350, -250 }, Ellipse{ V2_int{ 25, 12 } }, color::LightPurple,
			{ .fill_style = Solid{} }
		);
		ctx().render_queue.DrawShape(
			{ V2_int{ 200, -175 }, rotation }, Ellipse{ V2_int{ 25, 12 } }, color::Green,
			{ .fill_style = 1.0f }

		);
		ctx().render_queue.DrawShape(
			{ V2_int{ 275, -175 }, rotation }, Ellipse{ V2_int{ 25, 12 } }, color::DarkGreen,
			{ .fill_style = 5.0f }

		);
		ctx().render_queue.DrawShape(
			{ V2_int{ 350, -175 }, rotation }, Ellipse{ V2_int{ 25, 12 } }, color::LightGreen,
			{ .fill_style = Solid{} }

		);

		Polygon p{ GetStarVertices(5, 10, 20) };

		ctx().render_queue.DrawShape(V2_int{ -225, -100 }, p, color::Cyan, { .fill_style = 1.0f });
		ctx().render_queue.DrawShape(
			V2_int{ -300, -100 }, p, color::Cyan, { .fill_style = Solid{} }
		);
		ctx().render_queue.DrawShape(V2_int{ -150, -100 }, p, color::Cyan, { .fill_style = 5.0f });
	}
};

int main(int, char**) {
	Application app{ "ShapeScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<ShapeScene>();
}