
#include <chrono>
#include <cmath>
#include <vector>

#include "app/application.h"
#include "core/math/angle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/polygon.h"
#include "core/math/math_utils.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 };

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
		ctx().renderer.DrawLines(
			{ { -375, -375 }, { -350, -350 }, { -325, -375 } }, color::Beige, 1.0f, false
		);
		ctx().renderer.DrawLines(
			{ { -300, -375 }, { -275, -350 }, { -250, -375 } }, color::Beige, 5.0f, false
		);
		ctx().renderer.DrawLines(
			{ { -225, -375 }, { -200, -350 }, { -175, -375 } }, color::Beige, 1.0f, true
		);
		ctx().renderer.DrawPoint({ -350, -300 }, color::Red);
		ctx().renderer.DrawLine({ -300, -300 }, { -250, -325 }, color::Orange, 1.0f);
		ctx().renderer.DrawLine({ -200, -325 }, { -150, -300 }, color::Yellow, 5.0f);
		ctx().renderer.DrawCapsule(
			{}, { { -275, -250 }, { -175, -250 }, 12.0f }, color::Yellow, 1.0f
		);
		ctx().renderer.DrawCapsule(
			{}, { { -300, -300 + 150 }, { -250, -350 + 150 }, 12.0f }, color::Orange, 5.0f
		);
		ctx().renderer.DrawCapsule(
			{}, { { -200, -350 + 150 }, { -150, -300 + 150 }, 12.0f }, color::LightGold, Solid{}
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

		ctx().renderer.DrawArc(
			V2_float{ -50, arc_y }, { arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightGreen, 1.0f
		);
		ctx().renderer.DrawArc(
			V2_float{ 0, arc_y }, { arc_radius, start_angle2, end_angle2, clockwise },
			color::BrightGreen, 1.0f
		);
		ctx().renderer.DrawArc(
			V2_float{ 50, arc_y }, { arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightGreen, 1.0f
		);
		ctx().renderer.DrawArc(
			V2_float{ 100, arc_y }, { arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightGreen, 1.0f
		);

		ctx().renderer.DrawArc(
			V2_float{ -50, arc_y + 50.0f }, { arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightPink, 5.0f
		);
		ctx().renderer.DrawArc(
			V2_float{ 0, arc_y + 50.0f }, { arc_radius, start_angle3, end_angle2, clockwise },
			color::BrightPink, 5.0f
		);
		ctx().renderer.DrawArc(
			V2_float{ 50, arc_y + 50.0f }, { arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightPink, 5.0f
		);
		ctx().renderer.DrawArc(
			V2_float{ 100, arc_y + 50.0f }, { arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightPink, 5.0f
		);

		ctx().renderer.DrawArc(
			V2_float{ -50, arc_y + 100.0f }, { arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightYellow, Solid{}
		);
		ctx().renderer.DrawArc(
			V2_float{ 0, arc_y + 100.0f }, { arc_radius, start_angle3, end_angle2, clockwise },
			color::BrightYellow, Solid{}
		);
		ctx().renderer.DrawArc(
			V2_float{ 50, arc_y + 100.0f }, { arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightYellow, Solid{}
		);
		ctx().renderer.DrawArc(
			V2_float{ 100, arc_y + 100.0f }, { arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightYellow, Solid{}
		);

		clockwise = false;

		ctx().renderer.DrawArc(
			V2_float{ -50, arc_y + 150.0f }, { arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightGreen, 1.0f
		);
		ctx().renderer.DrawArc(
			V2_float{ 0, arc_y + 150.0f }, { arc_radius, start_angle2, end_angle2, clockwise },
			color::BrightGreen, 1.0f
		);
		ctx().renderer.DrawArc(
			V2_float{ 50, arc_y + 150.0f }, { arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightGreen, 1.0f
		);
		ctx().renderer.DrawArc(
			V2_float{ 100, arc_y + 150.0f }, { arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightGreen, 1.0f
		);

		ctx().renderer.DrawArc(
			V2_float{ -50, arc_y + 200.0f }, { arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightPink, 5.0f
		);
		ctx().renderer.DrawArc(
			V2_float{ 0, arc_y + 200.0f }, { arc_radius, start_angle3, end_angle2, clockwise },
			color::BrightPink, 5.0f
		);
		ctx().renderer.DrawArc(
			V2_float{ 50, arc_y + 200.0f }, { arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightPink, 5.0f
		);
		ctx().renderer.DrawArc(
			V2_float{ 100, arc_y + 200.0f }, { arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightPink, 5.0f
		);

		ctx().renderer.DrawArc(
			V2_float{ -50, arc_y + 250.0f }, { arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightYellow, Solid{}
		);
		ctx().renderer.DrawArc(
			V2_float{ 0, arc_y + 250.0f }, { arc_radius, start_angle3, end_angle2, clockwise },
			color::BrightYellow, Solid{}
		);
		ctx().renderer.DrawArc(
			V2_float{ 50, arc_y + 250.0f }, { arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightYellow, Solid{}
		);
		ctx().renderer.DrawArc(
			V2_float{ 100, arc_y + 250.0f }, { arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightYellow, Solid{}
		);

		ctx().renderer.DrawRect(
			V2_int{ -50, -325 }, V2_int{ 50, 25 }, color::Blue, 1.0f, Origin::Center
		);
		ctx().renderer.DrawRect(
			V2_int{ 0, -325 }, V2_int{ 50, 25 }, color::LightBlue, Solid{}, Origin::TopLeft
		);
		ctx().renderer.DrawRect(
			V2_int{ 100, -325 }, V2_int{ 50, 25 }, color::DarkBlue, 5.0f, Origin::Center
		);

		float time{ static_cast<float>(ctx().TimeSinceStart().count()) };

		Radians rotation{ Degrees{ time / 10.0f } };

		ctx().renderer.DrawRect(
			{ { -50, -250 }, rotation }, V2_int{ 50, 25 }, color::Blue, 1.0f, Origin::Center
		);
		ctx().renderer.DrawRect(
			{ { 0, -250 }, rotation }, V2_int{ 50, 25 }, color::LightBlue, Solid{}, Origin::TopLeft

		);
		ctx().renderer.DrawRect(
			{ { 100, -250 }, rotation }, V2_int{ 50, 25 }, color::DarkBlue, 5.0f, Origin::Center

		);

		ctx().renderer.DrawRoundedRect(
			V2_int{ -50, -175 }, { { 50, 25 }, 12.0f }, color::Blue, 1.0f, Origin::Center
		);
		ctx().renderer.DrawRoundedRect(
			V2_int{ 0, -175 }, { { 50, 25 }, 12.0f }, color::LightBlue, Solid{}, Origin::TopLeft
		);
		ctx().renderer.DrawRoundedRect(
			{ { 100, -175 }, rotation }, { { 50, 25 }, 12.0f }, color::DarkBlue, 5.0f,
			Origin::Center
		);
		ctx().renderer.DrawRoundedRect(
			{ { -50, -100 }, rotation }, { { 50, 25 }, 12.0f }, color::Blue, 1.0f, Origin::Center

		);
		ctx().renderer.DrawRoundedRect(
			{ { 0, -100 }, rotation }, { { 50, 25 }, 12.0f }, color::LightBlue, Solid{},
			Origin::TopLeft

		);
		ctx().renderer.DrawRoundedRect(
			{ { 100, -100 }, rotation }, { { 50, 25 }, 12.0f }, color::DarkBlue, 5.0f,
			Origin::Center

		);

		ctx().renderer.DrawCircle(V2_int{ 200, -325 }, 25.0f, color::Gold, 1.0f);
		ctx().renderer.DrawCircle(V2_int{ 275, -325 }, 25.0f, color::DarkYellow, 5.0f);
		ctx().renderer.DrawCircle(V2_int{ 350, -325 }, 25.0f, color::LightYellow, Solid{});

		ctx().renderer.DrawEllipse(
			V2_int{ 200, -250 }, Ellipse{ V2_int{ 25, 12 } }, color::Purple, 1.0f
		);
		ctx().renderer.DrawEllipse(
			V2_int{ 275, -250 }, Ellipse{ V2_int{ 25, 12 } }, color::Magenta, 5.0f
		);
		ctx().renderer.DrawEllipse(
			V2_int{ 350, -250 }, Ellipse{ V2_int{ 25, 12 } }, color::LightPurple, Solid{}
		);
		ctx().renderer.DrawEllipse(
			{ V2_int{ 200, -175 }, rotation }, Ellipse{ V2_int{ 25, 12 } }, color::Green, 1.0f

		);
		ctx().renderer.DrawEllipse(
			{ V2_int{ 275, -175 }, rotation }, Ellipse{ V2_int{ 25, 12 } }, color::DarkGreen, 5.0f

		);
		ctx().renderer.DrawEllipse(
			{ V2_int{ 350, -175 }, rotation }, Ellipse{ V2_int{ 25, 12 } }, color::LightGreen,
			Solid{}

		);

		Polygon p{ GetStarVertices(5, 10, 20) };

		ctx().renderer.DrawShape(p, V2_int{ -225, -100 }, color::Cyan, 1.0f);
		ctx().renderer.DrawShape(p, V2_int{ -300, -100 }, color::Cyan, Solid{});
		ctx().renderer.DrawShape(p, V2_int{ -150, -100 }, color::Cyan, 5.0f);
	}
};

int main(int, char**) {
	Application app{ "ShapeScene", game_size };
	app.StartWith<ShapeScene>();
}