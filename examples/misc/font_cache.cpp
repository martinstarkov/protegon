#include <cstdint>
#include <string>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class FontCacheScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("fontA", "assets/Arial.ttf");
	}

	void OnUpdate() override {
		auto font{ ctx().asset.Get<Font>("fontA") };
		auto size{ font.GetAtlasSize() };
		ctx().renderer.DrawTexture(
			font.GetAtlasTexture(), size, ctx().renderer.GetShader("texture"), {}
		);
	}
};

int main(int, char**) {
	Application app{ "FontCache" };
	app.StartWith<FontCacheScene>();
}