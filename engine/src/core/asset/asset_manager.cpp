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

template <>
Handle<Shader> AssetManager::Load<Shader>(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create shader from invalid path: ", asset_path.string()
	);
	// TODO: Implement.

	return Handle<Shader>{};
}

template <>
Handle<Texture> AssetManager::Load<Texture>(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create texture from invalid path: ", asset_path.string()
	);

	impl::Surface surface{ asset_path };

	auto texture =
		gl_.CreateTexture(surface.pixels.data(), GL_RGBA8, GL_UNSIGNED_BYTE, surface.size, GL_RGBA);

	return Handle<Texture>{ std::make_shared<impl::TextureAsset>(texture) };
}

template <>
Handle<Font> AssetManager::Load<Font>(const path& asset_path, std::int32_t pt_size) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create font from invalid path: ", asset_path.string()
	);

	auto ttf_font = TTF_OpenFont(asset_path.string().c_str(), pt_size);

	PTGN_ASSERT(ttf_font, TTF_GetError());

	auto font = std::make_unique<TTF_Font>(ttf_font, impl::TTF_FontDeleter{});

	return Handle<Font>{ std::make_shared<impl::FontAsset>(font, pt_size) };
}

template <>
Handle<Sound> AssetManager::Load<Sound>(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create sound from invalid path: ", asset_path.string()
	);
	auto mix_chunk = Mix_LoadWAV(asset_path.string().c_str());

	PTGN_ASSERT(mix_chunk, Mix_GetError());

	auto sound = std::make_unique<Mix_Chunk>(mix_chunk, impl::Mix_ChunkDeleter{});

	return Handle<Sound>{ std::make_shared<impl::SoundAsset>(sound) };
}

template <>
Handle<Music> AssetManager::Load<Music>(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create music from invalid path: ", asset_path.string()
	);
	auto mix_music = Mix_LoadMUS(asset_path.string().c_str());

	PTGN_ASSERT(mix_music, Mix_GetError());

	auto music = std::make_unique<Mix_Music>(mix_music, impl::Mix_MusicDeleter{});

	return Handle<Music>{ std::make_shared<impl::MusicAsset>(music) };
}

template <>
Handle<Json> AssetManager::Load<Json>(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create json from invalid path: ", asset_path.string()
	);

	auto json = LoadJson(asset_path);

	return Handle<Json>{ std::make_shared<impl::JsonAsset>(json) };
}

} // namespace ptgn