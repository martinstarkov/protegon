#include "app/application.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "runtime/graphics/shape.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class CircleEntityScene : public Scene {
	void OnEnter() override {
		CreateCircle(*this, { -100, 0 }, 35.0f, color::Gold, 1.0f);
		CreateCircle(*this, { 0, 0 }, 35.0f, color::LightYellow, Solid{});
		CreateCircle(*this, { 100, 0 }, 35.0f, color::DarkYellow, 5.0f);
	}
};

int main(int, char**) {
	Application app{ "circle_entity" };
	app.StartWith<CircleEntityScene>();
}
