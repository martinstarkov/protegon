#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "core/util/file.h"
#include "renderer/resources/shader.h"
#include "runtime/asset/asset_handle.h"

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

struct SDLInstance;

} // namespace impl

class AssetManager {
public:
	AssetManager(impl::SDLInstance& sdl, impl::gl::GLContext& gl);
	~AssetManager() noexcept						 = default;
	AssetManager(const AssetManager&)				 = delete;
	AssetManager& operator=(const AssetManager&)	 = delete;
	AssetManager(AssetManager&&) noexcept			 = delete;
	AssetManager& operator=(AssetManager&&) noexcept = delete;

	Handle<Asset::Audio> LoadAudio(const path& audio_path);
	Handle<Asset::Json> LoadJson(const path& json_path);
	Handle<Asset::Shader> LoadShader(
		std::variant<ShaderCode, path> source, const std::string& shader_name
	);
	Handle<Asset::Shader> LoadShader(
		std::variant<ShaderCode, std::string> vertex,
		std::variant<ShaderCode, std::string> fragment, const std::string& shader_name
	);
	Handle<Asset::Texture> LoadTexture(const path& texture_path);
	Handle<Asset::Font> LoadFont(const path& font_path, float point_size);

private:
	impl::SDLInstance& sdl_;
	impl::gl::GLContext& gl_;
};

} // namespace ptgn