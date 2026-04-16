#include "core/graphics/gradient.h"

#include <chrono>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "runtime/animation/tween.h"
#include "runtime/animation/tween_event.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/tint.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class GradientScene : public Scene {
public:
	Gradient gradient{ "rgba(255, 0, 0, 1.0) 0% "
					   "rgba(255, 255, 0, 1.0) 25% "
					   "rgba(0, 255, 0, 1.0) 50% "
					   "rgba(0, 255, 255, 1.0) 75% "
					   "rgba(0, 0, 255, 1.0) 100%" };

	void OnEnter() override {
		auto rect{ CreateRect(*this, {}, { 200, 200 }, color::White, Solid{}, Origin::Center) };

		auto tint_by_gradient = [this, rect](const auto& e) {
			SetTint(rect, gradient.Sample(e.progress));
		};

		CreateTween(*this).During(3s).Yoyo().Repeat().OnProgress(tint_by_gradient).Start();
	}
};

int main(int, char**) {
	Application app{ "GradientScene" };
	app.StartWith<GradientScene>();
}