#include "runtime/asset/asset_manager.h"

#include <SDL3/SDL_error.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <filesystem>
#include <memory>
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
#include "renderer/renderer.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/audio/audio.h"
#include "serialization/json/json.h"

#ifdef CreateFont
#undef CreateFont
#endif
#include <functional>
#include <optional>

// TODO: Add async asset loading.

namespace ptgn {

AssetManager::AssetManager(impl::SDLInstance& sdl, Renderer& renderer) :
	sdl_{ sdl }, renderer_{ renderer } {}

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

static void AddKey(ecs::Entity asset, std::string_view key) {
	asset.Add<impl::AssetName>(key);
	asset.Add<impl::AssetKey>(Hash(key));
}

Shader AssetManager::LoadShader(
	std::string_view key, const std::variant<ShaderCode, path>& source,
	const std::string& shader_name
) {
	auto shader{ CreateShader(true, source, shader_name) };
	AddKey(shader.entity_, key);
	return shader;
}

Shader AssetManager::LoadShader(
	std::string_view key, const std::variant<ShaderCode, std::string>& vertex,
	const std::variant<ShaderCode, std::string>& fragment, const std::string& shader_name
) {
	auto shader{ CreateShader(true, vertex, fragment, shader_name) };
	AddKey(shader.entity_, key);
	return shader;
}

Texture AssetManager::LoadTexture(std::string_view key, const path& asset_path) {
	auto texture{ CreateTexture(true, asset_path) };
	AddKey(texture.entity_, key);
	return texture;
}

Font AssetManager::LoadFont(std::string_view key, const path& asset_path, float pt_size) {
	auto font{ CreateFont(true, asset_path, pt_size) };
	AddKey(font.entity_, key);
	return font;
}

Audio AssetManager::LoadAudio(std::string_view key, const path& asset_path) {
	auto audio{ CreateAudio(true, asset_path) };
	AddKey(audio.entity_, key);
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

} // namespace ptgn