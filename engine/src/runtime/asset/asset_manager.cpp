#include "runtime/asset/asset_manager.h"

#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "app/application.h"
#include "core/assert.h"
#include "core/util/file.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/image/surface.h"
#include "renderer/resources/shader.h"
#include "runtime/asset/asset.h"
#include "runtime/asset/asset_handle.h"
#include "serialization/json/json.h"

// TODO: Add async asset loading.

namespace ptgn {

AssetManager::AssetManager(impl::SDLInstance& sdl, impl::gl::GLContext& gl) :
	sdl_{ sdl }, gl_{ gl } {}

Handle<Shader> AssetManager::LoadShader(
	std::variant<ShaderCode, path> source, const std::string& shader_name
) {
	auto shader = gl_.CreateShader(source, shader_name);

	return Handle<Shader>{ std::make_shared<impl::ShaderAsset>(std::move(shader)) };
}

Handle<Shader> AssetManager::LoadShader(
	std::variant<ShaderCode, std::string> vertex, std::variant<ShaderCode, std::string> fragment,
	const std::string& shader_name
) {
	auto shader = gl_.CreateShader(vertex, fragment, shader_name);

	return Handle<Shader>{ std::make_shared<impl::ShaderAsset>(std::move(shader)) };
}

Handle<Texture> AssetManager::LoadTexture(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create texture from invalid path: ", asset_path.string()
	);

	impl::Surface surface{ asset_path };

	auto texture =
		gl_.CreateTexture(surface.pixels.data(), GL_RGBA, GL_UNSIGNED_BYTE, surface.size, GL_RGBA);

	return Handle<Texture>{ std::make_shared<impl::TextureAsset>(std::move(texture)) };
}

Handle<Font> AssetManager::LoadFont(const path& asset_path, float pt_size) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create font from invalid path: ", asset_path.string()
	);

	auto ttf_font = TTF_OpenFont(asset_path.string().c_str(), pt_size);

	PTGN_ASSERT(ttf_font, SDL_GetError());

	std::unique_ptr<TTF_Font, impl::TTF_FontDeleter> font{ ttf_font, impl::TTF_FontDeleter{} };

	return Handle<Font>{ std::make_shared<impl::FontAsset>(std::move(font), pt_size) };
}

Handle<Audio> AssetManager::LoadAudio(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create audio from invalid path: ", asset_path.string()
	);

	PTGN_ASSERT(sdl_.mixer_, "Cannot load audio when SDL_mixer has not been created");

	auto mix_audio = MIX_LoadAudio(sdl_.mixer_, asset_path.string().c_str(), true);

	PTGN_ASSERT(mix_audio, SDL_GetError());

	std::unique_ptr<MIX_Audio, impl::MIX_AudioDeleter> music{ mix_audio, impl::MIX_AudioDeleter{} };

	return Handle<Audio>{ std::make_shared<impl::AudioAsset>(std::move(music)) };
}

Handle<Json> AssetManager::LoadJson(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create json from invalid path: ", asset_path.string()
	);

	auto json = ptgn::LoadJson(asset_path);

	return Handle<Json>{ std::make_shared<impl::JsonAsset>(std::move(json)) };
}

} // namespace ptgn