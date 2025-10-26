#include "core/ecs/components/movement.h"
#include "core/app/application.h"
#include "renderer/vfx/graphics.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"

using namespace ptgn;

struct GraphicsScene : public Scene {
	Graphics graphics;

	void Enter() override {
		graphics = CreateGraphics(*this);

		graphics.SetFillColor(color::Red);
		graphics.FillRect({}, Rect{ 30.0f, 30.0f });

		graphics.SetStrokeColor(color::Blue);
		graphics.SetLineWidth(2.0f);
		graphics.StrokeCircle({ 0, 80 }, Circle{ 40.0f });

		graphics.Line({ 100, 50 }, { -100, -50 });
	}

	void Update() override {
		MoveWASD(graphics, V2_float{ 300.0f * game.dt() });
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("GraphicsScene");
	game.scene.Enter<GraphicsScene>("");
	return 0;
}