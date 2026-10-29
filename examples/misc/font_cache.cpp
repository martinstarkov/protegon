
#include "app/application.h"
#include "app/editor.h"
#include "core/math/transform.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/render_queue.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class FontCacheScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("fontA", "assets/Arial.ttf");
	}

	void OnUpdate() override {
		auto size{ ctx().asset.GetFontAtlasSize("fontA") };
		auto texture{ ctx().asset.GetFontAtlasTexture("fontA") };
		ctx().render_queue.DrawTexture(
			Transform{}, texture, size, ctx().renderer.GetShader("texture")
		);
	}
};

int main(int, char**) {
	Application app{ "FontCacheScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<FontCacheScene>();
}