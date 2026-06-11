#include "runtime/graphics/text/font_system.h"

#include <ecs/ecs.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <ios>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/config.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "fonts/default_font.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/text/font.h"

namespace ptgn {

namespace {

inline constexpr std::string_view kDefaultFontCacheDirectory{ PTGN_ENGINE_ROOT "/assets/fonts" };
inline constexpr std::string_view kDefaultFontFile{ PTGN_ENGINE_ROOT
													"/assets/fonts/LiberationSans-Regular.ttf" };
/// @brief Relative to the working directory.
inline constexpr std::string_view kFontCacheDirectory{ "cache/fonts" };
/// @brief Enables generating a default font atlas at runtime and overwriting default_font.h with
/// the generated atlas. This is useful for development and testing.
#ifdef PTGN_DEBUG
inline constexpr bool kGenerateDefaultFontAtlas{ false };
#endif

void WriteGeneratedDefaultFontHeader(const path& font_png_path) {
	auto font_png{ ReadBinary(font_png_path) };
	auto header_path{ path{ kDefaultFontCacheDirectory } / "default_font.h" };

	PTGN_ASSERT(IsDirectoryPath(kDefaultFontCacheDirectory));

	std::ofstream out{ header_path, std::ios::binary | std::ios::trunc };
	PTGN_ASSERT(out, "Failed to open generated default font header: ", header_path.string());

	out << "#pragma once\n\n";
	out << "#include <cstdint>\n\n";
	out << "#include \"runtime/graphics/text/font.h\"\n\n";
	out << "namespace ptgn::impl {\n\n";
	out << "inline constexpr std::uint8_t kDefaultFontBytes[] = {";

	for (auto i{ 0uz }; i < font_png.size(); ++i) {
		out << (i % 12 == 0 ? "\n\t" : " ");
		out << std::format("0x{:02X}", font_png[i]);

		if (i + 1 != font_png.size()) {
			out << ',';
		}
	}

	out << "\n};\n\n";
	out << "inline constexpr FontBinary kDefaultFontBinary{ kDefaultFontBytes, "
		   "sizeof(kDefaultFontBytes) };\n\n";
	out << "} // namespace ptgn::impl\n";

	PTGN_ASSERT(out, "Failed to write generated default font header: ", header_path.string());
}

impl::FontObject GenerateDefaultFontObject(const AssetManager& assets) {
#ifdef __EMSCRIPTEN__
	static_assert(
		false, "Default font atlas header generation is not supported on Emscripten builds"
	);
#endif

	path font_path{ kDefaultFontFile };

	PTGN_ASSERT(FileExists(font_path), "Default font file does not exist: ", font_path.string());

	PTGN_ASSERT(
		impl::MatchesExtension<Font>(GetExtension(font_path)),
		"Default font file must have a valid extension: ", font_path.string()
	);

	PTGN_ASSERT(IsDirectoryPath(kDefaultFontCacheDirectory));

	auto font_png_path{ kDefaultFontCacheDirectory /
						font_path.filename().replace_extension("png") };

	impl::FontObject font_object{ assets, font_path, font_png_path };

	WriteGeneratedDefaultFontHeader(font_png_path);

	return font_object;
}

impl::FontObject GetDefaultFontObject(AssetManager& assets) {
#if !defined(__EMSCRIPTEN__) && defined(PTGN_DEBUG)
	if constexpr (kGenerateDefaultFontAtlas) {
		return GenerateDefaultFontObject(assets);
	} else {
		return impl::FontObject{ assets, impl::kDefaultFontBinary };
	}
#else
	return impl::FontObject{ assets, impl::kDefaultFontBinary };
#endif
}

} // namespace

FontSystem::FontSystem(AssetManager& assets) : assets_{ assets } {
	auto default_font{ GetDefaultFontObject(assets) };

	Font font{ assets_.CreateAsset(), true };
	font.GetEntity().Add<FontSize>(kDefaultFontSize);
	font.GetEntity().Add<impl::FontObject>(std::move(default_font));
	impl::AddAssetKey(font.GetEntity(), {}, std::nullopt);

	default_font_ = {};
}

FontSystem::~FontSystem() noexcept = default;

Font FontSystem::GetDefault() const {
	return assets_.Get<Font>(default_font_);
}

void FontSystem::SetDefault(std::string_view font_key) {
	PTGN_ASSERT(
		assets_.Has<Font>(font_key), "Font key must be loaded before setting it as default"
	);
	default_font_ = font_key;
}

impl::FontObject FontSystem::CreateFont(const AssetManager& asset_manager, const path& font_path) {
	auto absolute_font_path{ GetAbsolutePath(font_path) };

	PTGN_ASSERT(
		FileExists(absolute_font_path),
		"Cannot create font from invalid path: ", absolute_font_path.string()
	);

	auto extension{ GetExtension(absolute_font_path) };

	PTGN_ASSERT(
		impl::MatchesExtension<Font>(extension) || impl::MatchesExtension<Texture>(extension),
		"Font file must have a valid extension: ", absolute_font_path.string()
	);

	if (impl::MatchesExtension<Texture>(extension)) {
		return impl::FontObject{ asset_manager, absolute_font_path };
	}

	auto cache_directory{ GetWorkingDirectory() / kFontCacheDirectory };
	auto cache_name{ absolute_font_path.stem().string() };
	auto cache_png_file{ cache_directory / (cache_name + ".png") };

	if (FileExists(cache_png_file)) {
		return impl::FontObject{ asset_manager, cache_png_file };
	}

	return impl::FontObject{ asset_manager, absolute_font_path, cache_png_file };
}

} // namespace ptgn