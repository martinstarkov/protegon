#include "runtime/graphics/fx/bloom.h"

#include "app/application.h"
#include "core/editor.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class BloomEffectScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, "sprite", { -180.0f, 0.0f });
		CreateSprite(*this, "sprite", { 180.0f, 0.0f });

		CreateRect(*this, { 0.0f, -200.0f }, { 50.0f, 50.0f }, { 255, 0, 0, 255 });
		CreateRect(*this, { 100.0f, -200.0f }, { 50.0f, 50.0f }, { 0, 255, 0, 255 });
		CreateRect(*this, { -100.0f, -200.0f }, { 50.0f, 50.0f }, { 0, 0, 255, 255 });

		CreateEffect<Bloom>(*this);

		CreateRect(*this, { 0.0f, -300.0f }, { 50.0f, 50.0f }, { 255, 0, 0, 255 });
		CreateRect(*this, { 100.0f, -300.0f }, { 50.0f, 50.0f }, { 0, 255, 0, 255 });
		CreateRect(*this, { -100.0f, -300.0f }, { 50.0f, 50.0f }, { 0, 0, 255, 255 });
	}
};

int main(int, char**) {
	Application app{ "BloomEffectScene" };
	PTGN_WITH_EDITOR(app);
	app.StartWith<BloomEffectScene>();
}