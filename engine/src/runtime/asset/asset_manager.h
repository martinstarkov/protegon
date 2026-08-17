#pragma once

#include <ecs/ecs.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
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
#include "renderer/shader_compiler.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "renderer/text/font_atlas.h"
#include "runtime/asset/asset_key.h"
#include "runtime/asset/asset_serialization.h"
#include "runtime/asset/prefab.h"
#include "runtime/audio/audio.h"
#include "runtime/ecs/key_hash.h"
#include "runtime/graphics/text/font.h"
#include "serialization/json/json.h"
#include "serialization/serialize.h"

namespace ptgn {

class AudioSystem;
class FontSystem;
class Text;
class Renderer;
class Scene;
class AssetManager;
struct Project;

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
	using Object = impl::TextureObject;
	using Get = Texture;
	using ConstGet = Texture;

	static constexpr std::array extensions{
		std::string_view{ ".png" },
		std::string_view{ ".jpg" },
		std::string_view{ ".jpeg" },
		std::string_view{ ".bmp" },
		std::string_view{ ".gif" },
	};
};

template <>
struct AssetInfo<Audio> {
	static constexpr AssetKind kind = AssetKind::Audio;
	using Object = impl::AudioObject;
	using Get = Audio;
	using ConstGet = Audio;

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
	using Object = impl::FontAtlas;
	using Get = Font;
	using ConstGet = Font;

	static constexpr std::array extensions{
		std::string_view{ ".ttf" },
		std::string_view{ ".otf" },
	};
};

template <>
struct AssetInfo<Shader> {
	static constexpr AssetKind kind = AssetKind::Shader;
	using Object = impl::ShaderObject;
	using Get = Shader;
	using ConstGet = Shader;

	static constexpr std::array extensions{
		std::string_view{ ".glsl" },
	};
};

template <>
struct AssetInfo<json> {
	static constexpr AssetKind kind = AssetKind::Json;
	using Object = json;
	using Get = std::reference_wrapper<json>;
	using ConstGet = std::reference_wrapper<const json>;

	static constexpr std::array extensions{
		std::string_view{ ".json" },
	};
};

template <>
struct AssetInfo<Prefab> {
	static constexpr AssetKind kind = AssetKind::Prefab;
	using Object = Prefab;
	using Get = std::reference_wrapper<Prefab>;
	using ConstGet = std::reference_wrapper<const Prefab>;

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

struct AssetMetadata {
	std::uintmax_t file_size{ 0 };
	std::optional<V2_int> dimensions;
	std::optional<double> duration_seconds;
	ShaderStageMask shader_stages{ ShaderStageMask::None };
};

struct AssetStorageKey {
	std::size_t key_hash{ 0 };
	AssetKind kind{ AssetKind::Unknown };

	bool operator==(const AssetStorageKey&) const = default;
};

struct AssetStorageKeyHash {
	[[nodiscard]] std::size_t operator()(const AssetStorageKey& value) const noexcept {
		std::size_t result{ value.key_hash };
		const std::size_t kind{ static_cast<std::size_t>(value.kind) };
		result ^= kind + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
		return result;
	}
};

struct EngineShaderSource {
	AssetKey key;
	std::string name;
	path virtual_path;
	ShaderStageMask stages{ ShaderStageMask::None };
	std::string source;
};

struct AssetRecord {
	AssetKey key;
	path source_path;
	AssetKind kind{ AssetKind::Unknown };
	AssetLoadState load_state{ AssetLoadState::Unloaded };
	std::size_t reference_count{ 0 };
	bool globally_pinned{ false };
	bool manually_pinned{ false };
	bool cataloged{ false };
	bool engine_asset{ false };
	bool read_only{ false };
	bool compile_error{ false };
	std::string load_error;
	std::string compile_log;
	AssetMetadata metadata;
	std::optional<AssetPreview> preview;
};

struct AssetLoadProgress {
	std::size_t total_assets{ 0 };
	std::size_t completed_assets{ 0 };
	std::size_t failed_assets{ 0 };
	std::uintmax_t total_bytes{ 0 };
	std::uintmax_t completed_bytes{ 0 };
	std::string active_asset;

	[[nodiscard]] bool IsComplete() const {
		return completed_assets >= total_assets;
	}

	[[nodiscard]] float Fraction() const {
		if (total_bytes > 0) {
			return static_cast<float>(completed_bytes) /
				   static_cast<float>(total_bytes);
		}
		if (total_assets == 0) {
			return 1.0f;
		}
		return static_cast<float>(completed_assets) /
			   static_cast<float>(total_assets);
	}
};

struct AssetLoadBatchState;

/// @brief Move-only ownership of a set of loaded asset references.
/// Destroying the ticket releases the references and permits automatic unloading.
class AssetLoadTicket {
public:
	AssetLoadTicket() = default;
	~AssetLoadTicket() noexcept;

	AssetLoadTicket(const AssetLoadTicket&) = delete;
	AssetLoadTicket& operator=(const AssetLoadTicket&) = delete;

	AssetLoadTicket(AssetLoadTicket&& other) noexcept;
	AssetLoadTicket& operator=(AssetLoadTicket&& other) noexcept;

	[[nodiscard]] explicit operator bool() const;
	[[nodiscard]] bool IsComplete() const;
	[[nodiscard]] AssetLoadProgress GetProgress() const;
	[[nodiscard]] const std::vector<AssetKey>& GetDependencies() const;

	/// @brief Transfers dependency-release responsibility to a Scene.
	[[nodiscard]] std::vector<AssetKey> ReleaseOwnership();

private:
	friend class ::ptgn::AssetManager;

	AssetLoadTicket(
		AssetManager& assets,
		std::shared_ptr<AssetLoadBatchState> state,
		std::vector<AssetKey> dependencies
	);

	void Reset() noexcept;

	AssetManager* assets_{ nullptr };
	std::shared_ptr<AssetLoadBatchState> state_;
	std::vector<AssetKey> dependencies_;
	bool owns_references_{ false };
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
	~AssetAccessor() noexcept = default;
	AssetAccessor(const AssetAccessor&) = delete;
	AssetAccessor& operator=(const AssetAccessor&) = delete;
	AssetAccessor(AssetAccessor&&) noexcept = delete;
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
	/// @brief Advances asynchronous asset preparation and finalizes GPU/main-thread resources.
	/// Call once per application frame.
	void Update();

	/// @brief Loads all supported asset files from a directory immediately.
	void LoadDirectory(const path& directory, bool recursive = true);

	void LoadManifest(const path& asset_manifest_file);

	void Load(
		const std::vector<std::pair<AssetKey, std::variant<path, ShaderCode, ShaderPair>>>&
			asset_keys_and_paths
	);

	/// @brief Registers a path-backed asset without loading it. This is useful for asynchronous
	/// loading when the asset is not already present in a project catalog.
	[[nodiscard]] bool RegisterAsset(AssetKey key, const path& asset_path);

	/// @brief Loads and catalogs a path-backed asset. Relative paths may be project-relative or
	/// relative to the runtime asset root/current working directory.
	void Load(AssetKey key, const path& asset_path);
	void Load(ShaderKey key, const ShaderCode& shader_code);
	void Load(ShaderKey key, const ShaderPair& shader_pair);
	void Load(const SerializedAsset& asset);

	/// @brief Synchronously loads catalog assets. Prefer AcquireDependenciesAsync for gameplay.
	void LoadDependencies(std::span<const AssetKey> dependencies);

	/// @brief Starts a non-blocking manual residency load. Unload clears this residency pin.
	void LoadAssetAsync(const AssetKey& key);
	void LoadAssetAsync(const AssetKey& key, AssetKind kind);

	/// @brief Starts non-blocking loads and retains the assets until the returned ticket is moved
	/// into a Scene or destroyed.
	[[nodiscard]] impl::AssetLoadTicket AcquireDependenciesAsync(
		std::span<const AssetKey> dependencies
	);

	/// @return Aggregate progress for all currently active asynchronous load batches.
	[[nodiscard]] impl::AssetLoadProgress GetActiveLoadProgress() const;
	[[nodiscard]] bool IsLoading() const;

	/// @brief Loads an asset and pins it as a project-wide startup dependency.
	void LoadProjectAsset(AssetKey key, const path& asset_path);

	/// @brief Replaces the known project catalog without loading every entry.
	/// @return True when normalization/discovery changed the serialized catalog.
	bool RegisterCatalog(std::span<const SerializedAsset> assets, const Project& project);

	/// @brief Adds supported files found under the project Assets directory to the catalog.
	void RefreshCatalogFromDisk();

	/// @return The complete known path-backed project asset catalog.
	[[nodiscard]] std::vector<SerializedAsset> GetCatalog() const;
	[[nodiscard]] std::optional<SerializedAsset> GetCatalogAsset(const AssetKey& key) const;
	[[nodiscard]] std::optional<SerializedAsset> GetCatalogAsset(
		const AssetKey& key,
		AssetKind kind
	) const;

	void AddProjectAssetDependency(AssetKey key);
	/// @brief Pins and starts a non-blocking load for one project-wide dependency.
	void PreloadProjectAsset(AssetKey key);
	void RemoveProjectAssetDependency(const AssetKey& key);
	void AddProjectAssetDependencies(std::span<const AssetKey> dependencies);
	void SetProjectAssetDependencies(std::span<const AssetKey> dependencies);

	[[nodiscard]] const std::vector<AssetKey>& GetProjectAssetDependencies() const;
	[[nodiscard]] bool HasCatalogAsset(const AssetKey& key) const;
	[[nodiscard]] bool HasCatalogAsset(const AssetKey& key, AssetKind kind) const;

	/// @brief Copies a foreign file into the project folder owned by its detected asset kind.
	[[nodiscard]] std::optional<AssetKey> ImportAsset(const path& source_file);

	/// @brief Imports into destination_directory only when it belongs to the detected asset kind.
	/// Otherwise the asset is imported into that kind's base directory.
	[[nodiscard]] std::optional<AssetKey> ImportAsset(
		const path& source_file,
		const path& destination_directory
	);

	/// @brief Moves an asset into a base or user subdirectory belonging to the same asset kind.
	/// destination_directory is relative to Assets. The asset key is unchanged.
	bool MoveAsset(const AssetKey& key, const path& destination_directory = {});
	bool MoveAsset(
		const AssetKey& key,
		AssetKind kind,
		const path& destination_directory = {}
	);

	/// @brief Renames a user-created asset directory and preserves catalog keys/paths. Both paths are
	/// relative to Assets and must remain inside the same protected type directory.
	bool MoveAssetDirectory(const path& source_directory, const path& destination_directory);

	/// @brief Renames an unloaded, unreferenced catalog key without moving its source file.
	bool RenameAssetKey(const AssetKey& key, AssetKey new_key);
	bool RenameAssetKey(const AssetKey& key, AssetKind kind, AssetKey new_key);

	/// @brief Restores an exact catalog entry after its source file has been restored.
	bool RestoreCatalogAsset(const SerializedAsset& asset);

	/// @brief Removes a catalog entry and optionally deletes its project file.
	bool DeleteAsset(const AssetKey& key, bool delete_file = true);
	bool DeleteAsset(const AssetKey& key, AssetKind kind, bool delete_file = true);

	/// @brief Sets the vertex/fragment source descriptors for a shader program.
	/// $source selects this shader file, $builtin:<name> selects an embedded engine stage, and a
	/// project-relative path selects another GLSL file. A missing stage is retained as unconfigured;
	/// passing nullopt for both clears the explicit program configuration.
	bool ConfigureShaderProgram(
		const ShaderKey& key,
		std::optional<std::string> vertex_source,
		std::optional<std::string> fragment_source
	);

	[[nodiscard]] std::vector<impl::AssetRecord> GetEngineShaderAssets() const;
	[[nodiscard]] std::optional<std::string> GetEngineShaderSource(const AssetKey& key) const;
	[[nodiscard]] std::span<const std::string> GetEngineVertexShaderNames() const;
	[[nodiscard]] std::span<const std::string> GetEngineFragmentShaderNames() const;
	[[nodiscard]] std::optional<SerializedShaderProgram> SuggestShaderProgram(
		std::string_view source
	) const;
	[[nodiscard]] ShaderCompileResult ValidateShaderSource(
		const ShaderKey& key,
		std::string_view source
	) const;
	[[nodiscard]] ShaderCompileResult ValidateShaderSource(
		const ShaderKey& key,
		std::string_view source,
		const SerializedShaderProgram& program
	) const;
	[[nodiscard]] bool SaveShaderSource(
		const ShaderKey& key,
		std::string_view source,
		const ShaderCompileResult& validation
	);
	[[nodiscard]] ShaderCompileResult RecompileShaderSource(
		const ShaderKey& key,
		std::string_view source
	);
	[[nodiscard]] ShaderCompileResult RecompileShaderSource(
		const ShaderKey& key,
		std::string_view source,
		const SerializedShaderProgram& program
	);
	[[nodiscard]] std::optional<std::string> GetShaderSource(const ShaderKey& key) const;
	[[nodiscard]] std::optional<std::string> ResolveShaderStageSource(
		std::string_view owner_source,
		std::string_view reference,
		ShaderStageMask stage
	) const;

	[[nodiscard]] std::optional<path> GetProjectRoot() const;
	[[nodiscard]] std::optional<path> GetAssetDirectory() const;

	/// @brief Finds every catalog key appearing as a JSON string, including transitive prefab refs.
	[[nodiscard]] std::vector<AssetKey> DiscoverDependencies(
		const json& value,
		std::span<const AssetKey> manual_dependencies = {}
	) const;

	Audio LoadAudio(AudioKey key, const path& audio_path);

	json& LoadJson(const JsonKey& key, const path& json_path);

	Prefab& LoadPrefab(PrefabKey key, const path& file_path, const path& source_path);
	Prefab& SavePrefab(Prefab prefab, const path& prefab_path);
	Prefab& SavePrefab(Prefab prefab, const path& file_path, const path& source_path);
	bool SavePrefab(const PrefabKey& key);
	bool RemovePrefab(const PrefabKey& key, bool remove_file = true);
	[[nodiscard]] std::vector<PrefabKey> GetPrefabKeys() const;
	[[nodiscard]] path GetPrefabPath(const PrefabKey& key) const;

	Shader LoadShader(
		ShaderKey key,
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
		std::optional<std::string_view> shader_name = std::nullopt
	);

	Texture LoadTexture(
		TextureKey key,
		const path& texture_path,
		TextureFormat storage_format = kDefaultTextureStorageFormat,
		TextureParams params = {}
	);

	Font LoadFont(FontKey key, const path& font_path);

	template <AssetType T>
	bool Unload(const AssetKey& key);

	[[nodiscard]] std::size_t Size() const;

	V2_int GetTextureSize(const TextureKey& key) const;
	V2_int GetFontAtlasSize(const FontKey& key) const;
	impl::TextureId GetFontAtlasTexture(const FontKey& key) const;

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
	friend class impl::AssetLoadTicket;
	friend class impl::ApplicationContext;
	friend class Scene;
	friend class Shader;
	friend class Texture;
	friend class FontSystem;
	friend class Text;

	struct RuntimeAssetState {
		AssetLoadState load_state{ AssetLoadState::Unloaded };
		std::size_t reference_count{ 0 };
		bool globally_pinned{ false };
		bool manually_pinned{ false };
		std::string error;
		bool compile_error{ false };
		std::string compile_log;
		impl::AssetMetadata metadata;
		std::vector<std::weak_ptr<impl::AssetLoadBatchState>> waiters;
	};

	class AsyncLoader;

	AssetManager() = delete;
	explicit AssetManager(Renderer& renderer);
	~AssetManager() noexcept;
	AssetManager(const AssetManager&) = delete;
	AssetManager& operator=(const AssetManager&) = delete;
	AssetManager(AssetManager&&) noexcept = delete;
	AssetManager& operator=(AssetManager&&) noexcept = delete;

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
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
		std::string_view shader_name
	);

	Texture CreateTexture(
		const path& texture_path,
		TextureFormat storage_format = kDefaultTextureStorageFormat,
		TextureParams params = {}
	);

	Font CreateFont(const path& font_path);

	[[nodiscard]] std::vector<impl::AssetRecord> GetAssets() const;
	bool Unload(const AssetKey& key, AssetKind kind);
	bool ForceUnload(const AssetKey& key, AssetKind kind);

	void Load(AssetKey key, const path& asset_path, AssetKind kind);
	void QueueAssetLoad(
		const SerializedAsset& asset,
		const std::shared_ptr<impl::AssetLoadBatchState>& batch
	);
	void CompleteAssetLoad(
		impl::AssetStorageKey storage_key,
		bool success,
		std::string error = {}
	);

	void BeginAssetCapture(std::vector<AssetKey>& dependencies);
	void EndAssetCapture(std::vector<AssetKey>& dependencies);
	void TrackAssetDependency(const AssetKey& key);
	void TrackAssetLoad(const AssetKey& key, AssetKind kind, const path& source_path);

	[[nodiscard]] Shader CreateShader(
		bool persistent,
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
		std::string_view shader_name
	);

	[[nodiscard]] impl::TextureObject CreateTexture(
		const impl::Surface& surface,
		TextureFormat storage_format,
		TextureParams params = {}
	) const;

	[[nodiscard]] impl::TextureObject CreateTexture(
		const std::uint8_t* pixel_data,
		TextureDesc desc
	) const;

	[[nodiscard]] Audio CreateAudio(bool persistent, const path& asset_path);
	[[nodiscard]] Texture CreateTexture(
		bool persistent,
		const path& asset_path,
		TextureFormat storage_format,
		TextureParams params
	);
	[[nodiscard]] Font CreateFont(bool persistent, const path& asset_path);
	[[nodiscard]] Font CreateFont(bool persistent, impl::FontAtlasData&& data);
	[[nodiscard]] static impl::FontAtlasData PrepareFontAsset(const path& asset_path);

	[[nodiscard]] ecs::Entity CreateAsset();
	[[nodiscard]] path ResolveAssetPath(const SerializedAsset& asset) const;
	[[nodiscard]] path ResolvePathBackedAssetSource(const path& source_path) const;
	[[nodiscard]] std::optional<path> LocalizeProjectAsset(
		const AssetKey& key,
		AssetKind kind,
		const path& source_path
	);
	[[nodiscard]] std::optional<path> NormalizeProjectAssetFile(
		AssetKind kind,
		const path& source_path
	);
	void NormalizeShaderProgramConfiguration(SerializedAsset& asset, std::string_view source) const;
	[[nodiscard]] AssetKey MakeUniqueAssetKey(AssetKind kind, const path& source_path) const;
	[[nodiscard]] impl::AssetMetadata ProbeMetadata(const SerializedAsset& asset) const;
	[[nodiscard]] std::vector<AssetKey> ExpandDependencies(
		std::span<const AssetKey> dependencies
	) const;
	void ReleaseDependencies(std::span<const AssetKey> dependencies) noexcept;
	void PinProjectDependency(const AssetKey& key);
	void InitializeEngineShaderCatalog();
	[[nodiscard]] std::optional<std::string> ResolveShaderStageSourceText(
		std::string_view reference,
		const SerializedAsset& owner,
		std::optional<std::string_view> source_override = std::nullopt
	) const;
	[[nodiscard]] std::optional<std::variant<ShaderCode, ShaderPath, ShaderPair>> BuildShaderProgramSource(
		const SerializedAsset& asset,
		std::optional<std::string_view> source_override = std::nullopt,
		const std::optional<SerializedShaderProgram>& program_override = std::nullopt
	) const;

	Renderer& renderer_;
	AudioSystem* audio_{ nullptr };
	FontSystem* font_{ nullptr };

	ecs::Manager manager_;

	std::unordered_map<std::size_t, impl::JsonAssetData> jsons_;
	std::unordered_map<std::size_t, impl::PrefabAssetData> prefabs_;
	std::unordered_map<
		impl::AssetStorageKey,
		SerializedAsset,
		impl::AssetStorageKeyHash
	> catalog_;
	std::unordered_map<
		impl::AssetStorageKey,
		RuntimeAssetState,
		impl::AssetStorageKeyHash
	> runtime_states_;
	std::vector<AssetKey> project_asset_dependencies_;
	std::vector<AssetKey>* captured_asset_dependencies_{ nullptr };
	std::vector<std::weak_ptr<impl::AssetLoadBatchState>> active_batches_;
	std::vector<impl::AssetLoadTicket> project_load_tickets_;
	std::vector<impl::AssetLoadTicket> manual_load_tickets_;
	std::vector<std::shared_ptr<impl::AssetLoadBatchState>> manual_load_batches_;
	std::vector<impl::EngineShaderSource> engine_shader_sources_;
	std::vector<std::string> engine_vertex_shader_names_;
	std::vector<std::string> engine_fragment_shader_names_;

	std::optional<path> project_root_;
	std::optional<path> asset_directory_;

	std::unique_ptr<AsyncLoader> async_loader_;
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

std::optional<std::size_t> DetectTexturePathCount(
	AssetManager& assets,
	const TextureKey& texture_key,
	std::string_view marker
);

} // namespace impl

} // namespace ptgn
