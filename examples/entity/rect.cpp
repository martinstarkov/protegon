#include "app/application.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "runtime/graphics/shape.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class RectEntityScene : public Scene {
	void OnEnter() override {
		CreateRect(*this, { -100, 0 }, { 80, 40 }, color::Blue, 1.0f, Origin::Center);
		CreateRect(*this, { 0, 0 }, { 80, 40 }, color::LightBlue, Solid{}, Origin::Center);
		CreateRect(*this, { 100, 0 }, { 80, 40 }, color::DarkBlue, 5.0f, Origin::Center);
	}
};

int main(int, char**) {
	Application app{ "rect_entity" };
	app.StartWith<RectEntityScene>();
}
