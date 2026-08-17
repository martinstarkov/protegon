#include "runtime/graphics/text/font_system.h"

#include <ecs/ecs.h>

#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/build_info.h"
#include "core/config.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "fonts/default_font.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture.h"
#include "renderer/text/font_atlas.h"
#include "renderer/text/text_style.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/text/font.h"

namespace ptgn {

namespace {

inline constexpr std::string_view kDefaultFontCacheDirectory{ "/assets/fonts" };
inline constexpr std::string_view kDefaultFontFile{ "/assets/fonts/LiberationSans-Regular.ttf" };
inline constexpr std::string_view kFontCacheDirectory{ "cache/fonts" };
#ifdef PTGN_DEBUG
inline constexpr bool kGenerateDefaultFontAtlas{ false };
#endif

void WriteGeneratedDefaultFontHeader(const path& font_png_path) {
	auto font_png{ ReadBinary(font_png_path) };
	auto directory{ impl::GetBuildInfo().engine_directory / path{ kDefaultFontCacheDirectory } };
	auto header_path{ directory / "default_font.h" };

	PTGN_ASSERT(IsDirectoryPath(directory.string()));

	std::ofstream out{ header_path, std::ios::binary | std::ios::trunc };
	PTGN_ASSERT(out, "Failed to open generated default font header: ", header_path.string());

	out << "#pragma once\n\n";
	out << "#include <cstddef>\n";
	out << "#include <cstdint>\n";
	out << "#include <span>\n\n";
	out << "#include \"runtime/graphics/text/font.h\"\n\n";
	out << "namespace ptgn::impl {\n\n";
	out << "inline constexpr std::uint8_t kDefaultFontBytes[] = {";

	for (auto i{ 0uz }; i < font_png.size(); ++i) {
		out << (i % 12 == 0 ? "\n\t" : " ");
		out << std::format("0x{:02X}", std::to_integer<unsigned int>(font_png[i]));
		if (i + 1 != font_png.size()) {
			out << ',';
		}
	}

	out << "\n};\n\n";
	out << "inline constexpr FontBinary kDefaultFontBinary{\n";
	out << "\tstd::span{ kDefaultFontBytes }\n";
	out << "};\n\n";
	out << "} // namespace ptgn::impl\n";

	PTGN_ASSERT(out, "Failed to write generated default font header: ", header_path.string());
}

#if !defined(__EMSCRIPTEN__) && defined(PTGN_DEBUG)
[[maybe_unused]] impl::FontAtlas GenerateDefaultFontAtlas(Renderer& renderer) {
	auto font_path{ impl::GetBuildInfo().engine_directory / path{ kDefaultFontFile } };
	PTGN_ASSERT(FileExists(font_path), "Default font file does not exist: ", font_path.string());
	PTGN_ASSERT(
		impl::MatchesExtension<Font>(GetExtension(font_path)),
		"Default font file must have a valid extension: ", font_path.string()
	);

	auto directory{ impl::GetBuildInfo().engine_directory / path{ kDefaultFontCacheDirectory } };
	PTGN_ASSERT(IsDirectoryPath(directory.string()));
	auto font_png_path{ directory / font_path.filename().replace_extension("png") };

	impl::FontAtlas font_atlas{ renderer, font_path, font_png_path };
	WriteGeneratedDefaultFontHeader(font_png_path);
	return font_atlas;
}
#endif

impl::FontAtlas GetDefaultFontAtlas(Renderer& renderer) {
#if !defined(__EMSCRIPTEN__) && defined(PTGN_DEBUG)
	if constexpr (kGenerateDefaultFontAtlas) {
		return GenerateDefaultFontAtlas(renderer);
	} else {
		return impl::FontAtlas{ renderer, impl::kDefaultFontBinary };
	}
#else
	return impl::FontAtlas{ renderer, impl::kDefaultFontBinary };
#endif
}

} // namespace

FontSystem::FontSystem(Renderer& renderer, AssetManager& asset_manager) :
	asset_manager_{ asset_manager } {
	auto default_font{ GetDefaultFontAtlas(renderer) };

	Font font{ asset_manager_.CreateAsset(), true };
	font.GetEntity().Add<impl::FontAtlas>(std::move(default_font));
	impl::AddAssetKey(font.GetEntity(), kDefaultFont, std::nullopt);
}

FontSystem::~FontSystem() noexcept = default;

Font FontSystem::GetDefault() const {
	return asset_manager_.Get<Font>(default_font_);
}

void FontSystem::SetDefault(FontKey font_key) {
	PTGN_ASSERT(
		asset_manager_.Has<Font>(font_key), "Font key must be loaded before setting it as default"
	);
	default_font_ = std::move(font_key);
}

impl::FontAtlasData FontSystem::PrepareFontAtlas(const path& font_path) {
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

	static std::mutex prepare_mutex;
	std::scoped_lock lock{ prepare_mutex };

	if (impl::MatchesExtension<Texture>(extension)) {
		return impl::FontAtlas::PrepareCached(absolute_font_path);
	}

	auto cache_directory{ impl::GetBuildInfo().binary_directory / path{ kFontCacheDirectory } };
	auto cache_name{ absolute_font_path.stem().string() };
	auto cache_png_file{ cache_directory / (cache_name + ".png") };

	if (FileExists(cache_png_file)) {
		return impl::FontAtlas::PrepareCached(cache_png_file);
	}

	return impl::FontAtlas::PrepareGenerated(absolute_font_path, cache_png_file);
}

impl::FontAtlas FontSystem::CreateFontAtlas(Renderer& renderer, const path& font_path) {
	return impl::FontAtlas{ renderer, PrepareFontAtlas(font_path) };
}

} // namespace ptgn
