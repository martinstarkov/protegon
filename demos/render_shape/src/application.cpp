
#include "components/draw.h"
#include "core/game.h"
#include "core/window.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

struct ShapeScene : public Scene {
	void Enter() override {
		game.window.SetResizable();
	}

	std::vector<V2_float> GetStarVertices(int count, float outer_radius, float inner_radius) {
		std::vector<V2_float> vertices;
		float angleStep = pi<float> / count; // Half angle between full points

		for (int i = 0; i < 2 * count; ++i) {
			float r		= (i % 2 == 0) ? outer_radius : inner_radius;
			float theta = i * angleStep - half_pi<float>; // Rotate so the first point is at the top
			float x		= r * cos(theta);
			float y		= r * sin(theta);
			vertices.push_back({ x, y });
		}

		return vertices;
	}

	void Update() override {
		DrawDebugLines(
			{ { -375, -375 }, { -350, -350 }, { -325, -375 } }, color::Beige, 1.0f, false
		);
		DrawDebugLines(
			{ { -300, -375 }, { -275, -350 }, { -250, -375 } }, color::Beige, 5.0f, false
		);
		DrawDebugLines(
			{ { -225, -375 }, { -200, -350 }, { -175, -375 } }, color::Beige, 1.0f, true
		);
		DrawDebugPoint({ -350, -300 }, color::Red);
		DrawDebugLine({ -300, -300 }, { -250, -325 }, color::Orange, 1.0f);
		DrawDebugLine({ -200, -325 }, { -150, -300 }, color::Yellow, 5.0f);
		DrawDebugCapsule({ -275, -250 }, { -175, -250 }, 12.0f, color::Yellow, 1.0f);
		DrawDebugCapsule({ -300, -300 + 150 }, { -250, -350 + 150 }, 12.0f, color::Orange, 5.0f);
		DrawDebugCapsule(
			{ -200, -350 + 150 }, { -150, -300 + 150 }, 12.0f, color::LightGold, -1.0f
		);

		float start_angle1{ DegToRad(0.0f) };
		float end_angle1{ DegToRad(180.0f) };
		float start_angle2{ DegToRad(180.0f) };
		float end_angle2{ DegToRad(0.0f) };
		float start_angle3{ DegToRad(-180.0f) };
		float end_angle3{ DegToRad(90.0f) };
		float start_angle4{ DegToRad(-90.0f) };
		float end_angle4{ DegToRad(269.0f) };

		float arc_radius{ 20.0f };
		bool clockwise{ true };

		float arc_y{ -10 };

		DrawDebugArc(
			{ -50, arc_y }, arc_radius, start_angle1, end_angle1, color::BrightGreen, 1.0f,
			clockwise
		);
		DrawDebugArc(
			{ 0, arc_y }, arc_radius, start_angle2, end_angle2, color::BrightGreen, 1.0f, clockwise
		);
		DrawDebugArc(
			{ 50, arc_y }, arc_radius, start_angle3, end_angle3, color::BrightGreen, 1.0f, clockwise
		);
		DrawDebugArc(
			{ 100, arc_y }, arc_radius, start_angle4, end_angle4, color::BrightGreen, 1.0f,
			clockwise
		);

		DrawDebugArc(
			{ -50, arc_y + 50.0f }, arc_radius, start_angle1, end_angle1, color::BrightPink, 5.0f,
			clockwise
		);
		DrawDebugArc(
			{ 0, arc_y + 50.0f }, arc_radius, start_angle3, end_angle2, color::BrightPink, 5.0f,
			clockwise
		);
		DrawDebugArc(
			{ 50, arc_y + 50.0f }, arc_radius, start_angle3, end_angle3, color::BrightPink, 5.0f,
			clockwise
		);
		DrawDebugArc(
			{ 100, arc_y + 50.0f }, arc_radius, start_angle4, end_angle4, color::BrightPink, 5.0f,
			clockwise
		);

		DrawDebugArc(
			{ -50, arc_y + 100.0f }, arc_radius, start_angle1, end_angle1, color::BrightYellow,
			-1.0f, clockwise
		);
		DrawDebugArc(
			{ 0, arc_y + 100.0f }, arc_radius, start_angle3, end_angle2, color::BrightYellow, -1.0f,
			clockwise
		);
		DrawDebugArc(
			{ 50, arc_y + 100.0f }, arc_radius, start_angle3, end_angle3, color::BrightYellow,
			-1.0f, clockwise
		);
		DrawDebugArc(
			{ 100, arc_y + 100.0f }, arc_radius, start_angle4, end_angle4, color::BrightYellow,
			-1.0f, clockwise
		);

		clockwise = false;

		DrawDebugArc(
			{ -50, arc_y + 150.0f }, arc_radius, start_angle1, end_angle1, color::BrightGreen, 1.0f,
			clockwise
		);
		DrawDebugArc(
			{ 0, arc_y + 150.0f }, arc_radius, start_angle2, end_angle2, color::BrightGreen, 1.0f,
			clockwise
		);
		DrawDebugArc(
			{ 50, arc_y + 150.0f }, arc_radius, start_angle3, end_angle3, color::BrightGreen, 1.0f,
			clockwise
		);
		DrawDebugArc(
			{ 100, arc_y + 150.0f }, arc_radius, start_angle4, end_angle4, color::BrightGreen, 1.0f,
			clockwise
		);

		DrawDebugArc(
			{ -50, arc_y + 200.0f }, arc_radius, start_angle1, end_angle1, color::BrightPink, 5.0f,
			clockwise
		);
		DrawDebugArc(
			{ 0, arc_y + 200.0f }, arc_radius, start_angle3, end_angle2, color::BrightPink, 5.0f,
			clockwise
		);
		DrawDebugArc(
			{ 50, arc_y + 200.0f }, arc_radius, start_angle3, end_angle3, color::BrightPink, 5.0f,
			clockwise
		);
		DrawDebugArc(
			{ 100, arc_y + 200.0f }, arc_radius, start_angle4, end_angle4, color::BrightPink, 5.0f,
			clockwise
		);

		DrawDebugArc(
			{ -50, arc_y + 250.0f }, arc_radius, start_angle1, end_angle1, color::BrightYellow,
			-1.0f, clockwise
		);
		DrawDebugArc(
			{ 0, arc_y + 250.0f }, arc_radius, start_angle3, end_angle2, color::BrightYellow, -1.0f,
			clockwise
		);
		DrawDebugArc(
			{ 50, arc_y + 250.0f }, arc_radius, start_angle3, end_angle3, color::BrightYellow,
			-1.0f, clockwise
		);
		DrawDebugArc(
			{ 100, arc_y + 250.0f }, arc_radius, start_angle4, end_angle4, color::BrightYellow,
			-1.0f, clockwise
		);

		DrawDebugRect({ -50, -325 }, { 50, 25 }, color::Blue, Origin::Center, 1.0f, 0.0f);
		DrawDebugRect({ 0, -325 }, { 50, 25 }, color::LightBlue, Origin::TopLeft, -1.0f, 0.0f);
		DrawDebugRect({ 100, -325 }, { 50, 25 }, color::DarkBlue, Origin::Center, 5.0f, 0.0f);
		DrawDebugRect(
			{ -50, -250 }, { 50, 25 }, color::Blue, Origin::Center, 1.0f,
			DegToRad(game.time() / 10.0f)
		);
		DrawDebugRect(
			{ 0, -250 }, { 50, 25 }, color::LightBlue, Origin::TopLeft, -1.0f,
			DegToRad(game.time() / 10.0f)
		);
		DrawDebugRect(
			{ 100, -250 }, { 50, 25 }, color::DarkBlue, Origin::Center, 5.0f,
			DegToRad(game.time() / 10.0f)
		);

		DrawDebugRoundedRect(
			{ -50, -175 }, { 50, 25 }, 12.0f, color::Blue, Origin::Center, 1.0f, 0.0f
		);
		DrawDebugRoundedRect(
			{ 0, -175 }, { 50, 25 }, 12.0f, color::LightBlue, Origin::TopLeft, -1.0f, 0.0f
		);
		DrawDebugRoundedRect(
			{ 100, -175 }, { 50, 25 }, 12.0f, color::DarkBlue, Origin::Center, 5.0f, 0.0f
		);
		DrawDebugRoundedRect(
			{ -50, -100 }, { 50, 25 }, 12.0f, color::Blue, Origin::Center, 1.0f,
			DegToRad(game.time() / 10.0f)
		);
		DrawDebugRoundedRect(
			{ 0, -100 }, { 50, 25 }, 12.0f, color::LightBlue, Origin::TopLeft, -1.0f,
			DegToRad(game.time() / 10.0f)
		);
		DrawDebugRoundedRect(
			{ 100, -100 }, { 50, 25 }, 12.0f, color::DarkBlue, Origin::Center, 5.0f,
			DegToRad(game.time() / 10.0f)
		);

		DrawDebugCircle({ 200, -325 }, 25.0f, color::Gold, 1.0f);
		DrawDebugCircle({ 275, -325 }, 25.0f, color::DarkYellow, 5.0f);
		DrawDebugCircle({ 350, -325 }, 25.0f, color::LightYellow, -1.0f);
		DrawDebugEllipse({ 200, -250 }, { 25, 12 }, color::Purple, 1.0f, 0.0f);
		DrawDebugEllipse({ 275, -250 }, { 25, 12 }, color::Magenta, 5.0f, 0.0f);
		DrawDebugEllipse({ 350, -250 }, { 25, 12 }, color::LightPurple, -1.0f, 0.0f);
		DrawDebugEllipse(
			{ 200, -175 }, { 25, 12 }, color::Green, 1.0f, DegToRad(game.time() / 10.0f)
		);
		DrawDebugEllipse(
			{ 275, -175 }, { 25, 12 }, color::DarkGreen, 5.0f, DegToRad(game.time() / 10.0f)
		);
		DrawDebugEllipse(
			{ 350, -175 }, { 25, 12 }, color::LightGreen, -1.0f, DegToRad(game.time() / 10.0f)
		);
		Polygon p{ GetStarVertices(5, 10, 20) };
		DrawDebugPolygon(p.GetWorldVertices({ { -225, -100 } }), color::Cyan, 1.0f);
		DrawDebugPolygon(p.GetWorldVertices({ { -300, -100 } }), color::Cyan, -1.0f);
		DrawDebugPolygon(p.GetWorldVertices({ { -150, -100 } }), color::Cyan, 5.0f);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ShapeScene", resolution);
	game.scene.Enter<ShapeScene>("");
	return 0;
}