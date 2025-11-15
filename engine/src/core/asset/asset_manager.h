#pragma once

#include <cstdint>

#include "core/asset/asset_handle.h"
#include "core/util/file.h"

// TODO: Add something along the lines of:

// Handle<Sound> is a shared_ptr<Mix_Chunk>

// let sound_handle = asset_manager.Load<Sound>("path/to/sound");

// entity.Add<Sound>(sound_handle);

// entity.Get<Sound>().Play();

// In asset manager, map the hashed path to a weak ptr of the resource.

// When the handle gets destroyed (shared ptr custom deleter), free the mapped path.

// When using a duplicate path, get it from the mapped paths and turn the weak ptr into a shared
// ptr.

// Compile time:
// template <size_t N>
// void Load(const char (&filepath)[N]) {}

// Runtime:
// void Load(const path& filepath) {}

// Internally hash the filepath into std::size_t

namespace ptgn {

namespace impl {

namespace gl {

class GLContext;

} // namespace gl

} // namespace impl

class AssetManager {
public:
	AssetManager(impl::gl::GLContext& gl);
	~AssetManager() noexcept						 = default;
	AssetManager(const AssetManager&)				 = delete;
	AssetManager& operator=(const AssetManager&)	 = delete;
	AssetManager(AssetManager&&) noexcept			 = delete;
	AssetManager& operator=(AssetManager&&) noexcept = delete;

	Handle<Sound> LoadSound(const path& asset_path);
	Handle<Music> LoadMusic(const path& asset_path);
	Handle<Json> LoadJson(const path& asset_path);
	Handle<Shader> LoadShader(const path& asset_path);
	Handle<Texture> LoadTexture(const path& asset_path);
	Handle<Font> LoadFont(const path& asset_path, std::int32_t pt_size);

private:
	impl::gl::GLContext& gl_;
};

} // namespace ptgn