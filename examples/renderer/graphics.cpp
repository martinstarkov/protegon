#include "runtime/graphics/graphics.h"

#include "app/application.h"
#include "core/event/dispatcher.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "platform/input/events.h"
#include "platform/input/key.h"
#include "platform/window/window.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/ui/button.h"

using namespace ptgn;

struct GraphicsScene : public Scene {
	Graphics graphics;

	void OnEnter() override {
		graphics = CreateGraphics(*this);

		graphics.SetStrokeColor(color::Blue);
		graphics.SetLineWidth(FillStyle::Hollow(2.0f));
		graphics.StrokeCircle({ 0, 80 }, Circle{ 40.0f });
		graphics.StrokeCircle({ 40, 80 }, Circle{ 20.0f });

		graphics.SetFillColor(color::Blue);
		graphics.FillRect(Transform{ V2_int{ 0, 0 } }, Rect{ 40.0f, 30.0f });
		graphics.FillRect(Transform{ V2_int{ 70, 70 } }, Rect{ 40.0f, 30.0f });

		graphics.Line({ 100, 50 }, { -100, -50 });
	}

	void OnUpdate() override {
		MoveWASD(graphics, V2_float{ 300.0f * app().DeltaTime().count() });
	}
};

int main(int, char**) {
	Application game{ "GraphicsScene: WASD to move graphics object" };
	game.StartWith<GraphicsScene>();
}