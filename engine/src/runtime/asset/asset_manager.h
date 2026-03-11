#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

#include "core/util/file.h"
#include "ecs/ecs.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/shader.h"
#include "renderer/primitives/texture.h"
#include "runtime/asset/font_system.h"
#include "runtime/audio/audio.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/text.h"
#include "serialization/json/json.h"

#ifdef CreateFont
#undef CreateFont
#endif
#include <utility>
#include <vector>

struct SDL_IOStream;

namespace ptgn {

class Application;
class ApplicationContext;
class RenderContext;
class Text;
class DebugContext;

namespace impl {

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

	// TODO: Add separate shader loading support to LoadMany (.VERT + .FRAG) or (existing_key +
	// .FRAG)

	/// @brief Loads all supported asset files from a directory.
	/// @param directory The directory to scan.
	/// @param recursive If true, scans subdirectories recursively. If false only scans the provided
	/// directory.
	void LoadDirectory(const path& directory, bool recursive = true);

	/// @brief Load various different asset types from a json manifest file. Json format must be:
	///
	/// {
	///    "asset_key": "path/to/asset/file.extension",
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
	/// @param asset_manifest_file The path to the asset json manifest file.
	void LoadMany(const path& asset_manifest_file);

	/// @brief Loads multiple assets from the specified file paths.
	/// @param asset_keys_and_paths A vector of key-path pairs where each pair contains an asset
	/// identifier string and its corresponding file path.
	void LoadMany(const std::vector<std::pair<std::string, path>>& asset_keys_and_paths);

	/// @brief Loads a supported asset type (based on extension) from the specified file path and
	/// associates it with a key.
	/// @param key The unique identifier used to reference the loaded asset.
	/// @param asset_path The file system path to the asset to be loaded.
	void Load(std::string_view key, const path& asset_path);

	Audio CreateAudio(const path& audio_path);
	Audio LoadAudio(std::string_view key, const path& audio_path);

	/// @brief Note: Do not brace initialize JSON objects.
	/// See: https://json.nlohmann.me/home/faq/#brace-initialization-yields-arrays
	json CreateJson(const path& json_path);

	/// @brief Note: Do not brace initialize JSON objects.
	/// See: https://json.nlohmann.me/home/faq/#brace-initialization-yields-arrays
	json& LoadJson(std::string_view key, const path& json_path);

	Shader CreateShader(const std::variant<ShaderCode, path>& source, std::string_view shader_name);
	Shader LoadShader(
		std::string_view key, const std::variant<ShaderCode, path>& source,
		std::string_view shader_name
	);
	Shader CreateShader(
		const std::variant<ShaderCode, std::string>& vertex,
		const std::variant<ShaderCode, std::string>& fragment, std::string_view shader_name
	);
	Shader LoadShader(
		std::string_view key, const std::variant<ShaderCode, std::string>& vertex,
		const std::variant<ShaderCode, std::string>& fragment, std::string_view shader_name
	);

	Texture CreateTexture(const path& texture_path);
	Texture LoadTexture(std::string_view key, const path& texture_path);

	Font CreateFont(const path& font_path, float font_size);
	Font LoadFont(std::string_view key, const path& font_path, float font_size = kDefaultFontSize);

	bool UnloadAudio(std::string_view key);
	bool UnloadJson(std::string_view key);
	bool UnloadShader(std::string_view key);
	bool UnloadTexture(std::string_view key);
	bool UnloadFont(std::string_view key);

	/// @brief Note: Do not brace initialize JSON objects.
	/// See: https://json.nlohmann.me/home/faq/#brace-initialization-yields-arrays
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

	[[nodiscard]] Texture ToTexture(std::variant<Texture, std::string_view> texture) const;
	[[nodiscard]] std::optional<Texture> ToTexture(
		std::variant<std::monostate, Texture, std::string_view> texture
	) const;

	[[nodiscard]] std::optional<Font> ToFont(
		std::variant<std::monostate, Font, std::string_view> font
	) const;

private:
	friend class Application;
	friend class Shader;
	friend class Texture;
	friend class RenderContext;
	friend class FontSystem;
	friend class Text;
	friend class DebugContext;

	void Init(const std::shared_ptr<ApplicationContext>& ctx);

	std::optional<Font> GetFont(std::size_t key) const;

	Shader CreateShader(
		bool persistent, const std::variant<ShaderCode, path>& source, std::string_view shader_name
	);
	Shader CreateShader(
		bool persistent, const std::variant<ShaderCode, std::string>& vertex,
		const std::variant<ShaderCode, std::string>& fragment, std::string_view shader_name
	);
	Texture CreateTexture(bool persistent, const path& asset_path);

	std::optional<impl::TextureObject> CreateTextTextureObject(
		std::string_view text_content, Color color, float font_size, Font font_asset,
		const TextProperties& properties, float hd_scale, bool hd
	);

	Texture CreateTextTexture(
		std::string_view text_content, Color text_color, float font_size, Font font,
		const TextProperties& properties, float hd_scale, bool hd
	);
	Font CreateFont(bool persistent, const path& asset_path, float pt_size);
	Audio CreateAudio(bool persistent, const path& asset_path);

	ecs::Entity CreateAsset();

	ecs::Manager manager_;

	std::shared_ptr<ApplicationContext> ctx_;

	std::unordered_map<std::size_t, json> jsons_;
};

} // namespace ptgn