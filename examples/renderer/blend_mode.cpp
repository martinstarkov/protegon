#include "app/application.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "platform/window/window.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"

#include "runtime/scene/scene_manager.h"

using namespace ptgn;

struct BlendModeScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("semitransparent", "assets/semitransparent.png");
		ctx().asset.Load("opaque", "assets/smile.png");

		V2_float ws{ ctx().renderer.GetGameSize() };

		CreateRect(
			*this, -ws * 0.5f + V2_float{}, { ws.x, 100 }, color::Red, Solid{},
			Origin::TopLeft
		);
		CreateRect(
			*this, -ws * 0.5f + V2_float{ 0, 100 }, { ws.x, 100 }, Color{ 255, 0, 0, 128 },
			Solid{}, Origin::TopLeft
		);
		CreateRect(
			*this, -ws * 0.5f + V2_float{}, { ws.x / 2.0f, ws.y }, Color{ 0, 0, 255, 128 },
			Solid{}, Origin::TopLeft
		);

		auto s1 = CreateSprite(*this, "semitransparent", -ws * 0.5f + V2_float{ 100, 100 });
		SetDrawOrigin(s1, Origin::TopLeft);
		auto s2 = CreateSprite(*this, "opaque", -ws * 0.5f + V2_float{ 200, 200 });
		SetDrawOrigin(s2, Origin::TopLeft);
	}
};

int main(int, char**) {
	Application app{ "BlendModeScene" };
	app.StartWith<BlendModeScene>();
}