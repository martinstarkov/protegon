#pragma once

#include <cstdint>

#include "core/asset/asset_handle.h"
#include "core/util/file.h"

// TODO: Add something along the lines of:

// Handle<Sound> is a shared_ptr<Mix_Chunk>

// let sound_handle = asset_manager.Load<Sound>("path/to/sound");

// entity.Add<Sound>(sound_handle);

// entity.Get<Sound>().Play();

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

	template <Asset T>
	Handle<T> Load(const path& asset_path);

	template <Asset T>
	Handle<T> Load(const path& asset_path, std::int32_t pt_size);

private:
	impl::gl::GLContext& gl_;
};

} // namespace ptgn