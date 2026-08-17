#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <system_error>

#include "app/application.h"
#include "app/editor.h"
#include "core/assert.h"
#include "core/build_info.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/font_system.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_registry.h"

using namespace ptgn;

namespace {

Transform At(V2_float position) {
	Transform transform;
	transform.position = position;
	return transform;
}

void RemoveInterFontCacheForDemo() {
#ifndef __EMSCRIPTEN__
	std::error_code error;
	std::filesystem::remove(
		impl::GetBuildInfo().binary_directory / "cache/fonts/Inter-VariableFont.png",
		error
	);
#endif
}

const TextureKey kAsyncTexture{ "streaming/bmp" };
const FontKey kAsyncFont{ "streaming/inter" };
const AudioKey kAsyncMusic{ "streaming/music" };

std::string StateWord(bool loaded) {
	return loaded ? "ready" : "loading";
}

} // namespace

class BackgroundAssetLoadingScene : public Scene {
public:
	void OnNew() override {
		SetBackgroundColor(Color{ 20, 23, 30, 255 });

		RemoveInterFontCacheForDemo();

		PTGN_ASSERT(ctx().asset.RegisterAsset(kAsyncTexture, "assets/bmp.bmp"));
		PTGN_ASSERT(ctx().asset.RegisterAsset(kAsyncFont, "assets/Inter-VariableFont.ttf"));
		PTGN_ASSERT(ctx().asset.RegisterAsset(kAsyncMusic, "assets/music2.ogg"));

		const std::array<AssetKey, 3> dependencies{
			kAsyncTexture,
			kAsyncFont,
			kAsyncMusic,
		};
		ticket_ = ctx().asset.AcquireDependenciesAsync(
			std::span<const AssetKey>{ dependencies.data(), dependencies.size() }
		);

		CreateText(
			*this,
			At({ 0.0f, -260.0f }),
			"Scene is running while assets stream in",
			color::White,
			34.0f
		);

		font_demo_ = CreateText(
			*this,
			At({ 0.0f, -170.0f }),
			"This requests Inter immediately and falls back to the default font while it loads",
			color::White,
			28.0f,
			Origin::Center,
			kAsyncFont.value
		);

		texture_status_ = CreateText(
			*this,
			At({ 0.0f, 20.0f }),
			"BMP loading: sprite area is blank until ready",
			Color{ 235, 80, 220, 255 },
			24.0f
		);

		load_status_ = CreateText(
			*this,
			At({ 0.0f, 270.0f }),
			"Loading...",
			color::White,
			20.0f
		);
	}

	void OnUpdate() override {
		const bool texture_loaded{ ctx().asset.Has(kAsyncTexture) };
		const bool font_loaded{ ctx().asset.Has(kAsyncFont) };
		const bool audio_loaded{ ctx().asset.Has(kAsyncMusic) };

		if (texture_loaded && !sprite_created_) {
			auto sprite{ CreateSprite(*this, { 0.0f, 70.0f }, kAsyncTexture) };
			SetScale(sprite, 0.30f);
			sprite_created_ = true;

			texture_status_
				.Clear()
				.Content("BMP ready: the real sprite replaced the blank placeholder")
				.Color(color::White)
				.Size(24.0f);
		}

		if (font_loaded && !font_switched_) {
			font_demo_.Font(kDefaultFont).Font(kAsyncFont.value);
			font_switched_ = true;
		}

		const auto progress{ ticket_.GetProgress() };
		load_status_
			.Clear()
			.Content(std::format(
				"Overall: {}%  ({}/{})\nTexture: {}   Font: {}   Audio: {}",
				static_cast<int>(progress.Fraction() * 100.0f),
				progress.completed_assets,
				progress.total_assets,
				StateWord(texture_loaded),
				StateWord(font_loaded),
				StateWord(audio_loaded)
			))
			.Color(color::White)
			.Size(20.0f);
	}

private:
	impl::AssetLoadTicket ticket_;
	Text font_demo_;
	Text texture_status_;
	Text load_status_;
	bool sprite_created_{ false };
	bool font_switched_{ false };
};

PTGN_REGISTER_SCENE(BackgroundAssetLoadingScene, "Background Asset Loading Scene");

int main(int, char**) {
	Application app{ "Background Asset Loading Demo", { 960, 720 } };
	PTGN_WITH_EDITOR(app, true);
	app.StartWith<BackgroundAssetLoadingScene>("background");
}
