#include "app/application.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "runtime/graphics/shape.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class TriangleEntityScene : public Scene {
	void OnEnter() override {
		CreateTriangle(
			*this, { -100, 0 }, { 0, 45 }, { -45, -35 }, { 45, -35 }, color::BrightPink, 1.0f
		);
		CreateTriangle(
			*this, { 0, 0 }, { 0, 45 }, { -45, -35 }, { 45, -35 }, color::BrightYellow, Solid{}
		);
		CreateTriangle(
			*this, { 100, 0 }, { 0, 45 }, { -45, -35 }, { 45, -35 }, color::BrightGreen, 5.0f
		);
	}
};

int main(int, char**) {
	Application app{ "triangle_entity" };
	app.StartWith<TriangleEntityScene>();
}
