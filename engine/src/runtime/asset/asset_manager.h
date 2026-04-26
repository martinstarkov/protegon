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

#include "core/util/file.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/audio/audio.h"
#include "runtime/ecs/key_hash.h"
#include "runtime/graphics/text/font.h"
#include "serialization/json/fwd.h"
#include "serialization/serialize.h"

namespace ptgn {

class Application;
class RenderContext;
class AudioSystem;
class FontSystem;
class Text;
class DebugContext;

namespace impl {

class Renderer;

struct AssetKey : public KeyHash {
	using KeyHash::KeyHash;
};

struct AssetName {
	explicit AssetName(std::string_view name) : value{ name } {}

	std::string value;

	PTGN_SERIALIZE_VALUE(AssetName, value)
};

struct AssetPath {
	explicit AssetPath(const path& asset_path) : value{ asset_path } {}

	path value;

	PTGN_SERIALIZE_VALUE(AssetPath, value)
};

enum class AssetKind {
	Texture,
	Audio,
	Font,
	Json,
	Shader,
	Unknown
};

PTGN_SERIALIZE_ENUM(AssetKind);

template <typename>
struct AssetInfo;

template <>
struct AssetInfo<Texture> {
	static constexpr AssetKind kind = AssetKind::Texture;
	using Object					= impl::TextureObject;
	using Get						= Texture;
	using ConstGet					= Texture;

	static constexpr std::array extensions{
		std::string_view{ ".png" },
		std::string_view{ ".jpg" },
		std::string_view{ ".bmp" },
		std::string_view{ ".gif" },
	};
};

template <>
struct AssetInfo<Audio> {
	static constexpr AssetKind kind = AssetKind::Audio;
	using Object					= impl::AudioObject;
	using Get						= Audio;
	using ConstGet					= Audio;

	static constexpr std::array extensions{
		std::string_view{ ".ogg" },
		std::string_view{ ".mp3" },
		std::string_view{ ".wav" },
		std::string_view{ ".opus" },
	};
};

template <>
struct AssetInfo<Font> {
	static constexpr AssetKind kind = AssetKind::Font;
	using Object					= impl::FontObject;
	using Get						= Font;
	using ConstGet					= Font;

	static constexpr std::array extensions{
		std::string_view{ ".ttf" },
		std::string_view{ ".otf" },
	};
};

template <>
struct AssetInfo<Shader> {
	static constexpr AssetKind kind = AssetKind::Shader;
	using Object					= impl::ShaderObject;
	using Get						= Shader;
	using ConstGet					= Shader;

	static constexpr std::array extensions{
		std::string_view{ ".glsl" },
	};
};

template <>
struct AssetInfo<json> {
	static constexpr AssetKind kind = AssetKind::Json;
	using Object					= json;
	using Get						= std::reference_wrapper<json>;
	using ConstGet					= std::reference_wrapper<const json>;

	static constexpr std::array extensions{
		std::string_view{ ".json" },
	};
};

} // namespace impl

template <typename T>
concept AssetType = requires {
	typename impl::AssetInfo<T>::Object;
	typename impl::AssetInfo<T>::Get;
	typename impl::AssetInfo<T>::ConstGet;
	impl::AssetInfo<T>::kind;
	impl::AssetInfo<T>::extensions;
};

template <AssetType T>
using Asset = typename impl::AssetInfo<T>::Get;

template <AssetType T>
using ConstAsset = typename impl::AssetInfo<T>::ConstGet;

namespace impl {

template <AssetType T>
constexpr bool MatchesExtension(std::string_view extension) {
	for (auto candidate : AssetInfo<T>::extensions) {
		if (candidate == extension) {
			return true;
		}
	}
	return false;
}

AssetKind GetAssetKind(const path& path);

void AddAssetKey(ecs::Entity asset, std::string_view key, const std::optional<path>& path);

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

	/// @brief Note: Do not brace initialize JSON objects.
	template <AssetType T>
	std::optional<ConstAsset<T>> TryGet(std::string_view key) const;
	template <AssetType T>
	std::optional<Asset<T>> TryGet(std::string_view key);

	template <AssetType T>
	ConstAsset<T> Get(std::string_view key) const;

	template <AssetType T>
	Asset<T> Get(std::string_view key);

	template <AssetType T>
	[[nodiscard]] bool Has(std::string_view key) const;

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

	AssetManager() = delete;
	AssetManager(impl::Renderer& renderer, AudioSystem& audio, FontSystem& font);
	~AssetManager() noexcept						 = default;
	AssetManager(const AssetManager&)				 = delete;
	AssetManager& operator=(const AssetManager&)	 = delete;
	AssetManager(AssetManager&&) noexcept			 = delete;
	AssetManager& operator=(AssetManager&&) noexcept = delete;

	void Load(std::string_view key, const path& asset_path, impl::AssetKind kind);

	[[nodiscard]] Shader CreateShader(
		bool persistent, const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
		std::string_view shader_name
	);

	[[nodiscard]] Audio CreateAudio(bool persistent, const path& asset_path);
	[[nodiscard]] Texture CreateTexture(bool persistent, const path& asset_path);
	[[nodiscard]] Font CreateFont(bool persistent, const path& asset_path, float pt_size);

	[[nodiscard]] ecs::Entity CreateAsset();

	impl::Renderer& renderer_;
	AudioSystem& audio_;
	FontSystem& font_;

	ecs::Manager manager_;

	std::unordered_map<std::size_t, json> jsons_;
};

} // namespace ptgn