
#include "app/application.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "runtime/graphics/shape.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class RoundedRectEntityScene : public Scene {
	void OnEnter() override {
		CreateRoundedRect(*this, { -100, 0 }, { 90, 45 }, 12.0f, color::Blue, 1.0f, Origin::Center);
		CreateRoundedRect(
			*this, { 0, 0 }, { 90, 45 }, 12.0f, color::LightBlue, Solid{}, Origin::Center
		);
		CreateRoundedRect(
			*this, { 100, 0 }, { 90, 45 }, 12.0f, color::DarkBlue, 5.0f, Origin::Center
		);
	}
};

int main(int, char**) {
	Application app{ "rounded_rect_entity" };
	app.StartWith<RoundedRectEntityScene>();
}
