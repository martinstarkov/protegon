#include "runtime/asset/asset_manager.h"

#include <ecs/ecs.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <list>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/surface.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "renderer/text/font_atlas.h"
#include "renderer/text/font_cache.h"
#include "runtime/asset/asset_key.h"
#include "runtime/audio/audio.h"
#include "runtime/audio/audio_system.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/font_system.h"
#include "serialization/json/fwd.h"
#include "serialization/json/json_file.h"

namespace ptgn {

namespace {

AssetKind GetAssetKindFromEntity(ecs::Entity asset, const path& source_path) {
	using enum AssetKind;

	if (asset.Has<impl::TextureObject>()) {
		return Texture;
	}

	if (asset.Has<impl::AudioObject>()) {
		return Audio;
	}

	if (asset.Has<impl::FontAtlas>()) {
		return Font;
	}

	if (asset.Has<impl::ShaderObject>()) {
		return Shader;
	}

	if (!source_path.empty()) {
		return impl::GetAssetKind(source_path);
	}

	return Unknown;
}

} // namespace

namespace impl {

void AddAssetKey(ecs::Entity asset, AssetKey key, const std::optional<path>& path) {
	asset.Add<AssetKey>(std::move(key));
	if (path.has_value()) {
		asset.Add<impl::AssetPath>(path.value());
	}
}

AssetKind GetAssetKind(const path& path) {
	auto extension{ GetExtension(path) };

	if (MatchesExtension<Texture>(extension)) {
		return AssetKind::Texture;
	}
	if (MatchesExtension<Audio>(extension)) {
		return AssetKind::Audio;
	}
	if (MatchesExtension<Font>(extension)) {
		return AssetKind::Font;
	}
	if (MatchesExtension<json>(extension)) {
		return AssetKind::Json;
	}
	if (MatchesExtension<Shader>(extension)) {
		return AssetKind::Shader;
	}

	return AssetKind::Unknown;
}

AssetAccessor::AssetAccessor(AssetManager& assets) : assets{ assets } {}

std::vector<impl::AssetRecord> AssetAccessor::GetAssets() const {
	return assets.GetAssets();
}

bool AssetAccessor::Unload(const AssetKey& key, AssetKind kind) {
	return assets.Unload(key, kind);
}

AssetCaptureScope::AssetCaptureScope(
	AssetManager& assets, std::vector<AssetKey>& dependencies
) :
	assets_{ assets }, dependencies_{ dependencies } {
	assets_.BeginAssetCapture(dependencies_);
}

AssetCaptureScope::~AssetCaptureScope() noexcept {
	assets_.EndAssetCapture(dependencies_);
}

} // namespace impl

AssetManager::AssetManager(Renderer& renderer, AudioSystem& audio, FontSystem& font) :
	renderer_{ renderer }, audio_{ audio }, font_{ font } {
	// Note: Do not use audio or font systems here as those are constructed after asset manager.
}

void AssetManager::BeginAssetCapture(std::vector<AssetKey>& dependencies) {
	PTGN_ASSERT(
		captured_asset_dependencies_ == nullptr,
		"Asset dependency capture cannot be nested"
	);

	captured_asset_dependencies_ = &dependencies;
}

void AssetManager::EndAssetCapture(std::vector<AssetKey>& dependencies) {
	PTGN_ASSERT(
		captured_asset_dependencies_ == &dependencies,
		"Attempting to end an asset dependency capture that is not active"
	);

	captured_asset_dependencies_ = nullptr;
}

void AssetManager::TrackAssetLoad(
	const AssetKey& key, AssetKind kind, const path& source_path
) {
// Ordinary runtime loads remain transient. Only an active authoring capture (OnNew)
	// promotes them into the persistent project catalog and scene dependency list.
	if (!captured_asset_dependencies_ || key.value.empty() || kind == AssetKind::Unknown ||
		source_path.empty()) {
		return;
	}

	catalog_.insert_or_assign(
		Hash(key),
		SerializedAsset{
			.key = key,
			.kind = kind,
			.source_path = source_path,
		}
	);

	if (!std::ranges::contains(*captured_asset_dependencies_, key)) {
		captured_asset_dependencies_->emplace_back(key);
	}
}

void AssetManager::RegisterCatalog(std::span<const SerializedAsset> assets) {
	for (const auto& asset : assets) {
		PTGN_ASSERT(!asset.key.value.empty(), "Serialized asset key cannot be empty");
		PTGN_ASSERT(
			asset.kind != AssetKind::Unknown,
			"Serialized asset kind cannot be Unknown for key: ",
			asset.key
		);
		PTGN_ASSERT(
			!asset.source_path.empty(),
			"Serialized asset path cannot be empty for key: ",
			asset.key
		);

		catalog_.insert_or_assign(Hash(asset.key), asset);
	}
}

std::vector<SerializedAsset> AssetManager::GetCatalog() const {
	std::vector<SerializedAsset> assets;
	assets.reserve(catalog_.size());

	for (const auto& [_, asset] : catalog_) {
		assets.emplace_back(asset);
	}

	std::ranges::sort(assets, [](const SerializedAsset& lhs, const SerializedAsset& rhs) {
		if (lhs.key != rhs.key) {
			return lhs.key < rhs.key;
		}

		return lhs.kind < rhs.kind;
	});

	return assets;
}

void AssetManager::AddProjectAssetDependency(AssetKey key) {
	if (key.value.empty() || std::ranges::contains(project_asset_dependencies_, key)) {
		return;
	}

	PTGN_ASSERT(
		HasCatalogAsset(key),
		"Cannot add an asset to the project preload list before registering its source: ",
		key
	);

	project_asset_dependencies_.emplace_back(std::move(key));
}

void AssetManager::AddProjectAssetDependencies(std::span<const AssetKey> dependencies) {
	for (const auto& key : dependencies) {
		AddProjectAssetDependency(key);
	}
}

const std::vector<AssetKey>& AssetManager::GetProjectAssetDependencies() const {
	return project_asset_dependencies_;
}

bool AssetManager::HasCatalogAsset(const AssetKey& key) const {
	return catalog_.contains(Hash(key));
}

void AssetManager::Load(const SerializedAsset& asset) {
	Load(asset.key, asset.source_path, asset.kind);
}

void AssetManager::LoadDependencies(std::span<const AssetKey> dependencies) {
	for (const auto& key : dependencies) {
		auto it{ catalog_.find(Hash(key)) };

		PTGN_ASSERT(
			it != catalog_.end(),
			"Asset dependency is missing from the project catalog: ",
			key
		);

		Load(it->second);
	}
}

void AssetManager::LoadProjectAsset(AssetKey key, const path& asset_path) {
	AssetKey dependency{ key };

	Load(std::move(key), asset_path);

	PTGN_ASSERT(
		Has(dependency),
		"Project asset failed to load and cannot be persisted: ",
		dependency
	);

	AssetKind kind{ impl::GetAssetKind(asset_path) };

	if (kind == AssetKind::Texture && impl::IsFontAtlasPng(asset_path)) {
		kind = AssetKind::Font;
	}

	catalog_.insert_or_assign(
		Hash(dependency),
		SerializedAsset{
			.key = dependency,
			.kind = kind,
			.source_path = asset_path,
		}
	);

	AddProjectAssetDependency(std::move(dependency));
}

impl::TextureObject AssetManager::CreateTexture(
	const impl::Surface& surface, TextureFormat storage_format, TextureParams params
) const {
	PTGN_ASSERT(
		surface.GetChannelCount() == GetChannelCount(storage_format),
		"Surface and texture storage format channel count must match"
	);
	return CreateTexture(
		surface.Data(),
		TextureDesc{ .size{ surface.GetSize() }, .format = storage_format, .params{ params } }
	);
}

impl::TextureObject AssetManager::CreateTexture(
	const std::uint8_t* pixel_data, TextureDesc desc
) const {
	return impl::RendererAccessor{ renderer_ }.CreateTexture(pixel_data, desc);
}

Texture AssetManager::CreateTexture(
	bool persistent, const path& asset_path, TextureFormat storage_format, TextureParams params
) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create texture from invalid path: ", asset_path.string()
	);

	impl::Surface surface{ asset_path };

	const auto data{ surface.Data() };
	auto size{ surface.GetSize() };

	Texture texture{ CreateAsset(), persistent };

	texture.GetEntity().Add<impl::TextureObject>(CreateTexture(
		data, TextureDesc{ .size{ size }, .format = storage_format, .params{ params } }
	));

	return texture;
}

Texture AssetManager::CreateTexture(
	const path& asset_path, TextureFormat storage_format, TextureParams params
) {
	return CreateTexture(false, asset_path, storage_format, params);
}

Texture AssetManager::LoadTexture(
	TextureKey key, const path& asset_path, TextureFormat storage_format, TextureParams params
) {
	TrackAssetLoad(key, AssetKind::Texture, asset_path);

	if (auto existing{ TryGet<Texture>(key) }; existing.has_value()) {
		return existing.value();
	}
	auto texture{ CreateTexture(true, asset_path, storage_format, params) };
	impl::AddAssetKey(texture.GetEntity(), std::move(key), asset_path);
	return texture;
}

Font AssetManager::CreateFont(bool persistent, const path& asset_path) {
	Font font{ CreateAsset(), persistent };

	auto font_atlas{ FontSystem::CreateFontAtlas(renderer_, asset_path) };

	font.GetEntity().Add<impl::FontAtlas>(std::move(font_atlas));

	return font;
}

Font AssetManager::CreateFont(const path& asset_path) {
	return CreateFont(false, asset_path);
}

Font AssetManager::LoadFont(FontKey key, const path& asset_path) {
	TrackAssetLoad(key, AssetKind::Font, asset_path);

	if (auto existing{ TryGet<Font>(key) }; existing.has_value()) {
		return existing.value();
	}
	auto font{ CreateFont(true, asset_path) };
	impl::AddAssetKey(font.GetEntity(), std::move(key), asset_path);
	return font;
}

Audio AssetManager::CreateAudio(bool persistent, const path& asset_path) {
	Audio audio{ CreateAsset(), persistent };

	audio.GetEntity().Add<impl::AudioObject>(asset_path);

	return audio;
}

Audio AssetManager::CreateAudio(const path& asset_path) {
	return CreateAudio(false, asset_path);
}

Audio AssetManager::LoadAudio(AudioKey key, const path& asset_path) {
	TrackAssetLoad(key, AssetKind::Audio, asset_path);

	if (auto existing{ TryGet<Audio>(key) }; existing.has_value()) {
		return existing.value();
	}
	auto audio{ CreateAudio(true, asset_path) };
	impl::AddAssetKey(audio.GetEntity(), std::move(key), asset_path);
	return audio;
}

Shader AssetManager::CreateShader(
	bool persistent, const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
	std::string_view shader_name
) {
	Shader shader{ CreateAsset(), persistent };

	shader.GetEntity().Add<impl::ShaderObject>(
		impl::RendererAccessor{ renderer_ }.CreateShader(source, shader_name)
	);

	return shader;
}

Shader AssetManager::CreateShader(
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
) {
	return CreateShader(false, source, shader_name);
}

Shader AssetManager::LoadShader(
	ShaderKey key, const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
	std::optional<std::string_view> shader_name
) {
	if (const auto* shader_path{ std::get_if<ShaderPath>(&source) }) {
		TrackAssetLoad(key, AssetKind::Shader, shader_path->path);
	}

	if (auto existing{ TryGet<Shader>(key) }; existing.has_value()) {
		return existing.value();
	}

	auto shader{ CreateShader(true, source, shader_name.value_or(key)) };

	std::optional<path> source_path;

	if (const auto* shader_path{ std::get_if<ShaderPath>(&source) }) {
		source_path = shader_path->path;
	}

	impl::AddAssetKey(shader.GetEntity(), std::move(key), source_path);

	return shader;
}

json AssetManager::CreateJson(const path& asset_path) const {
	json j = ptgn::LoadJson(asset_path);
	return j;
}

json& AssetManager::LoadJson(const JsonKey& key, const path& asset_path) {
	TrackAssetLoad(key, AssetKind::Json, asset_path);

	auto hash{ Hash(key) };

	auto [it, _] = jsons_.try_emplace(
		hash, impl::JsonAssetData{
				  .key		   = key,
				  .source_path = asset_path,
				  .value	   = CreateJson(asset_path),
			  }
	);

	return it->second.value;
}

void AssetManager::LoadDirectory(const path& directory, bool recursive) {
	PTGN_ASSERT(
		DirectoryExists(directory) && FileExists(directory),
		"Provided path is not a valid directory: ", directory.string(),
		", current working directory is: ", GetWorkingDirectory()
	);

	std::unordered_map<AssetKind, std::unordered_set<std::size_t>> taken_asset_keys;

	auto process_entry = [&](const fs::directory_entry& entry) {
		if (!entry.is_regular_file()) {
			return;
		}

		const path& asset_path = entry.path();

		// Use filename without extension as key.
		// This is because including the extension would make
		// music.mp3 and music.ogg have different keys even though both would be loaded as audio
		// assets and only the first loaded one could ever be accessed.
		AssetKey key{ asset_path.stem().string() };

		auto hash{ Hash(key) };

		auto kind{ impl::GetAssetKind(asset_path) };

		PTGN_ASSERT(
			!taken_asset_keys[kind].contains(hash), "Duplicate ", json(kind),
			" key detected while loading directory: ", key
		);

		taken_asset_keys[kind].insert(hash);

		Load(std::move(key), asset_path, kind);
	};

	if (recursive) {
		for (const auto& entry : fs::recursive_directory_iterator(GetAbsolutePath(directory))) {
			process_entry(entry);
		}
	} else {
		for (const auto& entry : fs::directory_iterator(GetAbsolutePath(directory))) {
			process_entry(entry);
		}
	}
}

void AssetManager::LoadManifest(const path& asset_manifest_file) {
	PTGN_ASSERT(
		HasExtension(asset_manifest_file, ".json"), "Asset manifest file must be json file"
	);

	json assets = CreateJson(asset_manifest_file);

	PTGN_ASSERT(
		assets.is_object(),
		"Expected json object, but got something else for assets: ", assets.dump(4)
	);

	std::unordered_map<AssetKind, std::unordered_set<std::size_t>> taken_asset_keys;

	for (const auto& [key, asset_variant] : assets.items()) {
		auto key_hash{ Hash(key) };

		if (asset_variant.is_array()) {
			PTGN_ASSERT(
				!taken_asset_keys[AssetKind::Shader].contains(key_hash),
				"Shader key should not be repeated more than once: ", key
			);
			PTGN_ASSERT(
				asset_variant.size() == 2, "Shader asset array must have exactly two elements"
			);
			PTGN_ASSERT(
				asset_variant[0].is_string() && asset_variant[1].is_string(),
				"Shader asset array elements must both be strings"
			);
			ShaderPair shader_pair{ asset_variant[0].get<std::string>(),
									asset_variant[1].get<std::string>() };
			Load(std::move(key), shader_pair);
			continue;
		}

		PTGN_ASSERT(
			asset_variant.is_string(),
			"Expected string, but got something else for asset path: ", asset_variant.dump(4)
		);

		path asset_path{ asset_variant.get<std::string>() };

		auto kind{ impl::GetAssetKind(asset_path) };

		PTGN_ASSERT(
			!taken_asset_keys[kind].contains(key_hash), json(kind),
			" key should not be repeated more than once: ", key
		);

		taken_asset_keys[kind].insert(key_hash);

		Load(std::move(key), asset_path);
	}
}

void AssetManager::Load(
	const std::vector<std::pair<AssetKey, std::variant<path, ShaderCode, ShaderPair>>>&
		asset_keys_and_paths
) {
	for (const auto& [asset_key, asset_variant] : asset_keys_and_paths) {
		std::visit([this, &asset_key](const auto& v) { Load(asset_key, v); }, asset_variant);
	}
}

void AssetManager::Load(ShaderKey key, const ShaderCode& shader_code) {
	LoadShader(std::move(key), shader_code, std::nullopt);
}

void AssetManager::Load(ShaderKey key, const ShaderPair& shader_pair) {
	LoadShader(std::move(key), shader_pair, std::nullopt);
}

void AssetManager::Load(AssetKey key, const path& asset_path, AssetKind kind) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot get non-existent ", json(kind),
		" file: ", asset_path.string()
	);

	switch (kind) {
		using enum AssetKind;
		case Texture:
			if (impl::IsFontAtlasPng(asset_path)) {
				LoadFont(std::move(key), asset_path);
			} else {
				LoadTexture(std::move(key), asset_path);
			}
			break;
		case Audio:	 LoadAudio(std::move(key), asset_path); break;
		case Font:	 LoadFont(std::move(key), asset_path); break;
		case Json:	 LoadJson(std::move(key), asset_path); break;

		case Shader: {
			if (auto shader_content = FileToString(asset_path);
				!HasVertexAndFragmentShader(shader_content)) {
				// Skip shader files that don't contain both
				// vertex and fragment shader code since
				// those can't be loaded as standalone shader
				// assets. This allows for load directory to
				// be used on directories containing shader
				// files that are meant to be used as part of
				// shader pairs without causing errors.
				return;
			}
			LoadShader(std::move(key), asset_path, std::nullopt);
			break;
		}

		case Unknown: [[fallthrough]];
		default:
			PTGN_ERROR(
				"Attempting to load unsupported file extension from asset file: ",
				asset_path.string()
			);
			break;
	}
}

void AssetManager::Load(AssetKey key, const path& asset_path) {
	auto kind{ impl::GetAssetKind(asset_path) };
	Load(std::move(key), asset_path, kind);
}

ecs::Entity AssetManager::CreateAsset() {
	auto asset{ manager_.CreateEntity() };
	manager_.Refresh();
	return asset;
}

template <AssetType T>
bool HasAssetImpl(const ecs::Manager& manager, const AssetKey& key) {
	auto hash{ Hash(key) };
	using TObject = typename impl::AssetInfo<T>::Object;
	return manager.EntitiesWith<TObject, AssetKey>().AnyOf(
		[hash](auto, const auto&, const auto& asset_key) { return Hash(asset_key) == hash; }
	);
}

template <AssetType T>
std::optional<T> TryGetAssetImpl(const ecs::Manager& manager, const AssetKey& key) {
	auto hash{ Hash(key) };

	using TObject = typename impl::AssetInfo<T>::Object;

	for (auto [entity, _asset, asset_key] : manager.EntitiesWith<TObject, AssetKey>()) {
		if (Hash(asset_key) == hash) {
			return T{ entity, true };
		}
	}
	return std::nullopt;
}

template <AssetType T>
bool UnloadAssetImpl(ecs::Manager& manager, const AssetKey& key) {
	bool unloaded{ false };

	auto hash{ Hash(key) };

	using TObject = typename impl::AssetInfo<T>::Object;

	for (auto [entity, _asset, asset_key] : manager.EntitiesWith<TObject, AssetKey>()) {
		if (Hash(asset_key) == hash) {
			unloaded = true;
			entity.Destroy();
		}
	}

	manager.Refresh();
	return unloaded;
}

template <AssetType T>
bool AssetManager::Unload(const AssetKey& key) {
	if constexpr (std::is_same_v<std::remove_cvref_t<T>, json>) {
		return jsons_.erase(Hash(key)) != 0;
	} else {
		return UnloadAssetImpl<T>(manager_, key);
	}
}

template <AssetType T>
std::optional<ConstAsset<T>> AssetManager::TryGet(const AssetKey& key) const {
	if constexpr (std::is_same_v<std::remove_cvref_t<T>, json>) {
		auto it = jsons_.find(Hash(key));
		if (it == jsons_.end()) {
			return std::nullopt;
		}
		return std::cref(it->second.value);
	} else {
		return TryGetAssetImpl<T>(manager_, key);
	}
}

template <AssetType T>
std::optional<Asset<T>> AssetManager::TryGet(const AssetKey& key) {
	if constexpr (std::is_same_v<std::remove_cvref_t<T>, json>) {
		auto it = jsons_.find(Hash(key));
		if (it == jsons_.end()) {
			return std::nullopt;
		}
		return std::ref(it->second.value);
	} else {
		return TryGetAssetImpl<T>(manager_, key);
	}
}

template <AssetType T>
ConstAsset<T> AssetManager::Get(const AssetKey& key) const {
	auto asset{ TryGet<T>(key) };
	PTGN_ASSERT(asset.has_value(), "Asset not found for key: ", key);
	return asset.value();
}

template <AssetType T>
Asset<T> AssetManager::Get(const AssetKey& key) {
	auto asset{ TryGet<T>(key) };
	PTGN_ASSERT(asset.has_value(), "Asset not found for key: ", key);
	return asset.value();
}

bool AssetManager::Has(const AssetKey& key, AssetKind kind) const {
	switch (kind) {
		using enum AssetKind;

		case Texture: return Has<ptgn::Texture>(key);
		case Audio:	  return Has<ptgn::Audio>(key);
		case Font:	  return Has<ptgn::Font>(key);
		case Json:	  return Has<json>(key);
		case Shader:  return Has<ptgn::Shader>(key);
		case Unknown: break;
	}

	return false;
}

template <AssetType T>
bool AssetManager::Has(const AssetKey& key) const {
	if constexpr (std::is_same_v<std::remove_cvref_t<T>, json>) {
		return jsons_.contains(Hash(key));
	} else {
		return HasAssetImpl<T>(manager_, key);
	}
}

std::size_t AssetManager::Size() const {
	return manager_.Size() + jsons_.size();
}

V2_int AssetManager::GetTextureSize(const TextureKey& key) const {
	return Get<Texture>(key).GetEntity().Get<impl::TextureObject>().GetSize();
}

V2_int AssetManager::GetFontAtlasSize(const FontKey& key) const {
	return Get<Font>(key).GetEntity().Get<impl::FontAtlas>().GetSize();
}

impl::TextureId AssetManager::GetFontAtlasTexture(const FontKey& key) const {
	return Get<Font>(key).GetEntity().Get<impl::FontAtlas>().GetTexture();
}

std::vector<impl::AssetRecord> AssetManager::GetAssets() const {
	std::vector<impl::AssetRecord> records;
	records.reserve(manager_.Size() + jsons_.size());

	for (auto [asset, key] : manager_.EntitiesWith<AssetKey>()) {
		path source_path;

		if (auto asset_path{ asset.TryGet<impl::AssetPath>() }) {
			source_path = asset_path->value;
		}

		impl::AssetRecord record{
			.key		 = key,
			.source_path = source_path,
			.kind		 = GetAssetKindFromEntity(asset, source_path),
		};

		if (auto texture{ asset.TryGet<impl::TextureObject>() }) {
			record.preview = impl::AssetPreview{
				.texture = static_cast<impl::TextureId>(*texture),
				.size	 = texture->GetSize(),
			};
		} else if (auto font{ asset.TryGet<impl::FontAtlas>() }) {
			record.preview = impl::AssetPreview{
				.texture = font->GetTexture(),
				.size	 = font->GetSize(),
			};
		}

		records.emplace_back(std::move(record));
	}

	for (const auto& [_, asset] : jsons_) {
		records.emplace_back(
			impl::AssetRecord{
				.key		 = asset.key,
				.source_path = asset.source_path,
				.kind		 = AssetKind::Json,
			}
		);
	}

	return records;
}

bool AssetManager::Has(const AssetKey& key) const {
	return Has<Texture>(key) || Has<Audio>(key) || Has<Font>(key) || Has<Shader>(key) ||
		   Has<json>(key);
}

bool AssetManager::Unload(const AssetKey& key, AssetKind kind) {
	switch (kind) {
		case AssetKind::Texture: return Unload<Texture>(key);
		case AssetKind::Audio:	 return Unload<Audio>(key);
		case AssetKind::Font:	 return Unload<Font>(key);
		case AssetKind::Json:	 return Unload<json>(key);
		case AssetKind::Shader:	 return Unload<Shader>(key);
		case AssetKind::Unknown: break;
	}

	return false;
}

template bool AssetManager::Unload<json>(const AssetKey&);
template bool AssetManager::Unload<Font>(const AssetKey&);
template bool AssetManager::Unload<Texture>(const AssetKey&);
template bool AssetManager::Unload<Audio>(const AssetKey&);
template bool AssetManager::Unload<Shader>(const AssetKey&);

template bool AssetManager::Has<json>(const AssetKey&) const;
template bool AssetManager::Has<Font>(const AssetKey&) const;
template bool AssetManager::Has<Texture>(const AssetKey&) const;
template bool AssetManager::Has<Audio>(const AssetKey&) const;
template bool AssetManager::Has<Shader>(const AssetKey&) const;

template ConstAsset<json> AssetManager::Get<json>(const AssetKey&) const;
template ConstAsset<Font> AssetManager::Get<Font>(const AssetKey&) const;
template ConstAsset<Texture> AssetManager::Get<Texture>(const AssetKey&) const;
template ConstAsset<Audio> AssetManager::Get<Audio>(const AssetKey&) const;
template ConstAsset<Shader> AssetManager::Get<Shader>(const AssetKey&) const;

template Asset<json> AssetManager::Get<json>(const AssetKey&);
template Asset<Font> AssetManager::Get<Font>(const AssetKey&);
template Asset<Texture> AssetManager::Get<Texture>(const AssetKey&);
template Asset<Audio> AssetManager::Get<Audio>(const AssetKey&);
template Asset<Shader> AssetManager::Get<Shader>(const AssetKey&);

template std::optional<ConstAsset<json>> AssetManager::TryGet<json>(const AssetKey&) const;
template std::optional<ConstAsset<Font>> AssetManager::TryGet<Font>(const AssetKey&) const;
template std::optional<ConstAsset<Texture>> AssetManager::TryGet<Texture>(const AssetKey&) const;
template std::optional<ConstAsset<Audio>> AssetManager::TryGet<Audio>(const AssetKey&) const;
template std::optional<ConstAsset<Shader>> AssetManager::TryGet<Shader>(const AssetKey&) const;

template std::optional<Asset<json>> AssetManager::TryGet<json>(const AssetKey&);
template std::optional<Asset<Font>> AssetManager::TryGet<Font>(const AssetKey&);
template std::optional<Asset<Texture>> AssetManager::TryGet<Texture>(const AssetKey&);
template std::optional<Asset<Audio>> AssetManager::TryGet<Audio>(const AssetKey&);
template std::optional<Asset<Shader>> AssetManager::TryGet<Shader>(const AssetKey&);

} // namespace ptgn