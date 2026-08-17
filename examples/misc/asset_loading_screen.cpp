#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "app/application.h"
#include "app/editor.h"
#include "core/assert.h"
#include "core/build_info.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
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

const TextureKey kGameTexture{ "global/bmp/main" };
const FontKey kGameFont{ "global/font/inter" };
const AudioKey kGameMusic{ "global/music/main" };

struct AssetSpec {
	AssetKey key;
	path source;
};

std::vector<AssetSpec> GetGlobalAssets() {
	return {
		{ kGameTexture, "assets/bmp.bmp" },
		{ AssetKey{ "global/bmp/duplicate_1" }, "assets/bmp.bmp" },
		{ AssetKey{ "global/bmp/duplicate_2" }, "assets/bmp.bmp" },
		{ AssetKey{ "global/bmp/duplicate_3" }, "assets/bmp.bmp" },
		{ AssetKey{ "global/bmp/duplicate_4" }, "assets/bmp.bmp" },
		{ kGameFont, "assets/Inter-VariableFont.ttf" },
		{ AssetKey{ "global/bmp/duplicate_5" }, "assets/Inter-VariableFont.ttf" },
		{ AssetKey{ "global/bmp/duplicate_6" }, "assets/Inter-VariableFont.ttf" },
		{ AssetKey{ "global/bmp/duplicate_7" }, "assets/Inter-VariableFont.ttf" },
		{ AssetKey{ "global/bmp/duplicate_8" }, "assets/Inter-VariableFont.ttf" },
		{ AssetKey{ "global/bmp/duplicate_9" }, "assets/Inter-VariableFont.ttf" },
		{ AssetKey{ "global/bmp/duplicate_10" }, "assets/Inter-VariableFont.ttf" },
		{ AssetKey{ "global/bmp/duplicate_11" }, "assets/Inter-VariableFont.ttf" },
		{ AssetKey{ "global/bmp/duplicate_12" }, "assets/Inter-VariableFont.ttf" },
		{ AssetKey{ "global/bmp/duplicate_13" }, "assets/Inter-VariableFont.ttf" },
		{ AssetKey{ "global/bmp/duplicate_14" }, "assets/Inter-VariableFont.ttf" },
		{ kGameMusic, "assets/music2.ogg" },
		{ AssetKey{ "global/music/duplicate_1" }, "assets/music2.ogg" },
	};
}

std::string MakeProgressBar(float fraction, std::size_t width = 36) {
	fraction = std::clamp(fraction, 0.0f, 1.0f);
	const auto filled{ static_cast<std::size_t>(fraction * static_cast<float>(width)) };
	return "[" + std::string(filled, '#') + std::string(width - filled, '-') + "]";
}

} // namespace

class LoadedGameScene : public Scene {
public:
	void OnNew() override {
		SetBackgroundColor(Color{ 22, 26, 34, 255 });

		CreateText(
			*this,
			At({ 0.0f, -230.0f }),
			"Game scene started after all global assets finished loading",
			color::White,
			30.0f,
			Origin::Center,
			kGameFont.value
		);

		auto sprite{ CreateSprite(*this, { 0.0f, 20.0f }, kGameTexture) };
		SetScale(sprite, 0.35f);

		CreateText(
			*this,
			At({ 0.0f, 260.0f }),
			ctx().asset.Has(kGameMusic)
				? "music2.ogg is resident and ready to use"
				: "music2.ogg failed to load",
			color::White,
			22.0f
		);
	}
};

PTGN_REGISTER_SCENE(LoadedGameScene, "Loaded Game Scene");

class AssetLoadingScreenScene : public Scene {
public:
	void OnNew() override {
		SetBackgroundColor(Color{ 14, 16, 22, 255 });

		RemoveInterFontCacheForDemo();

		CreateText(
			*this,
			At({ 0.0f, -90.0f }),
			"Loading project assets...",
			color::White,
			40.0f
		);

		status_ = CreateText(
			*this,
			At({ 0.0f, 20.0f }),
			"Starting load...",
			color::White,
			22.0f
		);

		const auto specs{ GetGlobalAssets() };
		dependencies_.reserve(specs.size());

		for (const auto& spec : specs) {
			PTGN_ASSERT(
				ctx().asset.RegisterAsset(spec.key, spec.source),
				"Failed to register loading-screen asset: ", spec.source.string()
			);
			dependencies_.emplace_back(spec.key);
		}

		ctx().asset.AddProjectAssetDependencies(
			std::span<const AssetKey>{ dependencies_.data(), dependencies_.size() }
		);
		ticket_ = ctx().asset.AcquireDependenciesAsync(
			std::span<const AssetKey>{ dependencies_.data(), dependencies_.size() }
		);
	}

	void OnUpdate() override {
		const auto progress{ ticket_.GetProgress() };
		const int percent{ static_cast<int>(progress.Fraction() * 100.0f) };

		std::string active{
			progress.active_asset.empty() ? "preparing workers" : progress.active_asset
		};

		status_
			.Clear()
			.Content(std::format(
				"{}  {}%\n{}/{} assets complete\nCurrent: {}",
				MakeProgressBar(progress.Fraction()),
				percent,
				progress.completed_assets,
				progress.total_assets,
				active
			))
			.Color(color::White)
			.Size(22.0f);

		if (switched_ || !progress.IsComplete()) {
			return;
		}

		if (progress.failed_assets > 0) {
			status_
				.Clear()
				.Content(std::format("Loading failed: {} asset(s)", progress.failed_assets))
				.Color(color::Red)
				.Size(24.0f);
			return;
		}

		switched_ = ctx().scene.Switch<LoadedGameScene>("game");
	}

private:
	std::vector<AssetKey> dependencies_;
	impl::AssetLoadTicket ticket_;
	Text status_;
	bool switched_{ false };
};

PTGN_REGISTER_SCENE(AssetLoadingScreenScene, "Asset Loading Screen");

int main(int, char**) {
	Application app{ "Real Asset Loading Screen Demo", { 960, 720 } };
	PTGN_WITH_EDITOR(app, true);
	app.StartWith<AssetLoadingScreenScene>("loading");
}
