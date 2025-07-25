#include "rendering/resources/font.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "SDL_error.h"
#include "SDL_rwops.h"
#include "SDL_ttf.h"
#include "common/assert.h"
#include "core/game.h"
#include "core/sdl_instance.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "resources/fonts.h"
#include "utility/file.h"

namespace ptgn::impl {

void TTF_FontDeleter::operator()(TTF_Font* font) const {
	if (game.sdl_instance_->SDLTTFIsInitialized()) {
		TTF_CloseFont(font);
	}
}

void FontManager::Init() {
	raw_default_font_ = GetRawBuffer(LiberationSansRegular);
	auto default_font{ LoadFromBinary(raw_default_font_, 20, 0, false) };
	fonts_.try_emplace(FontKey{}, Font{ default_font });
	SetDefault(FontKey{});
}

int FontManager::GetLineSkip(const FontKey& key, const FontSize& font_size) const {
	return TTF_FontLineSkip(Get(key, font_size).get());
}

std::shared_ptr<TTF_Font> FontManager::Get(const FontKey& key, const FontSize& font_size) const {
	PTGN_ASSERT(Has(key), "Cannot get font which has not been loaded");

	if (font_size == FontSize{}) {
		return std::shared_ptr<TTF_Font>{ fonts_.find(key)->second.get(), [](TTF_Font*) {
										 } };
	}

	auto it{ font_paths_.find(key) };
	if (it != font_paths_.end()) {
		return std::shared_ptr<TTF_Font>{ TTF_OpenFont(it->second.c_str(), font_size),
										  TTF_FontDeleter{} };
	}

	// Font has no path defined.
	PTGN_ASSERT(key == FontKey{}, "Font key must have a valid path unless it is the default font");
	return std::shared_ptr<TTF_Font>{ LoadFromBinary(raw_default_font_, font_size, 0, false),
									  TTF_FontDeleter{} };
}

V2_int FontManager::GetSize(
	const FontKey& key, const std::string& content, const FontSize& font_size
) const {
	V2_int size;

	auto font{ Get(key, font_size) };

	if (content.empty()) {
		size.x = 0;
		size.y = GetHeight(key, font_size);
		return size;
	}

	TTF_SizeUTF8(font.get(), content.c_str(), &size.x, &size.y);
	return size;
}

void FontManager::SetDefault(const FontKey& key) {
	PTGN_ASSERT(Has(key), "Font key must be loaded before setting it as default");
	default_key_ = key;
}

std::int32_t FontManager::GetHeight(const FontKey& key, const FontSize& font_size) const {
	return TTF_FontHeight(Get(key, font_size).get());
}

bool FontManager::Has(const FontKey& key) const {
	return fonts_.find(key) != fonts_.end();
}

FontManager::Font FontManager::LoadFromFile(
	const path& filepath, std::int32_t size, std::int32_t index
) {
	PTGN_ASSERT(
		FileExists(filepath), "Cannot load font with nonexistent path: ", filepath.string()
	);
	Font ptr{ TTF_OpenFontIndex(filepath.string().c_str(), size, index) };
	PTGN_ASSERT(ptr != nullptr, TTF_GetError());
	return ptr;
}

TTF_Font* FontManager::LoadFromBinary(
	SDL_RWops* raw_buffer, std::int32_t size, std::int32_t index, bool free_buffer
) {
	PTGN_ASSERT(raw_buffer != nullptr, SDL_GetError());
	TTF_Font* ptr{ TTF_OpenFontIndexRW(raw_buffer, static_cast<int>(free_buffer), size, index) };
	PTGN_ASSERT(ptr != nullptr, TTF_GetError());
	return ptr;
}

FontManager::Font FontManager::LoadFromBinary(
	const FontBinary& binary, std::int32_t size, std::int32_t index
) {
	auto raw_buffer{ GetRawBuffer(binary) };
	return Font{ LoadFromBinary(raw_buffer, size, index, true) };
}

SDL_RWops* FontManager::GetRawBuffer(const FontBinary& binary) {
	PTGN_ASSERT(binary.buffer != nullptr, "Cannot load font from invalid binary");
	return SDL_RWFromMem(
		static_cast<void*>(binary.buffer), static_cast<std::int32_t>(binary.length)
	);
}

void FontManager::Load(
	const FontKey& key, const path& filepath, std::int32_t size, std::int32_t index
) {
	font_paths_.try_emplace(key, filepath.string());
	fonts_.try_emplace(key, LoadFromFile(filepath, size, index));
}

void FontManager::Load(
	const FontKey& key, const FontBinary& binary, std::int32_t size, std::int32_t index
) {
	fonts_.try_emplace(key, LoadFromBinary(binary, size, index));
}

void FontManager::Unload(const FontKey& key) {
	fonts_.erase(key);
	font_paths_.erase(key);
}

} // namespace ptgn::impl