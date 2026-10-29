#include "runtime/graphics/graphics.h"

#include <chrono>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

struct GraphicsScene : public Scene {
	Graphics graphics;

	void OnEnter() override {
		graphics = CreateGraphics(*this);

		graphics.SetStrokeColor(color::Blue);
		graphics.SetLineWidth(2.0f);
		graphics.StrokeCircle({ 0, 80 }, Circle{ 40.0f });
		graphics.StrokeCircle({ 40, 80 }, Circle{ 20.0f });

		graphics.SetFillColor(color::Blue);
		graphics.FillRect(Transform{ V2_int{ 0, 0 } }, Rect{ 40.0f, 30.0f });
		graphics.FillRect(Transform{ V2_int{ 70, 70 } }, Rect{ 40.0f, 30.0f });

		graphics.Line({ 100, 50 }, { -100, -50 });
	}

	void OnUpdate() override {
		constexpr V2_float speed{ 300.0f };
		float dt{ ctx().dt().count() };
		MoveWASD(graphics, speed * dt);
	}
};

int main(int, char**) {
	Application game{ "GraphicsScene: WASD to move graphics object" };
	game.StartWith<GraphicsScene>();
}