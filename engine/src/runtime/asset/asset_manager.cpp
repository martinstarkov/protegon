#include "runtime/asset/asset_manager.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "app/application.h"
#include "core/assert.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "ecs/ecs.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_renderer.h"
#include "renderer/backend/gl/gl_shader.h"
#include "renderer/image/surface.h"
#include "renderer/primitives/font.h"
#include "renderer/primitives/fonts.h"
#include "renderer/renderer.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/audio/audio.h"
#include "serialization/json/json.h"

#ifdef CreateFont
#undef CreateFont
#endif
#include <cstdint>

#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "renderer/primitives/text.h"

// TODO: Add async asset loading.

namespace ptgn {

static void AddKey(ecs::Entity asset, std::string_view key, std::optional<path> path) {
	asset.Add<impl::AssetName>(key);
	asset.Add<impl::AssetKey>(Hash(key));
	if (path.has_value()) {
		asset.Add<ptgn::path>(*path);
	}
}

static TTF_Font* LoadFromBinary(SDL_IOStream* raw_buffer, float font_size, bool free_buffer) {
	PTGN_ASSERT(raw_buffer != nullptr, SDL_GetError());
	auto ptr{ TTF_OpenFontIO(raw_buffer, free_buffer, font_size) };
	PTGN_ASSERT(ptr != nullptr, SDL_GetError());
	return ptr;
}

static SDL_IOStream* GetRawBuffer(const FontBinary& binary) {
	PTGN_ASSERT(binary.buffer != nullptr, "Cannot load font from invalid binary");
	return SDL_IOFromMem(
		static_cast<void*>(binary.buffer), static_cast<std::int32_t>(binary.length)
	);
}

AssetManager::AssetManager(impl::SDLInstance& sdl, Renderer& renderer) :
	sdl_{ sdl }, renderer_{ renderer } {
	constexpr std::string_view key{ "" };
	constexpr auto hash{ Hash(key) };
	if (!raw_default_font_) {
		raw_default_font_ = GetRawBuffer(impl::GetLiberationSansRegular());
		auto default_font{ LoadFromBinary(raw_default_font_, default_font_size, false) };
		std::shared_ptr<TTF_Font> f{ default_font, impl::TTF_FontDeleter{} };

		Font font{ CreateAsset(), true };
		font.entity_.Add<impl::FontSize>(default_font_size);
		font.entity_.Add<std::shared_ptr<TTF_Font>>(f);
		AddKey(font.entity_, key, {});
	}
	default_font_key_ = hash;
}

AssetManager::~AssetManager() noexcept {
	if (raw_default_font_) {
		SDL_CloseIO(raw_default_font_);
	}
}

Shader AssetManager::CreateShader(
	bool persistent, const std::variant<ShaderCode, path>& source, const std::string& shader_name
) {
	Shader shader{ CreateAsset(), persistent };
	shader.entity_.Add<impl::ShaderObject>(
		renderer_.gl_renderer_.get(),
		renderer_.gl_renderer_->gl->shaders.CreateProgram(source, shader_name)
	);
	return shader;
}

Shader AssetManager::CreateShader(
	bool persistent, const std::variant<ShaderCode, std::string>& vertex,
	const std::variant<ShaderCode, std::string>& fragment, const std::string& shader_name
) {
	Shader shader{ CreateAsset(), persistent };
	shader.entity_.Add<impl::ShaderObject>(
		renderer_.gl_renderer_.get(),
		renderer_.gl_renderer_->gl->shaders.CreateProgram(vertex, fragment, shader_name)
	);
	return shader;
}

Texture AssetManager::CreateTexture(bool persistent, const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create texture from invalid path: ", asset_path.string()
	);

	impl::Surface surface{ asset_path };

	Texture texture{ CreateAsset(), persistent };
	texture.entity_.Add<impl::TextureObject>(
		renderer_.gl_renderer_.get(),
		renderer_.gl_renderer_->gl->textures.CreateTexture(
			surface.pixels.data(), GL_RGBA, GL_UNSIGNED_BYTE, surface.size, GL_RGBA
		)
	);

	return texture;
}

Font AssetManager::CreateFont(bool persistent, const path& asset_path, float pt_size) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create font from invalid path: ", asset_path.string()
	);

	auto ttf_font = TTF_OpenFont(asset_path.string().c_str(), pt_size);

	PTGN_ASSERT(ttf_font, SDL_GetError());

	std::shared_ptr<TTF_Font> f{ ttf_font, impl::TTF_FontDeleter{} };

	Font font{ CreateAsset(), persistent };
	font.entity_.Add<impl::FontSize>(pt_size);
	font.entity_.Add<std::shared_ptr<TTF_Font>>(f);

	return font;
}

Audio AssetManager::CreateAudio(bool persistent, const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create audio from invalid path: ", asset_path.string()
	);

	PTGN_ASSERT(sdl_.mixer_, "Cannot load audio when SDL_mixer has not been created");

	auto mix_audio = MIX_LoadAudio(sdl_.mixer_, asset_path.string().c_str(), true);

	PTGN_ASSERT(mix_audio, SDL_GetError());

	std::shared_ptr<MIX_Audio> a{ mix_audio, impl::MIX_AudioDeleter{} };

	Audio audio{ CreateAsset(), persistent };
	audio.entity_.Add<std::shared_ptr<MIX_Audio>>(a);

	return audio;
}

Shader AssetManager::CreateShader(
	const std::variant<ShaderCode, path>& source, const std::string& shader_name
) {
	return CreateShader(false, source, shader_name);
}

Shader AssetManager::CreateShader(
	const std::variant<ShaderCode, std::string>& vertex,
	const std::variant<ShaderCode, std::string>& fragment, const std::string& shader_name
) {
	return CreateShader(false, vertex, fragment, shader_name);
}

Texture AssetManager::CreateTexture(const path& asset_path) {
	return CreateTexture(false, asset_path);
}

Font AssetManager::CreateFont(const path& asset_path, float pt_size) {
	return CreateFont(false, asset_path, pt_size);
}

Audio AssetManager::CreateAudio(const path& asset_path) {
	return CreateAudio(false, asset_path);
}

json AssetManager::CreateJson(const path& asset_path) {
	return ptgn::LoadJson(asset_path);
}

Shader AssetManager::LoadShader(
	std::string_view key, const std::variant<ShaderCode, path>& source,
	const std::string& shader_name
) {
	auto shader{ CreateShader(true, source, shader_name) };
	AddKey(shader.entity_, key, {});
	return shader;
}

Shader AssetManager::LoadShader(
	std::string_view key, const std::variant<ShaderCode, std::string>& vertex,
	const std::variant<ShaderCode, std::string>& fragment, const std::string& shader_name
) {
	auto shader{ CreateShader(true, vertex, fragment, shader_name) };
	AddKey(shader.entity_, key, {});
	return shader;
}

Texture AssetManager::LoadTexture(std::string_view key, const path& asset_path) {
	auto texture{ CreateTexture(true, asset_path) };
	AddKey(texture.entity_, key, asset_path);
	return texture;
}

Font AssetManager::LoadFont(std::string_view key, const path& asset_path, float pt_size) {
	auto font{ CreateFont(true, asset_path, pt_size) };
	AddKey(font.entity_, key, asset_path);
	return font;
}

Audio AssetManager::LoadAudio(std::string_view key, const path& asset_path) {
	auto audio{ CreateAudio(true, asset_path) };
	AddKey(audio.entity_, key, asset_path);
	return audio;
}

json& AssetManager::LoadJson(std::string_view key, const path& asset_path) {
	auto [it, _] = jsons_.insert_or_assign(Hash(key), ptgn::LoadJson(asset_path));
	return it->second;
}

template <typename ResourceComponent>
bool UnloadAssetImpl(ecs::Manager& manager, std::string_view key) {
	auto hash{ Hash(key) };

	bool unloaded{ false };

	for (auto [entity, k, resource] : manager.EntitiesWith<impl::AssetKey, ResourceComponent>()) {
		if (k.hash == hash) {
			unloaded = true;
			entity.Destroy();
		}
	}

	manager.Refresh();
	return unloaded;
}

bool AssetManager::UnloadAudio(std::string_view key) {
	return UnloadAssetImpl<std::shared_ptr<MIX_Audio>>(manager_, key);
}

bool AssetManager::UnloadJson(std::string_view key) {
	return jsons_.erase(Hash(key)) != 0;
}

bool AssetManager::UnloadShader(std::string_view key) {
	return UnloadAssetImpl<impl::ShaderObject>(manager_, key);
}

bool AssetManager::UnloadTexture(std::string_view key) {
	return UnloadAssetImpl<impl::TextureObject>(manager_, key);
}

bool AssetManager::UnloadFont(std::string_view key) {
	return UnloadAssetImpl<std::shared_ptr<TTF_Font>>(manager_, key);
}

ecs::Entity AssetManager::CreateAsset() {
	auto asset{ manager_.CreateEntity() };
	manager_.Refresh();
	return asset;
}

template <typename ResourceComponent, typename HandleType>
std::optional<HandleType> GetAssetImpl(const ecs::Manager& manager, std::string_view key) {
	auto hash{ Hash(key) };

	for (auto [entity, k, resource] : manager.EntitiesWith<impl::AssetKey, ResourceComponent>()) {
		if (k.hash == hash) {
			return HandleType{ entity, true };
		}
	}

	return std::nullopt;
}

std::optional<Audio> AssetManager::GetAudio(std::string_view key) const {
	return GetAssetImpl<std::shared_ptr<MIX_Audio>, Audio>(manager_, key);
}

std::optional<Shader> AssetManager::GetShader(std::string_view key) const {
	return GetAssetImpl<impl::ShaderObject, Shader>(manager_, key);
}

std::optional<Texture> AssetManager::GetTexture(std::string_view key) const {
	return GetAssetImpl<impl::TextureObject, Texture>(manager_, key);
}

std::optional<Font> AssetManager::GetFont(std::string_view key) const {
	return GetAssetImpl<std::shared_ptr<TTF_Font>, Font>(manager_, key);
}

Font AssetManager::GetDefaultFont() const {
	for (auto [entity, k, resource] :
		 manager_.EntitiesWith<impl::AssetKey, std::shared_ptr<TTF_Font>>()) {
		if (k.hash == default_font_key_) {
			return Font{ entity, true };
		}
	}
	PTGN_ERROR("Failed to find default font from asset manager");
}

std::optional<std::reference_wrapper<const json>> AssetManager::GetJson(std::string_view key
) const {
	auto hash{ Hash(key) };

	auto it = jsons_.find(hash);
	if (it == jsons_.end()) {
		return std::nullopt;
	}

	return std::cref(it->second);
}

std::optional<std::reference_wrapper<json>> AssetManager::GetJson(std::string_view key) {
	auto hash{ Hash(key) };

	auto it = jsons_.find(hash);
	if (it == jsons_.end()) {
		return std::nullopt;
	}

	return std::ref(it->second);
}

template <typename ResourceComponent>
bool HasAssetImpl(const ecs::Manager& manager, std::string_view key) {
	auto hash{ Hash(key) };

	for (auto [entity, k, resource] : manager.EntitiesWith<impl::AssetKey, ResourceComponent>()) {
		if (k.hash == hash) {
			return true;
		}
	}

	return false;
}

bool AssetManager::HasJson(std::string_view key) const {
	return jsons_.contains(Hash(key));
}

bool AssetManager::HasAudio(std::string_view key) const {
	return HasAssetImpl<std::shared_ptr<MIX_Audio>>(manager_, key);
}

bool AssetManager::HasShader(std::string_view key) const {
	return HasAssetImpl<impl::ShaderObject>(manager_, key);
}

bool AssetManager::HasTexture(std::string_view key) const {
	return HasAssetImpl<impl::TextureObject>(manager_, key);
}

bool AssetManager::HasFont(std::string_view key) const {
	return HasAssetImpl<std::shared_ptr<TTF_Font>>(manager_, key);
}

void AssetManager::SetDefaultFont(std::string_view key) {
	PTGN_ASSERT(HasFont(key), "Font key must be loaded before setting it as default");
	default_font_key_ = Hash(key);
}

std::shared_ptr<TTF_Font> AssetManager::GetFont(
	std::string_view key, std::optional<float> font_size
) const {
	auto font{ GetFont(key) };

	if (!font) {
		return std::shared_ptr<TTF_Font>{ LoadFromBinary(raw_default_font_, *font_size, false),
										  impl::TTF_FontDeleter{} };
	}

	auto entity{ (*font).entity_ };

	if (!font_size.has_value()) {
		return entity.Get<std::shared_ptr<TTF_Font>>();
	}

	if (entity.Has<path>()) {
		auto path_string{ entity.Get<path>().string() };
		PTGN_ASSERT(!path_string.empty(), "Invalid font path");
		return std::shared_ptr<TTF_Font>{ TTF_OpenFont(path_string.c_str(), *font_size),
										  impl::TTF_FontDeleter{} };
	}

	// Font has no path defined.
	PTGN_ASSERT(
		entity.Get<impl::AssetKey>().hash == Hash(""),
		"Font key must have a valid path unless it is the default font"
	);

	return std::shared_ptr<TTF_Font>{ LoadFromBinary(raw_default_font_, *font_size, false),
									  impl::TTF_FontDeleter{} };
}

int AssetManager::GetFontLineSkip(std::string_view key, std::optional<float> font_size) const {
	return TTF_GetFontLineSkip(GetFont(key, font_size).get());
}

int AssetManager::GetFontHeight(std::string_view key, std::optional<float> font_size) const {
	return TTF_GetFontHeight(GetFont(key, font_size).get());
}

V2_int AssetManager::GetFontSize(
	std::string_view key, std::string_view text_content, std::optional<float> font_size,
	int max_wrap_width
) const {
	V2_int size;

	if (text_content.empty()) {
		size.x = 0;
		size.y = GetFontHeight(key, font_size);
		return size;
	}

	auto success{ TTF_GetStringSizeWrapped(
		GetFont(key, font_size).get(), text_content.data(), text_content.length(), max_wrap_width,
		&size.x, &size.y
	) };

	PTGN_ASSERT(success, "Failed to get size of wrapped font string");

	return size;
}

V2_int AssetManager::GetFontSize(
	Font font, std::string_view text_content, std::optional<float> font_size, int max_wrap_width
) const {
	return GetFontSize(
		font.entity_ ? font.entity_.Get<impl::AssetName>().name : "", text_content, font_size,
		max_wrap_width
	);
}

Texture AssetManager::CreateTextTexture(
	std::string_view text_content, Color color, float font_size, Font font_asset,
	const TextProperties& properties
) {
	Texture texture{ CreateAsset(), false };

	if (text_content.empty()) {
		return texture;
	}

	if (!font_asset.IsValid()) {
		font_asset = GetDefaultFont();
	}

	PTGN_ASSERT(font_asset.IsValid());

	PTGN_ASSERT(font_asset.entity_.Has<std::shared_ptr<TTF_Font>>());

	auto font{ font_asset.entity_.Get<std::shared_ptr<TTF_Font>>().get() };

	PTGN_ASSERT(font != nullptr, "Cannot create texture for text with nullptr font");

	TTF_SetFontStyle(font, std::to_underlying(properties.style));

	TTF_SetFontWrapAlignment(font, static_cast<TTF_HorizontalAlignment>(properties.justify));

	if (properties.line_skip.GetValue().has_value()) {
		TTF_SetFontLineSkip(font, *properties.line_skip.GetValue());
	}

	PTGN_ASSERT(font_size > 0, "Font size must be greater than zero");
	PTGN_ASSERT(
		font_size < 10000, "Font size exceeds maximum allowable font size or grew recursively"
	);

	TTF_SetFontSize(font, static_cast<float>(static_cast<int>(font_size)));

	SDL_Color text_color{ color.r, color.g, color.b, color.a };

	PTGN_ASSERT(properties.outline.width >= 0, "Cannot have negative font outline width");

	SDL_Surface* outline_surface{ nullptr };

	if (properties.outline.width != 0 && properties.outline.color != color::Transparent) {
		PTGN_ASSERT(
			properties.render_mode == FontRenderMode::Blended,
			"Font render mode must be set to blended when drawing text with outline"
		);
		TTF_SetFontOutline(font, properties.outline.width);

		SDL_Color outline_color{ properties.outline.color.r, properties.outline.color.g,
								 properties.outline.color.b, properties.outline.color.a };

		outline_surface = TTF_RenderText_Blended_Wrapped(
			font, text_content.data(), text_content.length(), outline_color, properties.wrap_after
		);

		PTGN_ASSERT(outline_surface != nullptr, "Failed to create text outline");

		TTF_SetFontOutline(font, 0);
	}

	SDL_Surface* surface{ nullptr };

	switch (properties.render_mode) {
		case FontRenderMode::Solid:
			surface = TTF_RenderText_Solid_Wrapped(
				font, text_content.data(), text_content.length(), text_color, properties.wrap_after
			);
			break;
		case FontRenderMode::Shaded: {
			SDL_Color shading_color{ properties.shading_color.r, properties.shading_color.g,
									 properties.shading_color.b, properties.shading_color.a };
			surface = TTF_RenderText_Shaded_Wrapped(
				font, text_content.data(), text_content.length(), text_color, shading_color,
				properties.wrap_after
			);
			break;
		}
		case FontRenderMode::Blended:
			surface = TTF_RenderText_Blended_Wrapped(
				font, text_content.data(), text_content.length(), text_color, properties.wrap_after
			);
			break;
		default:
			PTGN_ERROR("Unrecognized render mode given when creating surface from font information"
			);
	}

	PTGN_ASSERT(surface != nullptr, "Failed to create surface for given font information");

	if (outline_surface) {
		SDL_Rect rect{ properties.outline.width, properties.outline.width, surface->w, surface->h };

		SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
		SDL_BlitSurface(surface, NULL, outline_surface, &rect);
		SDL_DestroySurface(surface);

		surface = outline_surface;
	}

	PTGN_ASSERT(surface != nullptr, "Failed to blit text surface to text outline surface");

	impl::Surface s{ surface };

	texture.entity_.Add<impl::TextureObject>(
		renderer_.gl_renderer_.get(),
		renderer_.gl_renderer_->gl->textures.CreateTexture(
			s.pixels.data(), GL_RGBA, GL_UNSIGNED_BYTE, s.size, GL_RGBA
		)
	);

	return texture;
}

} // namespace ptgn