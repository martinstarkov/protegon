#include "components/draw.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/window.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/graphics/rect.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

struct BlendModeScene : public Scene {
	void Enter() override {
		LoadResource("semitransparent", "resources/semitransparent.png");
		LoadResource("opaque", "resources/opaque.png");

		V2_float ws{ game.window.GetSize() };

		CreateRect(*this, {}, { ws.x, 100 }, color::Red, -1.0f, Origin::TopLeft);
		CreateRect(
			*this, { 0, 100 }, { ws.x, 100 }, Color{ 255, 0, 0, 128 }, -1.0f, Origin::TopLeft
		);
		CreateRect(
			*this, {}, { ws.x / 2.0f, ws.y }, Color{ 0, 0, 255, 128 }, -1.0f, Origin::TopLeft
		);

		CreateSprite(*this, "semitransparent").SetPosition({ 100, 100 }).SetOrigin(Origin::TopLeft);
		CreateSprite(*this, "opaque").SetPosition({ 200, 200 }).SetOrigin(Origin::TopLeft);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("BlendModeScene");
	game.scene.Enter<BlendModeScene>("");
	return 0;
}