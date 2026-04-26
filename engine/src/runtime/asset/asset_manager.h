#pragma once

#include <ecs/ecs.h>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/util/file.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset.h"
#include "runtime/audio/audio.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/font_system.h"
#include "runtime/graphics/text/text.h"
#include "serialization/serialize.h"

#ifdef CreateFont
#undef CreateFont
#endif

namespace ptgn {

class Application;
class RenderContext;
class AudioSystem;
class FontSystem;
class Text;
class DebugContext;

namespace impl {

class Renderer;

struct AssetName {
	explicit AssetName(std::string_view name) : name{ name } {}

	std::string name;

	PTGN_REFLECT_VALUE(AssetName, name)
};

struct AssetKey {
	std::size_t hash{ 0 };

	PTGN_REFLECT_VALUE(AssetKey, hash)
};

void AddAssetKey(ecs::Entity asset, std::size_t key_hash, const std::optional<path>& path);
void AddAssetKey(ecs::Entity asset, std::string_view key, const std::optional<path>& path);

enum class AssetType {
	Texture,
	Audio,
	Font,
	Json,
	Shader,
	Unknown
};
PTGN_REFLECT_ENUM(AssetType);

static const std::unordered_map<std::string, AssetType> kExtensionToType{
	{ ".png", AssetType::Texture }, { ".jpg", AssetType::Texture },
	{ ".bmp", AssetType::Texture }, { ".gif", AssetType::Texture },

	{ ".ogg", AssetType::Audio },	{ ".mp3", AssetType::Audio },
	{ ".wav", AssetType::Audio },	{ ".opus", AssetType::Audio },

	{ ".ttf", AssetType::Font },	{ ".otf", AssetType::Font },

	{ ".json", AssetType::Json },

	{ ".glsl", AssetType::Shader }
};

AssetType GetAssetType(const std::string& ext);

AssetType GetAssetType(const path& asset_path);

} // namespace impl

class AssetManager {
public:
	/// @brief Loads all supported asset files from a directory.
	/// @param directory The directory to scan.
	/// @param recursive If true, scans subdirectories recursively. If false only scans the provided
	/// directory.
	void LoadDirectory(const path& directory, bool recursive = true);

	/// @brief Load various different asset types from a json manifest file. Json format must be:
	///
	/// {
	///    "asset_key": "path/to/asset/file.extension",
	///    "shader_key1": "path/to/shader.glsl",
	///    "shader_key2": ["vertex_shader_name", "path/to/fragment_shader.glsl"],
	///    "shader_key3": ["path/to/vertex_shader.glsl", "fragment_shader_name"],
	///    ...
	/// }
	///
	/// Supported extensions:
	///
	/// Texture: .PNG, .JPG, .BMP, .GIF
	///
	/// Audio: .OGG (only one supported by Emscripten), MP3, WAV, OPUS
	///
	/// Font: .TTF, .OTF
	///
	/// JSON: .JSON
	///
	/// Shader: .GLSL or array of [vertex shader path or name, fragment shader path or name]. Name
	/// is used to reference an already loaded shader, while path is used to load a new shader.
	///
	/// @param asset_manifest_file The path to the asset json manifest file.
	void LoadMany(const path& asset_manifest_file);

	/// @brief Loads multiple assets from the specified file paths.
	/// @param asset_keys_and_paths A vector of key-path pairs where each pair contains an asset
	/// identifier string and its corresponding file path.
	void LoadMany(
		const std::vector<std::pair<std::string, std::variant<path, ShaderCode, ShaderPair>>>&
			asset_keys_and_paths
	);

	/// @brief Loads a supported asset type (based on extension) from the specified file path and
	/// associates it with a key.
	/// @param key The unique identifier used to reference the loaded asset.
	/// @param asset_path The file system path to the asset to be loaded.
	void Load(std::string_view key, const path& asset_path);
	void Load(std::string_view key, const ShaderCode& shader_code);
	void Load(std::string_view key, const ShaderPair& shader_pair);

	Audio CreateAudio(const path& audio_path);
	Audio LoadAudio(std::string_view key, const path& audio_path);

	/// @brief Note: Do not brace initialize JSON objects.
	/// See: https://json.nlohmann.me/home/faq/#brace-initialization-yields-arrays
	[[nodiscard]] static json CreateJson(const path& json_path);

	/// @brief Note: Do not brace initialize JSON objects.
	/// See: https://json.nlohmann.me/home/faq/#brace-initialization-yields-arrays
	json& LoadJson(std::string_view key, const path& json_path);

	Shader CreateShader(
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
	);

	Shader LoadShader(
		std::string_view key, const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
		std::optional<std::string_view> shader_name = std::nullopt
	);

	Texture CreateTexture(const path& texture_path);
	Texture LoadTexture(std::string_view key, const path& texture_path);

	Font CreateFont(const path& font_path, float font_size);
	Font LoadFont(std::string_view key, const path& font_path, float font_size = kDefaultFontSize);

	template <AssetType T>
	bool Unload(std::string_view key);

	bool UnloadAudio(std::string_view key);
	bool UnloadJson(std::string_view key);
	bool UnloadShader(std::string_view key);
	bool UnloadTexture(std::string_view key);
	bool UnloadFont(std::string_view key);

	template <AssetType T>
	std::optional<T> Get(std::string_view key) const;

	/// @brief Note: Do not brace initialize JSON objects.
	/// See: https://json.nlohmann.me/home/faq/#brace-initialization-yields-arrays
	std::optional<std::reference_wrapper<json>> GetJson(std::string_view key);
	std::optional<std::reference_wrapper<const json>> GetJson(std::string_view key) const;

	std::optional<Audio> GetAudio(std::string_view key) const;
	std::optional<Shader> GetShader(std::string_view key) const;
	std::optional<Texture> GetTexture(std::string_view key) const;
	std::optional<Font> GetFont(std::string_view key) const;

	template <AssetType T>
	[[nodiscard]] bool Has(std::string_view key) const;

	[[nodiscard]] bool HasJson(std::string_view key) const;
	[[nodiscard]] bool HasAudio(std::string_view key) const;
	[[nodiscard]] bool HasShader(std::string_view key) const;
	[[nodiscard]] bool HasTexture(std::string_view key) const;
	[[nodiscard]] bool HasFont(std::string_view key) const;

	/// @return The total number of assets currently loaded in the manager. Never below 1 (default
	/// font is always loaded).
	[[nodiscard]] std::size_t Size() const;

private:
	friend class Application;
	friend class Shader;
	friend class Texture;
	friend class RenderContext;
	friend class FontSystem;
	friend class Text;
	friend class DebugContext;
	template <AssetType T>
	friend class AssetOrKey;

	AssetManager() = delete;
	AssetManager(impl::Renderer& renderer, AudioSystem& audio, FontSystem& font);
	~AssetManager() noexcept						 = default;
	AssetManager(const AssetManager&)				 = delete;
	AssetManager& operator=(const AssetManager&)	 = delete;
	AssetManager(AssetManager&&) noexcept			 = delete;
	AssetManager& operator=(AssetManager&&) noexcept = delete;

	void Load(std::string_view key, const path& asset_path, impl::AssetType type);

	template <AssetType T>
	[[nodiscard]] bool Has(std::size_t key_hash) const;

	template <AssetType T>
	std::optional<T> Get(std::size_t key_hash) const;

	std::optional<std::reference_wrapper<json>> GetJson(std::size_t key_hash);
	std::optional<std::reference_wrapper<const json>> GetJson(std::size_t key_hash) const;
	std::optional<Font> GetFont(std::size_t key_hash) const;
	std::optional<Audio> GetAudio(std::size_t key_hash) const;
	std::optional<Texture> GetTexture(std::size_t key_hash) const;
	std::optional<Shader> GetShader(std::size_t key_hash) const;

	[[nodiscard]] Shader CreateShader(
		bool persistent, const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
		std::string_view shader_name
	);

	[[nodiscard]] Texture CreateTexture(bool persistent, const path& asset_path);
	[[nodiscard]] Font CreateFont(bool persistent, const path& asset_path, float pt_size);
	[[nodiscard]] Audio CreateAudio(bool persistent, const path& asset_path);

	[[nodiscard]] ecs::Entity CreateAsset();

	impl::Renderer& renderer_;
	AudioSystem& audio_;
	FontSystem& font_;

	ecs::Manager manager_;

	std::unordered_map<std::size_t, json> jsons_;
};

} // namespace ptgn