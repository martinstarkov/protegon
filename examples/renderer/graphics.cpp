#include "app/application.h"
#include "core/event/dispatcher.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "platform/input/events.h"
#include "platform/input/key.h"
#include "platform/window/window.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/graphics_component.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/scene/scene.h"
#include "runtime/ui/button.h"

using namespace ptgn;

struct GraphicsScene : public Scene {
	Graphics graphics;

	void OnEnter() override {
		graphics = CreateGraphics(*this);

		graphics.SetFillColor(color::Red);
		graphics.FillRect({}, Rect{ 30.0f, 30.0f });

		graphics.SetStrokeColor(color::Blue);
		graphics.SetLineWidth(2.0f);
		graphics.StrokeCircle({ 0, 80 }, Circle{ 40.0f });

		graphics.Line({ 100, 50 }, { -100, -50 });
	}

	void OnUpdate() override {
		// MoveWASD(graphics, V2_float{ 300.0f * game.dt() });
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application game{ { .window = { .title		= "GraphicsScene: WASD to move graphics object",
									.resizeable = true } } };
	game.StartWith<GraphicsScene>("");
	return 0;
}