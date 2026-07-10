#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

struct BlendModeScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("semitransparent", "assets/semitransparent.png");
		ctx().asset.Load("opaque", "assets/smile.png");

		V2_float ws{ ctx().renderer.GetLogicalSize() };

		CreateRect(
			*this, -ws * 0.5f + V2_float{}, { ws.x, 100 }, color::Red, Solid{}, Origin::TopLeft
		);
		CreateRect(
			*this, -ws * 0.5f + V2_float{ 0, 100 }, { ws.x, 100 }, Color{ 255, 0, 0, 128 }, Solid{},
			Origin::TopLeft
		);
		CreateRect(
			*this, -ws * 0.5f + V2_float{}, { ws.x / 2.0f, ws.y }, Color{ 0, 0, 255, 128 }, Solid{},
			Origin::TopLeft
		);

		auto s1 = CreateSprite(*this, -ws * 0.5f + V2_float{ 100, 100 }, "semitransparent");
		s1.Add<Origin>(Origin::TopLeft);
		auto s2 = CreateSprite(*this, -ws * 0.5f + V2_float{ 200, 200 }, "opaque");
		s2.Add<Origin>(Origin::TopLeft);
	}
};

int main(int, char**) {
	Application app{ "BlendModeScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<BlendModeScene>();
}