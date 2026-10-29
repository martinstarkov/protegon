#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "runtime/graphics/shape.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class LineEntityScene : public Scene {
	void OnEnter() override {
		CreateLine(*this, { -150, 0 }, { -50, -25 }, { 50, 25 }, color::Orange, 1.0f);
		CreateLine(*this, { 0, 0 }, { -50, 25 }, { 50, -25 }, color::Yellow, 5.0f);
		CreateLine(*this, { 150, 0 }, { -50, 0 }, { 50, 0 }, color::LightGold, 8.0f);
	}
};

int main(int, char**) {
	Application app;
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<LineEntityScene>();
}
