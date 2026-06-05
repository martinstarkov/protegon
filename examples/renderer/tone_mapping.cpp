#include "app/application.h"
#include "core/graphics/color.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class ToneMappingScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, "sprite", { 0.0f, 0.0f });
		CreateRect(*this, { 0.0f, -200.0f }, { 100.0f, 100.0f }, color::Blue);

		AddScreenEffect<Bloom>(
			*this, Bloom{ .threshold	   = 0.005f,
						  .soft_knee	   = 0.01f,
						  .radius		   = 1.0f,
						  .intensity	   = 1.0f,
						  .blur_iterations = 4 }
		);
	}
};

int main(int, char**) {
	Application app{ "ToneMappingScene" };
	// PTGN_WITH_EDITOR(app);
	app.StartWith<ToneMappingScene>();
}