#pragma once

#include <string>
#include <variant>

#include "core/util/file.h"
#include "renderer/primitives/font.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/audio/audio.h"
#include "serialization/json/json.h"

namespace ptgn {

class Renderer;

namespace impl {

struct SDLInstance;

struct AssetName {
	std::string name;
};

struct AssetKey {
	std::size_t hash{ 0 };
};

} // namespace impl

class AssetManager {
public:
	AssetManager(impl::SDLInstance& sdl, Renderer& renderer);
	~AssetManager() noexcept						 = default;
	AssetManager(const AssetManager&)				 = delete;
	AssetManager& operator=(const AssetManager&)	 = delete;
	AssetManager(AssetManager&&) noexcept			 = delete;
	AssetManager& operator=(AssetManager&&) noexcept = delete;

	Audio LoadAudio(const path& audio_path);
	json LoadJson(const path& json_path);
	impl::Shader LoadShader(
		const std::variant<ShaderCode, path>& source, const std::string& shader_name
	);
	impl::Shader LoadShader(
		const std::variant<ShaderCode, std::string>& vertex,
		const std::variant<ShaderCode, std::string>& fragment, const std::string& shader_name
	);
	impl::Texture LoadTexture(const path& texture_path);
	Font LoadFont(const path& font_path, float point_size);

private:
	friend class Shader;
	friend class Texture;

	impl::SDLInstance& sdl_;
	Renderer& renderer_;
};

} // namespace ptgn