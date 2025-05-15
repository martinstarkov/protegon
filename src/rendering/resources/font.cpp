#include "rendering/resources/font.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "common/assert.h"
#include "core/game.h"
#include "core/sdl_instance.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "resources/fonts.h"
#include "SDL_error.h"
#include "SDL_rwops.h"
#include "SDL_ttf.h"
#include "utility/file.h"

namespace ptgn::impl {

void TTF_FontDeleter::operator()(TTF_Font* font) const {
	if (game.sdl_instance_->SDLTTFIsInitialized()) {
		TTF_CloseFont(font);
	}
}

void FontManager::Init() {
	Load(FontKey{}, LiberationSansRegular, 20);
	SetDefault(FontKey{});
}

V2_int FontManager::GetSize(const FontKey& key, const std::string& content) const {
	PTGN_ASSERT(Has(key), "Cannot get size of font which has not been loaded");
	V2_int size;
	TTF_SizeUTF8(Get(key), content.c_str(), &size.x, &size.y);
	return size;
}

void FontManager::SetDefault(const FontKey& key) {
	PTGN_ASSERT(Has(key), "Font key must be loaded before setting it as default");
	default_key_ = key;
}

TTF_Font* FontManager::Get(const FontKey& key) const {
	PTGN_ASSERT(Has(key), "Cannot get font key which is not loaded");
	return fonts_.find(key)->second.get();
}

std::int32_t FontManager::GetHeight(const FontKey& key) const {
	return TTF_FontHeight(Get(key));
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

FontManager::Font FontManager::LoadFromBinary(
	const FontBinary& binary, std::int32_t size, std::int32_t index
) {
	PTGN_ASSERT(binary.buffer != nullptr, "Cannot load font from invalid binary");
	SDL_RWops* rw{
		SDL_RWFromMem(static_cast<void*>(binary.buffer), static_cast<std::int32_t>(binary.length))
	};
	PTGN_ASSERT(rw != nullptr, SDL_GetError());
	Font ptr{ TTF_OpenFontIndexRW(rw, 1, size, index) };
	PTGN_ASSERT(ptr != nullptr, TTF_GetError());
	return ptr;
}

void FontManager::Load(
	const FontKey& key, const path& filepath, std::int32_t size, std::int32_t index
) {
	fonts_.try_emplace(key, LoadFromFile(filepath, size, index));
}

void FontManager::Load(
	const FontKey& key, const FontBinary& binary, std::int32_t size, std::int32_t index
) {
	fonts_.try_emplace(key, LoadFromBinary(binary, size, index));
}

void FontManager::Unload(const FontKey& key) {
	fonts_.erase(key);
}

} // namespace ptgn::impl