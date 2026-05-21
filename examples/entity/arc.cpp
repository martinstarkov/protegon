#include "app/application.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/angle.h"
#include "core/math/vector2.h"
#include "runtime/graphics/shape.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class ArcEntityScene : public Scene {
	void OnEnter() override {
		constexpr Degrees start_angle{ 0.0f };
		constexpr Degrees end_angle{ 270.0f };

		CreateArc(
			*this, { -100, 0 }, 40.0f, start_angle, end_angle, true, color::BrightGreen, 1.0f
		);
		CreateArc(*this, { 0, 0 }, 40.0f, start_angle, end_angle, false, color::BrightPink, 5.0f);
		CreateArc(
			*this, { 100, 0 }, 40.0f, start_angle, end_angle, true, color::BrightYellow, Solid{}
		);
	}
};

int main(int, char**) {
	Application app{ "arc_entity" };
	app.StartWith<ArcEntityScene>();
}
