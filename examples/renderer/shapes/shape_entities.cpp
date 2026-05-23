#include <cmath>
#include <vector>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/angle.h"
#include "core/math/geometry/origin.h"
#include "core/math/math_utils.h"
#include "core/math/vector2.h"
#include "runtime/graphics/shape.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

static std::vector<V2_float> MakeStarVertices(int count, float outer_radius, float inner_radius) {
	std::vector<V2_float> vertices;
	float angle_step{ kPi / static_cast<float>(count) };

	for (int i{ 0 }; i < 2 * count; ++i) {
		float radius{ (i % 2 == 0) ? outer_radius : inner_radius };
		float theta{ static_cast<float>(i) * angle_step - kHalfPi };
		vertices.push_back({ radius * std::cos(theta), radius * std::sin(theta) });
	}

	return vertices;
}

class ShapeEntitiesScene : public Scene {
	void OnEnter() override {
		CreateRect(*this, { -300, 250 }, { 80, 40 }, color::Blue, 1.0f, Origin::Center);
		CreateRoundedRect(
			*this, { -150, 250 }, { 90, 45 }, 12.0f, color::LightBlue, Solid{}, Origin::Center
		);
		CreateCircle(*this, { 0, 250 }, 35.0f, color::Gold, 1.0f);
		CreateEllipse(*this, { 150, 250 }, { 45, 25 }, color::Purple, Solid{});
		CreateCapsule(*this, { 300, 250 }, { -50, 0 }, { 50, 0 }, 16.0f, color::Orange, 5.0f);

		CreateArc(
			*this, { -300, 50 }, 45.0f, Degrees{ 0.0f }, Degrees{ 270.0f }, true,
			color::BrightGreen, 4.0f
		);
		CreateTriangle(
			*this, { -150, 50 }, { 0, 45 }, { -45, -35 }, { 45, -35 }, color::BrightPink, Solid{}
		);
		CreateLine(*this, { 0, 50 }, { -60, -25 }, { 60, 25 }, color::Yellow, 6.0f);

		auto star_vertices{ MakeStarVertices(5, 45.0f, 20.0f) };
		CreatePolygon(*this, { 150, 50 }, star_vertices, color::Cyan, Solid{});

		CreateRect(*this, { 300, 50 }, { 70, 70 }, color::DarkBlue, 5.0f, Origin::Center);
		CreateRoundedRect(
			*this, { -225, -150 }, { 100, 50 }, 18.0f, color::DarkBlue, 5.0f, Origin::Center
		);
		CreateCircle(*this, { -75, -150 }, 35.0f, color::LightYellow, Solid{});
		CreateEllipse(*this, { 75, -150 }, { 50, 20 }, color::Magenta, 5.0f);
		CreateCapsule(
			*this, { 225, -150 }, { -60, -15 }, { 60, 15 }, 15.0f, color::LightGold, Solid{}
		);
	}
};

int main(int, char**) {
	Application app{ "shape_entities" };
	app.StartWith<ShapeEntitiesScene>();
}
