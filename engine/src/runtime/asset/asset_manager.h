#pragma once

#include <string>
#include <string_view>
#include <variant>

#include "core/util/file.h"
#include "ecs/ecs.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/audio_asset.h"
#include "runtime/asset/font_asset.h"
#include "runtime/asset/shader_asset.h"
#include "runtime/asset/texture_asset.h"
#include "runtime/audio/audio.h"
#include "serialization/json/json.h"

namespace ptgn {

class Renderer;

namespace impl {

struct SDLInstance;

struct AssetName {
	AssetName(std::string_view name) : name{ name } {}

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

	Audio LoadAudio(std::string_view key, const path& audio_path);
	json LoadJson(std::string_view key, const path& json_path);
	Shader LoadShader(
		std::string_view key, const std::variant<ShaderCode, path>& source,
		const std::string& shader_name
	);
	Shader LoadShader(
		std::string_view key, const std::variant<ShaderCode, std::string>& vertex,
		const std::variant<ShaderCode, std::string>& fragment, const std::string& shader_name
	);
	Texture LoadTexture(std::string_view key, const path& texture_path);
	Font LoadFont(std::string_view key, const path& font_path, float point_size);

	void UnloadAudio(std::string_view key);
	void UnloadJson(std::string_view key);
	void UnloadShader(std::string_view key);
	void UnloadTexture(std::string_view key);
	void UnloadFont(std::string_view key);

private:
	friend class Shader;
	friend class Texture;

	ecs::Manager manager_;
	impl::SDLInstance& sdl_;
	Renderer& renderer_;
};

} // namespace ptgn