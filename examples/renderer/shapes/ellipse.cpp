#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "runtime/graphics/shape.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class EllipseEntityScene : public Scene {
	void OnEnter() override {
		CreateEllipse(*this, { -150, 0 }, { 45, 25 }, color::Purple, 1.0f);
		CreateEllipse(*this, { 0, 0 }, { 45, 25 }, color::LightPurple, Solid{});
		CreateEllipse(*this, { 150, 0 }, { 45, 25 }, color::Magenta, 5.0f);
	}
};

int main(int, char**) {
	Application app;
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<EllipseEntityScene>();
}
