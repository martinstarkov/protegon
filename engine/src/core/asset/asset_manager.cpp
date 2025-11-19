#include "core/asset/asset_manager.h"

#include <cstdint>
#include <memory>

#include "SDL_mixer.h"
#include "SDL_ttf.h"
#include "core/assert.h"
#include "core/asset/asset.h"
#include "core/asset/asset_handle.h"
#include "core/util/file.h"
#include "renderer/gl/gl_context.h"
#include "renderer/image/surface.h"
#include "serialization/json/json.h"

// TODO: Add async asset loading.

namespace ptgn {

AssetManager::AssetManager(impl::gl::GLContext& gl) : gl_{ gl } {}

Handle<Shader> AssetManager::LoadShader(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create shader from invalid path: ", asset_path.string()
	);
	// TODO: Implement.

	return Handle<Shader>{};
}

Handle<Texture> AssetManager::LoadTexture(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create texture from invalid path: ", asset_path.string()
	);

	impl::Surface surface{ asset_path };

	auto texture =
		gl_.CreateTexture(surface.pixels.data(), GL_RGBA8, GL_UNSIGNED_BYTE, surface.size, GL_RGBA);

	return Handle<Texture>{ std::make_shared<impl::TextureAsset>(std::move(texture)) };
}

Handle<Font> AssetManager::LoadFont(const path& asset_path, std::int32_t pt_size) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create font from invalid path: ", asset_path.string()
	);

	auto ttf_font = TTF_OpenFont(asset_path.string().c_str(), pt_size);

	PTGN_ASSERT(ttf_font, TTF_GetError());

	std::unique_ptr<TTF_Font, impl::TTF_FontDeleter> font{ ttf_font, impl::TTF_FontDeleter{} };

	return Handle<Font>{ std::make_shared<impl::FontAsset>(std::move(font), pt_size) };
}

Handle<Sound> AssetManager::LoadSound(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create sound from invalid path: ", asset_path.string()
	);
	auto mix_chunk = Mix_LoadWAV(asset_path.string().c_str());

	PTGN_ASSERT(mix_chunk, Mix_GetError());

	std::unique_ptr<Mix_Chunk, impl::Mix_ChunkDeleter> sound{ mix_chunk, impl::Mix_ChunkDeleter{} };

	return Handle<Sound>{ std::make_shared<impl::SoundAsset>(std::move(sound)) };
}

Handle<Music> AssetManager::LoadMusic(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create music from invalid path: ", asset_path.string()
	);
	auto mix_music = Mix_LoadMUS(asset_path.string().c_str());

	PTGN_ASSERT(mix_music, Mix_GetError());

	std::unique_ptr<Mix_Music, impl::Mix_MusicDeleter> music{ mix_music, impl::Mix_MusicDeleter{} };

	return Handle<Music>{ std::make_shared<impl::MusicAsset>(std::move(music)) };
}

Handle<Json> AssetManager::LoadJson(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create json from invalid path: ", asset_path.string()
	);

	auto json = ptgn::LoadJson(asset_path);

	return Handle<Json>{ std::make_shared<impl::JsonAsset>(std::move(json)) };
}

} // namespace ptgn