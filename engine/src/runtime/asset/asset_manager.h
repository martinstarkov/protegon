#pragma once

#include <ecs/ecs.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "renderer/text/font_atlas.h"
#include "runtime/asset/asset_key.h"
#include "runtime/asset/asset_serialization.h"
#include "runtime/audio/audio.h"
#include "runtime/ecs/key_hash.h"
#include "runtime/graphics/text/font.h"
#include "runtime/asset/prefab.h"
#include "serialization/json/json.h"
#include "serialization/serialize.h"

namespace ptgn {

class AudioSystem;
class FontSystem;
class Text;
class Renderer;
class AssetManager;

namespace impl {

class Surface;
class ApplicationContext;

struct AssetPath {
	explicit AssetPath(const path& asset_path) : value{ asset_path } {}

	path value;

	PTGN_REFLECT_VALUE(AssetPath, value)
};

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
	using Object					= impl::FontAtlas;
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

template <>
struct AssetInfo<Prefab> {
	static constexpr AssetKind kind = AssetKind::Prefab;
	using Object					= Prefab;
	using Get						= std::reference_wrapper<Prefab>;
	using ConstGet					= std::reference_wrapper<const Prefab>;

	static constexpr std::array extensions{
		kPrefabExtension,
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
	return std::ranges::contains(AssetInfo<T>::extensions, extension);
}

AssetKind GetAssetKind(const path& path);

void AddAssetKey(ecs::Entity asset, AssetKey key, const std::optional<path>& path);

struct AssetPreview {
	TextureId texture;
	V2_int size;
};

struct AssetRecord {
	AssetKey key;
	path source_path;
	AssetKind kind{ AssetKind::Unknown };
	std::optional<AssetPreview> preview;
};

/// @brief Temporarily records path-backed asset loads as dependencies of one scene.
class AssetCaptureScope {
public:
	AssetCaptureScope(AssetManager& assets, std::vector<AssetKey>& dependencies);
	~AssetCaptureScope() noexcept;

	AssetCaptureScope(const AssetCaptureScope&) = delete;
	AssetCaptureScope& operator=(const AssetCaptureScope&) = delete;
	AssetCaptureScope(AssetCaptureScope&&) noexcept = delete;
	AssetCaptureScope& operator=(AssetCaptureScope&&) noexcept = delete;

private:
	AssetManager& assets_;
	std::vector<AssetKey>& dependencies_;
};

struct JsonAssetData {
	AssetKey key;
	path source_path;
	json value;
};

struct PrefabAssetData {
	PrefabKey key;
	path file_path;
	path source_path;
	Prefab value;
};

class AssetAccessor {
public:
	explicit AssetAccessor(AssetManager& assets);
	~AssetAccessor() noexcept						   = default;
	AssetAccessor(const AssetAccessor&)				   = delete;
	AssetAccessor& operator=(const AssetAccessor&)	   = delete;
	AssetAccessor(AssetAccessor&&) noexcept			   = delete;
	AssetAccessor& operator=(AssetAccessor&&) noexcept = delete;

	template <AssetType T>
	ConstAsset<T> Get(const AssetKey& key) const;

	template <AssetType T>
	Asset<T> Get(const AssetKey& key);

	template <AssetType T>
	[[nodiscard]] bool Has(const AssetKey& key) const;

	[[nodiscard]] std::vector<impl::AssetRecord> GetAssets() const;
	bool Unload(const AssetKey& key, AssetKind kind);

private:
	AssetManager& assets;
};

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
	/// Font: .TTF, .OTF, font atlas .PNG with embedded font data.
	///
	/// JSON: .JSON
	///
	/// Prefab: .PTGNPREFAB
	///
	/// Shader: .GLSL or array of [vertex shader path or name, fragment shader path or name]. Name
	/// is used to reference an already loaded shader, while path is used to load a new shader.
	///
	/// @param asset_manifest_file The path to the asset json manifest file.
	void LoadManifest(const path& asset_manifest_file);

	/// @brief Loads multiple assets from the specified file paths.
	/// @param asset_keys_and_paths A vector of key-path pairs where each pair contains an asset
	/// identifier string and its corresponding file path.
	void Load(
		const std::vector<std::pair<AssetKey, std::variant<path, ShaderCode, ShaderPair>>>&
			asset_keys_and_paths
	);

	/// @brief Loads a supported asset type (based on extension) from the specified file path and
	/// associates it with a key.
	/// @param key The unique identifier used to reference the loaded asset.
	/// @param asset_path The file system path to the asset to be loaded.
	void Load(AssetKey key, const path& asset_path);
	void Load(ShaderKey key, const ShaderCode& shader_code);
	void Load(ShaderKey key, const ShaderPair& shader_pair);

	/// @brief Loads one persistent path-backed asset descriptor.
	void Load(const SerializedAsset& asset);

	/// @brief Loads catalog assets referenced by the provided keys.
	void LoadDependencies(std::span<const AssetKey> dependencies);

	/// @brief Loads an asset and pins it as a project-wide startup dependency.
	void LoadProjectAsset(AssetKey key, const path& asset_path);

	/// @brief Merges persistent descriptors into the known project asset catalog.
	void RegisterCatalog(std::span<const SerializedAsset> assets);

	/// @return The complete known path-backed project asset catalog.
	[[nodiscard]] std::vector<SerializedAsset> GetCatalog() const;

	void AddProjectAssetDependency(AssetKey key);
	void AddProjectAssetDependencies(std::span<const AssetKey> dependencies);

	/// @return Asset keys that should be loaded globally whenever the project starts.
	[[nodiscard]] const std::vector<AssetKey>& GetProjectAssetDependencies() const;

	[[nodiscard]] bool HasCatalogAsset(const AssetKey& key) const;

	Audio LoadAudio(AudioKey key, const path& audio_path);

	/// @brief Note: Do not brace initialize JSON objects.
	/// See: https://json.nlohmann.me/home/faq/#brace-initialization-yields-arrays
	json& LoadJson(const JsonKey& key, const path& json_path);

	Prefab& LoadPrefab(PrefabKey key, const path& prefab_path);
	Prefab& SavePrefab(Prefab prefab, const path& prefab_path);
	Prefab& SavePrefab(Prefab prefab, const path& file_path, const path& source_path);
	bool SavePrefab(const PrefabKey& key);
	bool RemovePrefab(const PrefabKey& key, bool remove_file = true);
	[[nodiscard]] std::vector<PrefabKey> GetPrefabKeys() const;
	[[nodiscard]] path GetPrefabPath(const PrefabKey& key) const;

	Shader LoadShader(
		ShaderKey key, const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
		std::optional<std::string_view> shader_name = std::nullopt
	);

	Texture LoadTexture(
		TextureKey key, const path& texture_path,
		TextureFormat storage_format = kDefaultTextureStorageFormat, TextureParams params = {}
	);

	Font LoadFont(FontKey key, const path& font_path);

	template <AssetType T>
	bool Unload(const AssetKey& key);

	/// @return The total number of assets currently loaded in the manager. Never below 1 (default
	/// font is always loaded).
	[[nodiscard]] std::size_t Size() const;

	V2_int GetTextureSize(const TextureKey& key) const;
	V2_int GetFontAtlasSize(const FontKey& key) const;
	impl::TextureId GetFontAtlasTexture(const FontKey& key) const;

	/// @brief Note: Do not brace initialize JSON objects.
	/// See: https://json.nlohmann.me/home/faq/#brace-initialization-yields-arrays
	[[nodiscard]] json CreateJson(const path& json_path) const;

	[[nodiscard]] bool Has(const AssetKey& key) const;

	template <typename T>
	requires std::derived_from<std::remove_cvref_t<T>, AssetKey> &&
			 requires { std::remove_cvref_t<T>::kind; }
	bool Has(const T& key) const {
		using Value = std::remove_cvref_t<T>;

		return Has(static_cast<const AssetKey&>(key), Value::kind);
	}
private:
	friend class impl::AssetAccessor;
	friend class impl::AssetCaptureScope;
	friend class impl::ApplicationContext;
	friend class Shader;
	friend class Texture;
	friend class FontSystem;
	friend class Text;

	AssetManager() = delete;
	AssetManager(Renderer& renderer, AudioSystem& audio, FontSystem& font);
	~AssetManager() noexcept						 = default;
	AssetManager(const AssetManager&)				 = delete;
	AssetManager& operator=(const AssetManager&)	 = delete;
	AssetManager(AssetManager&&) noexcept			 = delete;
	AssetManager& operator=(AssetManager&&) noexcept = delete;

	/// @brief Note: Do not brace initialize JSON objects.
	template <AssetType T>
	std::optional<ConstAsset<T>> TryGet(const AssetKey& key) const;
	template <AssetType T>
	std::optional<Asset<T>> TryGet(const AssetKey& key);

	template <AssetType T>
	ConstAsset<T> Get(const AssetKey& key) const;

	template <AssetType T>
	Asset<T> Get(const AssetKey& key);

	template <AssetType T>
	[[nodiscard]] bool Has(const AssetKey& key) const;

	bool Has(const AssetKey& key, AssetKind kind) const;

	Audio CreateAudio(const path& audio_path);

	Shader CreateShader(
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
	);

	Texture CreateTexture(
		const path& texture_path, TextureFormat storage_format = kDefaultTextureStorageFormat,
		TextureParams params = {}
	);

	Font CreateFont(const path& font_path);

	[[nodiscard]] std::vector<impl::AssetRecord> GetAssets() const;
	bool Unload(const AssetKey& key, AssetKind kind);

	void Load(AssetKey key, const path& asset_path, AssetKind kind);

	void BeginAssetCapture(std::vector<AssetKey>& dependencies);
	void EndAssetCapture(std::vector<AssetKey>& dependencies);
	void TrackAssetLoad(const AssetKey& key, AssetKind kind, const path& source_path);

	[[nodiscard]] Shader CreateShader(
		bool persistent, const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
		std::string_view shader_name
	);

	[[nodiscard]] impl::TextureObject CreateTexture(
		const impl::Surface& surface, TextureFormat storage_format, TextureParams params = {}
	) const;

	[[nodiscard]] impl::TextureObject CreateTexture(
		const std::uint8_t* pixel_data, TextureDesc desc
	) const;

	[[nodiscard]] Audio CreateAudio(bool persistent, const path& asset_path);
	[[nodiscard]] Texture CreateTexture(
		bool persistent, const path& asset_path, TextureFormat storage_format, TextureParams params
	);
	[[nodiscard]] Font CreateFont(bool persistent, const path& asset_path);

	[[nodiscard]] ecs::Entity CreateAsset();

	Renderer& renderer_;
	AudioSystem& audio_;
	FontSystem& font_;

	ecs::Manager manager_;

	std::unordered_map<std::size_t, impl::JsonAssetData> jsons_;
	std::unordered_map<std::size_t, impl::PrefabAssetData> prefabs_;
	std::unordered_map<std::size_t, SerializedAsset> catalog_;
	std::vector<AssetKey> project_asset_dependencies_;
	std::vector<AssetKey>* captured_asset_dependencies_{ nullptr };
};

namespace impl {

template <AssetType T>
ConstAsset<T> AssetAccessor::Get(const AssetKey& key) const {
	return assets.Get<T>(key);
}

template <AssetType T>
Asset<T> AssetAccessor::Get(const AssetKey& key) {
	return assets.Get<T>(key);
}

template <AssetType T>
bool AssetAccessor::Has(const AssetKey& key) const {
	return assets.Has<T>(key);
}

} // namespace impl

} // namespace ptgn