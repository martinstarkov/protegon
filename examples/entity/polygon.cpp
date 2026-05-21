#include <cmath>
#include <vector>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
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

class PolygonEntityScene : public Scene {
	void OnEnter() override {
		auto star_vertices{ MakeStarVertices(5, 45.0f, 20.0f) };

		CreatePolygon(*this, { -100, 0 }, star_vertices, color::Cyan, 1.0f);
		CreatePolygon(*this, { 0, 0 }, star_vertices, color::Cyan, Solid{});
		CreatePolygon(*this, { 100, 0 }, star_vertices, color::Cyan, 5.0f);
	}
};

int main(int, char**) {
	Application app{ "polygon_entity" };
	app.StartWith<PolygonEntityScene>();
}
