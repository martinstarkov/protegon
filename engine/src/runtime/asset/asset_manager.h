#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

#include "core/graphics/color.h"
#include "core/util/file.h"
#include "ecs/ecs.h"
#include "renderer/primitives/font.h"
#include "renderer/primitives/text.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/audio/audio.h"
#include "serialization/json/json.h"

#ifdef CreateFont
#undef CreateFont
#endif

struct SDL_IOStream;

namespace ptgn {

class Application;
class ApplicationContext;

namespace impl {

class TextDraw;
class FontSystem;

struct AssetName {
	explicit AssetName(std::string_view name) : name{ name } {}

	std::string name;
};

struct AssetKey {
	std::size_t hash{ 0 };
};

void AddAssetKey(ecs::Entity asset, std::string_view key, std::optional<path> path);

} // namespace impl

class AssetManager {
public:
	AssetManager()									 = default;
	~AssetManager() noexcept						 = default;
	AssetManager(const AssetManager&)				 = delete;
	AssetManager& operator=(const AssetManager&)	 = delete;
	AssetManager(AssetManager&&) noexcept			 = delete;
	AssetManager& operator=(AssetManager&&) noexcept = delete;

	Audio CreateAudio(const path& audio_path);
	Audio LoadAudio(std::string_view key, const path& audio_path);

	json CreateJson(const path& json_path);
	json& LoadJson(std::string_view key, const path& json_path);

	Shader CreateShader(
		const std::variant<ShaderCode, path>& source, const std::string& shader_name
	);
	Shader LoadShader(
		std::string_view key, const std::variant<ShaderCode, path>& source,
		const std::string& shader_name
	);
	Shader CreateShader(
		const std::variant<ShaderCode, std::string>& vertex,
		const std::variant<ShaderCode, std::string>& fragment, const std::string& shader_name
	);
	Shader LoadShader(
		std::string_view key, const std::variant<ShaderCode, std::string>& vertex,
		const std::variant<ShaderCode, std::string>& fragment, const std::string& shader_name
	);

	Texture CreateTexture(const path& texture_path);
	Texture LoadTexture(std::string_view key, const path& texture_path);

	Font CreateFont(const path& font_path, float point_size);
	Font LoadFont(std::string_view key, const path& font_path, float point_size);

	bool UnloadAudio(std::string_view key);
	bool UnloadJson(std::string_view key);
	bool UnloadShader(std::string_view key);
	bool UnloadTexture(std::string_view key);
	bool UnloadFont(std::string_view key);

	std::optional<std::reference_wrapper<json>> GetJson(std::string_view key);
	std::optional<std::reference_wrapper<const json>> GetJson(std::string_view key) const;
	std::optional<Audio> GetAudio(std::string_view key) const;
	std::optional<Shader> GetShader(std::string_view key) const;
	std::optional<Texture> GetTexture(std::string_view key) const;
	std::optional<Font> GetFont(std::string_view key) const;

	[[nodiscard]] bool HasJson(std::string_view key) const;
	[[nodiscard]] bool HasAudio(std::string_view key) const;
	[[nodiscard]] bool HasShader(std::string_view key) const;
	[[nodiscard]] bool HasTexture(std::string_view key) const;
	[[nodiscard]] bool HasFont(std::string_view key) const;

private:
	friend class Application;
	friend class Shader;
	friend class Texture;
	friend class FontSystem;
	friend class impl::TextDraw;

	std::optional<Font> GetFont(std::size_t key) const;

	Shader CreateShader(
		bool persistent, const std::variant<ShaderCode, path>& source,
		const std::string& shader_name
	);
	Shader CreateShader(
		bool persistent, const std::variant<ShaderCode, std::string>& vertex,
		const std::variant<ShaderCode, std::string>& fragment, const std::string& shader_name
	);
	Texture CreateTexture(bool persistent, const path& asset_path);
	Texture CreateTextTexture(
		std::string_view text_content, Color text_color, float font_size, Font font,
		const TextProperties& properties
	);
	Font CreateFont(bool persistent, const path& asset_path, float pt_size);
	Audio CreateAudio(bool persistent, const path& asset_path);

	ecs::Entity CreateAsset();

	ecs::Manager manager_;

	void SetContext(const std::shared_ptr<ApplicationContext>& ctx);

	std::shared_ptr<ApplicationContext> ctx_;

	std::unordered_map<std::size_t, json> jsons_;
};

} // namespace ptgn