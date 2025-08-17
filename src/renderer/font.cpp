#include "renderer/font.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "SDL_error.h"
#include "SDL_rwops.h"
#include "SDL_ttf.h"
#include "common/assert.h"
#include "components/generic.h"
#include "core/game.h"
#include "core/sdl_instance.h"
#include "math/vector2.h"
#include "resources/fonts.h"
#include "resources/resource_manager.h"
#include "serialization/fwd.h"
#include "serialization/json.h"
#include "utility/file.h"

namespace ptgn::impl {

void TTF_FontDeleter::operator()(TTF_Font* font) const {
	if (game.sdl_instance_->SDLTTFIsInitialized()) {
		TTF_CloseFont(font);
	}
}

FontManager::FontManager(FontManager&& other) noexcept : ResourceManager{ std::move(other) } {
	raw_default_font_ = std::exchange(other.raw_default_font_, nullptr);
}

FontManager& FontManager::operator=(FontManager&& other) noexcept {
	if (this != &other) {
		ResourceManager::operator=(std::move(other));
		raw_default_font_ = std::exchange(other.raw_default_font_, nullptr);
	}
	return *this;
}

FontManager::~FontManager() {
	if (game.sdl_instance_->SDLIsInitialized() && raw_default_font_) {
		SDL_RWclose(raw_default_font_);
	}
}

void FontManager::Load(const ResourceHandle& key, const path& filepath) {
	Load(key, filepath, default_font_size_);
}

void FontManager::Load(
	const ResourceHandle& key, const path& filepath, std::int32_t size, std::int32_t index
) {
	auto [it, inserted] = resources_.try_emplace(key);
	if (inserted || key == ResourceHandle{} /* Replacing default font */) {
		it->second.key		= key;
		it->second.filepath = filepath;
		it->second.resource = LoadFromFile(filepath, size, index);
	}
}

void FontManager::Load(
	const ResourceHandle& key, const FontBinary& binary, std::int32_t size, std::int32_t index
) {
	auto [it, inserted] = resources_.try_emplace(key);
	if (inserted) {
		it->second.key = key;
		// Not applicable: it->second.filepath
		it->second.resource = LoadFromBinary(binary, size, index);
	}
}

void FontManager::Init() {
	ResourceHandle key{};
	if (!raw_default_font_) {
		raw_default_font_ = GetRawBuffer(GetLiberationSansRegular());
		auto default_font{
			LoadFromBinary(raw_default_font_, default_font_size_, default_font_index_, false)
		};
		auto [it, inserted] = resources_.try_emplace(key);
		if (inserted) {
			it->second.key = key;
			// Not applicable: it->second.filepath
			it->second.resource = Font{ default_font };
		}
	}
	PTGN_ASSERT(Has(key), "Failed to initialize default font");
	SetDefault(key);
}

int FontManager::GetLineSkip(const ResourceHandle& key, const FontSize& font_size) const {
	return TTF_FontLineSkip(Get(key, font_size).get());
}

TemporaryFont FontManager::Get(const ResourceHandle& key, const FontSize& font_size) const {
	PTGN_ASSERT(Has(key), "Cannot get font which has not been loaded");

	auto& resource_info{ resources_.find(key)->second };

	if (font_size == FontSize{}) {
		return TemporaryFont{ resource_info.resource.get(), [](TTF_Font*) {
							 } };
	}

	if (!resource_info.filepath.empty()) {
		auto path_string{ resource_info.filepath.string() };
		return TemporaryFont{ TTF_OpenFont(path_string.c_str(), font_size), TTF_FontDeleter{} };
	}

	// Font has no path defined.
	PTGN_ASSERT(
		key == ResourceHandle{}, "Font key must have a valid path unless it is the default font"
	);
	return TemporaryFont{ LoadFromBinary(raw_default_font_, font_size, default_font_index_, false),
						  TTF_FontDeleter{} };
}

V2_int FontManager::GetSize(
	const ResourceHandle& key, const std::string& content, const FontSize& font_size
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

void FontManager::SetDefault(const ResourceHandle& key) {
	PTGN_ASSERT(Has(key), "Font key must be loaded before setting it as default");
	default_key_ = key;
}

FontSize FontManager::GetHeight(const ResourceHandle& key, const FontSize& font_size) const {
	return TTF_FontHeight(Get(key, font_size).get());
}

Font FontManager::LoadFromFile(const path& filepath, std::int32_t size, std::int32_t index) {
	PTGN_ASSERT(
		FileExists(filepath), "Cannot load font with nonexistent path: ", filepath.string()
	);
	Font ptr{ TTF_OpenFontIndex(filepath.string().c_str(), size, index) };
	PTGN_ASSERT(ptr != nullptr, TTF_GetError());
	return ptr;
}

Font FontManager::LoadFromFile(const path& filepath) {
	return LoadFromFile(filepath, default_font_size_, default_font_index_);
}

TTF_Font* FontManager::LoadFromBinary(
	SDL_RWops* raw_buffer, std::int32_t size, std::int32_t index, bool free_buffer
) {
	PTGN_ASSERT(raw_buffer != nullptr, SDL_GetError());
	TTF_Font* ptr{ TTF_OpenFontIndexRW(raw_buffer, static_cast<int>(free_buffer), size, index) };
	PTGN_ASSERT(ptr != nullptr, TTF_GetError());
	return ptr;
}

Font FontManager::LoadFromBinary(const FontBinary& binary, std::int32_t size, std::int32_t index) {
	auto raw_buffer{ GetRawBuffer(binary) };
	return Font{ LoadFromBinary(raw_buffer, size, index, true) };
}

SDL_RWops* FontManager::GetRawBuffer(const FontBinary& binary) {
	PTGN_ASSERT(binary.buffer != nullptr, "Cannot load font from invalid binary");
	return SDL_RWFromMem(
		static_cast<void*>(binary.buffer), static_cast<std::int32_t>(binary.length)
	);
}

void to_json(json& j, const FontManager& manager) {
	to_json(j, static_cast<const FontManager::ParentManager&>(manager));
}

void from_json(const json& j, FontManager& manager) {
	from_json(j, static_cast<FontManager::ParentManager&>(manager));
	manager.Init();
}

} // namespace ptgn::impl