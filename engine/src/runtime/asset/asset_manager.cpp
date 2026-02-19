#include "runtime/asset/asset_manager.h"

#include <SDL3/SDL_error.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "app/application.h"
#include "core/assert.h"
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
#include "runtime/asset/asset_handle.h"
#include "runtime/asset/audio_asset.h"
#include "runtime/asset/font_asset.h"
#include "runtime/asset/shader_asset.h"
#include "runtime/asset/texture_asset.h"
#include "runtime/audio/audio.h"
#include "serialization/json/json.h"

// TODO: Add async asset loading.

namespace ptgn {

AssetManager::AssetManager(impl::SDLInstance& sdl, Renderer& renderer) :
	sdl_{ sdl }, renderer_{ renderer } {}

Shader AssetManager::LoadShader(
	const std::variant<ShaderCode, path>& source, const std::string& shader_name
) {
	Shader shader{ CreateAsset() };
	shader.entity_.Add<impl::ShaderObject>(
		renderer_.gl_renderer_.get(),
		renderer_.gl_renderer_->gl->shaders.CreateProgram(source, shader_name)
	);
	return shader;
}

Shader AssetManager::LoadShader(
	const std::variant<ShaderCode, std::string>& vertex,
	const std::variant<ShaderCode, std::string>& fragment, const std::string& shader_name
) {
	Shader shader{ CreateAsset() };
	shader.entity_.Add<impl::ShaderObject>(
		renderer_.gl_renderer_.get(),
		renderer_.gl_renderer_->gl->shaders.CreateProgram(vertex, fragment, shader_name)
	);
	return shader;
}

Texture AssetManager::LoadTexture(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create texture from invalid path: ", asset_path.string()
	);

	impl::Surface surface{ asset_path };

	Texture texture{ CreateAsset() };
	texture.entity_.Add<impl::TextureObject>(
		renderer_.gl_renderer_.get(),
		renderer_.gl_renderer_->gl->textures.CreateTexture(
			surface.pixels.data(), GL_RGBA, GL_UNSIGNED_BYTE, surface.size, GL_RGBA
		)
	);

	return texture;
}

Font AssetManager::LoadFont(const path& asset_path, float pt_size) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create font from invalid path: ", asset_path.string()
	);

	auto ttf_font = TTF_OpenFont(asset_path.string().c_str(), pt_size);

	PTGN_ASSERT(ttf_font, SDL_GetError());

	std::shared_ptr<TTF_Font> f{ ttf_font, impl::TTF_FontDeleter{} };

	Font font{ CreateAsset() };
	font.entity_.Add<impl::FontSize>(pt_size);
	font.entity_.Add<std::shared_ptr<TTF_Font>>(f);

	return font;
}

Audio AssetManager::LoadAudio(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create audio from invalid path: ", asset_path.string()
	);

	PTGN_ASSERT(sdl_.mixer_, "Cannot load audio when SDL_mixer has not been created");

	auto mix_audio = MIX_LoadAudio(sdl_.mixer_, asset_path.string().c_str(), true);

	PTGN_ASSERT(mix_audio, SDL_GetError());

	std::shared_ptr<MIX_Audio> a{ mix_audio, impl::MIX_AudioDeleter{} };

	Audio audio{ CreateAsset() };
	audio.entity_.Add<std::shared_ptr<MIX_Audio>>(a);

	return audio;
}

json AssetManager::LoadJson(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create json from invalid path: ", asset_path.string()
	);

	json j{ ptgn::LoadJson(asset_path) };

	/*
	// TODO: Make json an asset
	JsonAsset json_asset{ CreateAsset() };
	json_asset.entity_.Add<json>(j);
	return json_asset;
	*/

	return j;
}

Shader AssetManager::LoadShader(
	std::string_view key, const std::variant<ShaderCode, path>& source,
	const std::string& shader_name
) {
	auto shader{ LoadShader(source, shader_name) };
	shader.entity_.Add<impl::PersistentTag>();
	shader.entity_.Add<impl::AssetName>(key);
	shader.entity_.Add<impl::AssetKey>(Hash(key));
	return shader;
}

Shader AssetManager::LoadShader(
	std::string_view key, const std::variant<ShaderCode, std::string>& vertex,
	const std::variant<ShaderCode, std::string>& fragment, const std::string& shader_name
) {
	auto shader{ LoadShader(vertex, fragment, shader_name) };
	shader.entity_.Add<impl::PersistentTag>();
	shader.entity_.Add<impl::AssetName>(key);
	shader.entity_.Add<impl::AssetKey>(Hash(key));
	return shader;
}

Texture AssetManager::LoadTexture(std::string_view key, const path& asset_path) {
	auto texture{ LoadTexture(asset_path) };
	texture.entity_.Add<impl::PersistentTag>();
	texture.entity_.Add<impl::AssetName>(key);
	texture.entity_.Add<impl::AssetKey>(Hash(key));
	return texture;
}

Font AssetManager::LoadFont(std::string_view key, const path& asset_path, float pt_size) {
	auto font{ LoadFont(asset_path, pt_size) };
	font.entity_.Add<impl::PersistentTag>();
	font.entity_.Add<impl::AssetName>(key);
	font.entity_.Add<impl::AssetKey>(Hash(key));
	return font;
}

Audio AssetManager::LoadAudio(std::string_view key, const path& asset_path) {
	auto audio{ LoadAudio(asset_path) };
	audio.entity_.Add<impl::PersistentTag>();
	audio.entity_.Add<impl::AssetName>(key);
	audio.entity_.Add<impl::AssetKey>(Hash(key));
	return audio;
}

json AssetManager::LoadJson(std::string_view key, const path& asset_path) {
	return LoadJson(asset_path);
	// TODO: Add these back once json is an asset.
	// auto json{ LoadJson(asset_path) };
	// json.entity_.Add<impl::PersistentTag>();
	// json.entity_.Add<impl::AssetName>(key);
	// json.entity_.Add<impl::AssetKey>(Hash(key));
	// return json;
}

void AssetManager::UnloadAudio(std::string_view key) {
	auto hash{ Hash(key) };
	for (auto [entity, k, r] :
		 manager_.EntitiesWith<impl::AssetKey, std::shared_ptr<MIX_Audio>>()) {
		if (k.hash == hash) {
			entity.Destroy();
		}
	}
	manager_.Refresh();
}

void AssetManager::UnloadJson(std::string_view key) {
	auto hash{ Hash(key) };
	for (auto [entity, k, r] : manager_.EntitiesWith<impl::AssetKey, json>()) {
		if (k.hash == hash) {
			entity.Destroy();
		}
	}
	manager_.Refresh();
}

void AssetManager::UnloadShader(std::string_view key) {
	auto hash{ Hash(key) };
	for (auto [entity, k, r] : manager_.EntitiesWith<impl::AssetKey, impl::ShaderObject>()) {
		if (k.hash == hash) {
			entity.Destroy();
		}
	}
	manager_.Refresh();
}

void AssetManager::UnloadTexture(std::string_view key) {
	auto hash{ Hash(key) };
	for (auto [entity, k, r] : manager_.EntitiesWith<impl::AssetKey, impl::TextureObject>()) {
		if (k.hash == hash) {
			entity.Destroy();
		}
	}
	manager_.Refresh();
}

void AssetManager::UnloadFont(std::string_view key) {
	auto hash{ Hash(key) };
	for (auto [entity, k, r] : manager_.EntitiesWith<impl::AssetKey, std::shared_ptr<TTF_Font>>()) {
		if (k.hash == hash) {
			entity.Destroy();
		}
	}
	manager_.Refresh();
}

ecs::Entity AssetManager::CreateAsset() {
	auto asset{ manager_.CreateEntity() };
	manager_.Refresh();
	return asset;
}

} // namespace ptgn