#include "runtime/graphics/fx/bloom.h"

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class BloomEffectScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, { -180, 0 }, "sprite");
		CreateRect(*this, { 0, -320 }, { 100, 100 }, color::Blue);

		auto rect2{ CreateRect(*this, { 0, -200 }, { 100, 100 }, color::Blue) };
		auto sprite2{ CreateSprite(*this, { 180, 0 }, "sprite") };

		AddEffect<Bloom>(sprite2, Bloom{ .threshold = 0.5f }).Add<EffectMargin>(40);
		AddEffect<Bloom>(
			rect2, Bloom{ .threshold	   = 0.0f,
						  .soft_knee	   = 0.01f,
						  .radius		   = 5.0f,
						  .intensity	   = 5.0f,
						  .blur_iterations = 10 }
		)
			.Add<EffectMargin>(40);
	}
};

int main(int, char**) {
	Application app{ "BloomEffectScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<BloomEffectScene>();
}