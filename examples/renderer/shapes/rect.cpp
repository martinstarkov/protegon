#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/shape.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class RectEntityScene : public Scene {
	void OnEnter() override {
		CreateRect(*this, { -100, 0 }, { 80, 40 }, color::Blue, 1.0f, Origin::Center);
		CreateRect(*this, { 0, 0 }, { 80, 40 }, color::LightBlue, Solid{}, Origin::Center);
		auto rect3{
			CreateRect(*this, { 100, 0 }, { 80, 40 }, color::DarkBlue, 5.0f, Origin::Center)
		};
		SetRotation(rect3, 45.0f);
	}
};

int main(int, char**) {
	Application app;
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<RectEntityScene>();
}
