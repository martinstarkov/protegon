
#include "ecs/components/draw.h"
#include "core/app/application.h"
#include "core/app/window.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

struct ShapeScene : public Scene {
	void Enter() override {
		Application::Get().window_.SetResizable();
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
		Application::Get().render_.DrawLines(
			{}, { { -375, -375 }, { -350, -350 }, { -325, -375 } }, color::Beige, 1.0f, false
		);
		Application::Get().render_.DrawLines(
			{ { -300, -375 }, { -275, -350 }, { -250, -375 } }, color::Beige, 5.0f, false
		);
		Application::Get().render_.DrawLines(
			{ { -225, -375 }, { -200, -350 }, { -175, -375 } }, color::Beige, 1.0f, true
		);
		Application::Get().render_.DrawPoint({ -350, -300 }, color::Red);
		Application::Get().render_.DrawLine({ -300, -300 }, { -250, -325 }, color::Orange, 1.0f);
		Application::Get().render_.DrawLine({ -200, -325 }, { -150, -300 }, color::Yellow, 5.0f);
		Application::Get().render_.DrawCapsule(
			{}, { { -275, -250 }, { -175, -250 }, 12.0f }, color::Yellow, 1.0f
		);
		Application::Get().render_.DrawCapsule(
			{}, { { -300, -300 + 150 }, { -250, -350 + 150 }, 12.0f }, color::Orange, 5.0f
		);
		Application::Get().render_.DrawCapsule(
			{}, { { -200, -350 + 150 }, { -150, -300 + 150 }, 12.0f }, color::LightGold, -1.0f
		);

		constexpr float start_angle1{ DegToRad(0.0f) };
		constexpr float end_angle1{ DegToRad(180.0f) };
		constexpr float start_angle2{ DegToRad(180.0f) };
		constexpr float end_angle2{ DegToRad(0.0f) };
		constexpr float start_angle3{ DegToRad(-180.0f) };
		constexpr float end_angle3{ DegToRad(90.0f) };
		constexpr float start_angle4{ DegToRad(-90.0f) };
		constexpr float end_angle4{ DegToRad(269.0f) };

		float arc_radius{ 20.0f };
		bool clockwise{ true };

		float arc_y{ -10 };

		Application::Get().render_.DrawArc(
			V2_float{ -50, arc_y }, { arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightGreen, 1.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 0, arc_y }, { arc_radius, start_angle2, end_angle2, clockwise },
			color::BrightGreen, 1.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 50, arc_y }, { arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightGreen, 1.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 100, arc_y }, { arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightGreen, 1.0f
		);

		Application::Get().render_.DrawArc(
			V2_float{ -50, arc_y + 50.0f }, { arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightPink, 5.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 0, arc_y + 50.0f }, { arc_radius, start_angle3, end_angle2, clockwise },
			color::BrightPink, 5.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 50, arc_y + 50.0f }, { arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightPink, 5.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 100, arc_y + 50.0f }, { arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightPink, 5.0f
		);

		Application::Get().render_.DrawArc(
			V2_float{ -50, arc_y + 100.0f }, { arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightYellow, -1.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 0, arc_y + 100.0f }, { arc_radius, start_angle3, end_angle2, clockwise },
			color::BrightYellow, -1.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 50, arc_y + 100.0f }, { arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightYellow, -1.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 100, arc_y + 100.0f }, { arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightYellow, -1.0f
		);

		clockwise = false;

		Application::Get().render_.DrawArc(
			V2_float{ -50, arc_y + 150.0f }, { arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightGreen, 1.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 0, arc_y + 150.0f }, { arc_radius, start_angle2, end_angle2, clockwise },
			color::BrightGreen, 1.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 50, arc_y + 150.0f }, { arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightGreen, 1.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 100, arc_y + 150.0f }, { arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightGreen, 1.0f
		);

		Application::Get().render_.DrawArc(
			V2_float{ -50, arc_y + 200.0f }, { arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightPink, 5.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 0, arc_y + 200.0f }, { arc_radius, start_angle3, end_angle2, clockwise },
			color::BrightPink, 5.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 50, arc_y + 200.0f }, { arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightPink, 5.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 100, arc_y + 200.0f }, { arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightPink, 5.0f
		);

		Application::Get().render_.DrawArc(
			V2_float{ -50, arc_y + 250.0f }, { arc_radius, start_angle1, end_angle1, clockwise },
			color::BrightYellow, -1.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 0, arc_y + 250.0f }, { arc_radius, start_angle3, end_angle2, clockwise },
			color::BrightYellow, -1.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 50, arc_y + 250.0f }, { arc_radius, start_angle3, end_angle3, clockwise },
			color::BrightYellow, -1.0f
		);
		Application::Get().render_.DrawArc(
			V2_float{ 100, arc_y + 250.0f }, { arc_radius, start_angle4, end_angle4, clockwise },
			color::BrightYellow, -1.0f
		);

		Application::Get().render_.DrawRect(
			V2_int{ -50, -325 }, V2_int{ 50, 25 }, color::Blue, 1.0f, Origin::Center
		);
		Application::Get().render_.DrawRect(
			V2_int{ 0, -325 }, V2_int{ 50, 25 }, color::LightBlue, -1.0f, Origin::TopLeft
		);
		Application::Get().render_.DrawRect(
			V2_int{ 100, -325 }, V2_int{ 50, 25 }, color::DarkBlue, 5.0f, Origin::Center
		);
		Application::Get().render_.DrawRect(
			{ { -50, -250 }, DegToRad(Application::Get().time() / 10.0f) }, V2_int{ 50, 25 }, color::Blue, 1.0f,
			Origin::Center
		);
		Application::Get().render_.DrawRect(
			{ { 0, -250 }, DegToRad(Application::Get().time() / 10.0f) }, V2_int{ 50, 25 }, color::LightBlue,
			-1.0f, Origin::TopLeft

		);
		Application::Get().render_.DrawRect(
			{ { 100, -250 }, DegToRad(Application::Get().time() / 10.0f) }, V2_int{ 50, 25 }, color::DarkBlue,
			5.0f, Origin::Center

		);

		Application::Get().render_.DrawRoundedRect(
			V2_int{ -50, -175 }, { { 50, 25 }, 12.0f }, color::Blue, 1.0f, Origin::Center
		);
		Application::Get().render_.DrawRoundedRect(
			V2_int{ 0, -175 }, { { 50, 25 }, 12.0f }, color::LightBlue, -1.0f, Origin::TopLeft
		);
		Application::Get().render_.DrawRoundedRect(
			{ { 100, -175 }, DegToRad(Application::Get().time() / 10.0f) }, { { 50, 25 }, 12.0f },
			color::DarkBlue, 5.0f, Origin::Center
		);
		Application::Get().render_.DrawRoundedRect(
			{ { -50, -100 }, DegToRad(Application::Get().time() / 10.0f) }, { { 50, 25 }, 12.0f }, color::Blue,
			1.0f, Origin::Center

		);
		Application::Get().render_.DrawRoundedRect(
			{ { 0, -100 }, DegToRad(Application::Get().time() / 10.0f) }, { { 50, 25 }, 12.0f }, color::LightBlue,
			-1.0f, Origin::TopLeft

		);
		Application::Get().render_.DrawRoundedRect(
			{ { 100, -100 }, DegToRad(Application::Get().time() / 10.0f) }, { { 50, 25 }, 12.0f },
			color::DarkBlue, 5.0f, Origin::Center

		);

		Application::Get().render_.DrawCircle(V2_int{ 200, -325 }, 25.0f, color::Gold, 1.0f);
		Application::Get().render_.DrawCircle(V2_int{ 275, -325 }, 25.0f, color::DarkYellow, 5.0f);
		Application::Get().render_.DrawCircle(V2_int{ 350, -325 }, 25.0f, color::LightYellow, -1.0f);

		Application::Get().render_.DrawEllipse(
			V2_int{ 200, -250 }, Ellipse{ V2_int{ 25, 12 } }, color::Purple, 1.0f
		);
		Application::Get().render_.DrawEllipse(
			V2_int{ 275, -250 }, Ellipse{ V2_int{ 25, 12 } }, color::Magenta, 5.0f
		);
		Application::Get().render_.DrawEllipse(
			V2_int{ 350, -250 }, Ellipse{ V2_int{ 25, 12 } }, color::LightPurple, -1.0f
		);
		Application::Get().render_.DrawEllipse(
			{ V2_int{ 200, -175 }, DegToRad(Application::Get().time() / 10.0f) }, Ellipse{ V2_int{ 25, 12 } },
			color::Green, 1.0f

		);
		Application::Get().render_.DrawEllipse(
			{ V2_int{ 275, -175 }, DegToRad(Application::Get().time() / 10.0f) }, Ellipse{ V2_int{ 25, 12 } },
			color::DarkGreen, 5.0f

		);
		Application::Get().render_.DrawEllipse(
			{ V2_int{ 350, -175 }, DegToRad(Application::Get().time() / 10.0f) }, Ellipse{ V2_int{ 25, 12 } },
			color::LightGreen, -1.0f

		);

		Polygon p{ GetStarVertices(5, 10, 20) };

		Application::Get().render_.DrawShape(V2_int{ -225, -100 }, p, color::Cyan, 1.0f);
		Application::Get().render_.DrawShape(V2_int{ -300, -100 }, p, color::Cyan, -1.0f);
		Application::Get().render_.DrawShape(V2_int{ -150, -100 }, p, color::Cyan, 5.0f);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("ShapeScene", resolution);
	Application::Get().scene_.Enter<ShapeScene>("");
	return 0;
}