#include "runtime/asset/asset_manager.h"

#include <ecs/ecs.h>

#include <filesystem>
#include <fstream>
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
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "core/util/string.h"
#include "renderer/renderer.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "runtime/audio/audio.h"
#include "runtime/audio/audio_system.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/font_system.h"
#include "serialization/json/fwd.h"

namespace ptgn {

namespace impl {

void AddAssetKey(ecs::Entity asset, std::string_view key, const std::optional<path>& path) {
	asset.Add<impl::AssetKey>(key);
	if (path.has_value()) {
		asset.Add<impl::AssetPath>(*path);
	}
}

AssetKind GetAssetKind(const path& path) {
	auto extension{ ToLower(path.extension().string()) };

	PTGN_ASSERT(!extension.empty(), "Asset file extension is missing: ", path.string());

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

} // namespace impl

AssetManager::AssetManager(impl::Renderer& renderer, AudioSystem& audio, FontSystem& font) :
	renderer_{ renderer }, audio_{ audio }, font_{ font } {
	// Note: Do not use audio or font here as they are constructed after asset manager.
}

Texture AssetManager::CreateTexture(bool persistent, const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create texture from invalid path: ", asset_path.string()
	);

	impl::Surface surface{ asset_path };

	const auto data{ surface.Data() };
	auto size{ surface.GetSize() };

	Texture texture{ CreateAsset(), persistent };

	texture.GetEntity().Add<impl::TextureObject>(
		renderer_.CreateTexture(data, size, TextureFormat::RGBA8)
	);

	return texture;
}

Texture AssetManager::CreateTexture(const path& asset_path) {
	return CreateTexture(false, asset_path);
}

Texture AssetManager::LoadTexture(std::string_view key, const path& asset_path) {
	if (auto existing{ TryGet<Texture>(key) }; existing.has_value()) {
		return *existing;
	}
	auto texture{ CreateTexture(true, asset_path) };
	impl::AddAssetKey(texture.GetEntity(), key, asset_path);
	return texture;
}

Font AssetManager::CreateFont(bool persistent, const path& asset_path, float font_size) {
	Font font{ CreateAsset(), persistent };

	font.GetEntity().Add<FontSize>(font_size);

	auto f{ FontSystem::CreateFont(asset_path, font_size) };

	font.GetEntity().Add<impl::FontObject>(f);

	return font;
}

Font AssetManager::CreateFont(const path& asset_path, float font_size) {
	return CreateFont(false, asset_path, font_size);
}

Font AssetManager::LoadFont(std::string_view key, const path& asset_path, float font_size) {
	if (auto existing{ TryGet<Font>(key) }; existing.has_value()) {
		return *existing;
	}
	auto font{ CreateFont(true, asset_path, font_size) };
	impl::AddAssetKey(font.GetEntity(), key, asset_path);
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

Audio AssetManager::LoadAudio(std::string_view key, const path& asset_path) {
	if (auto existing{ TryGet<Audio>(key) }; existing.has_value()) {
		return *existing;
	}
	auto audio{ CreateAudio(true, asset_path) };
	impl::AddAssetKey(audio.GetEntity(), key, asset_path);
	return audio;
}

Shader AssetManager::CreateShader(
	bool persistent, const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
	std::string_view shader_name
) {
	Shader shader{ CreateAsset(), persistent };

	shader.GetEntity().Add<impl::ShaderObject>(renderer_.CreateShader(source, shader_name));

	return shader;
}

Shader AssetManager::CreateShader(
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
) {
	return CreateShader(false, source, shader_name);
}

Shader AssetManager::LoadShader(
	std::string_view key, const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
	std::optional<std::string_view> shader_name
) {
	if (auto existing{ TryGet<Shader>(key) }; existing.has_value()) {
		return *existing;
	}
	auto shader{ CreateShader(true, source, shader_name.value_or(key)) };
	impl::AddAssetKey(shader.GetEntity(), key, {});
	return shader;
}

json AssetManager::CreateJson(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path),
		"Cannot load json file from a nonexistent file path: ", asset_path.string()
	);
	std::ifstream json_file(asset_path);
	json j = json::parse(json_file);
	return j;
}

json& AssetManager::LoadJson(std::string_view key, const path& asset_path) {
	auto [it, _] = jsons_.try_emplace(Hash(key), CreateJson(asset_path));
	return it->second;
}

void AssetManager::LoadDirectory(const path& directory, bool recursive) {
	PTGN_ASSERT(
		DirectoryExists(directory) && FileExists(directory),
		"Provided path is not a valid directory: ", directory.string(),
		", current working directory is: ", GetWorkingDirectory()
	);

	std::unordered_map<impl::AssetKind, std::unordered_set<std::size_t>> taken_asset_keys;

	auto process_entry = [&](const fs::directory_entry& entry) {
		if (!entry.is_regular_file()) {
			return;
		}

		const path& asset_path = entry.path();

		// Use filename without extension as key.
		// This is because including the extension would make
		// music.mp3 and music.ogg have different keys even though both would be loaded as audio
		// assets and only the first loaded one could ever be accessed.
		std::string key{ asset_path.stem().string() };

		auto hash{ Hash(key) };

		auto kind{ impl::GetAssetKind(asset_path.extension().string()) };

		PTGN_ASSERT(
			!taken_asset_keys[kind].contains(hash), "Duplicate ", json(kind),
			" key detected while loading directory: ", key
		);

		taken_asset_keys[kind].insert(hash);

		Load(key, asset_path, kind);
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

void AssetManager::LoadMany(const path& asset_manifest_file) {
	PTGN_ASSERT(
		ToLower(asset_manifest_file.extension().string()) == ".json",
		"Asset manifest file must be json file"
	);

	json assets = CreateJson(asset_manifest_file);

	PTGN_ASSERT(
		assets.is_object(),
		"Expected json object, but got something else for assets: ", assets.dump(4)
	);

	std::unordered_map<impl::AssetKind, std::unordered_set<std::size_t>> taken_asset_keys;

	for (const auto& [key, asset_variant] : assets.items()) {
		auto key_hash{ Hash(key) };

		if (asset_variant.is_array()) {
			PTGN_ASSERT(
				!taken_asset_keys[impl::AssetKind::Shader].contains(key_hash),
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
			Load(key, shader_pair);
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

		Load(key, asset_path);
	}
}

void AssetManager::LoadMany(
	const std::vector<std::pair<std::string, std::variant<path, ShaderCode, ShaderPair>>>&
		asset_keys_and_paths
) {
	for (const auto& [asset_key, asset_variant] : asset_keys_and_paths) {
		std::visit([this, &asset_key](const auto& v) { Load(asset_key, v); }, asset_variant);
	}
}

void AssetManager::Load(std::string_view key, const ShaderCode& shader_code) {
	LoadShader(key, shader_code, std::nullopt);
}

void AssetManager::Load(std::string_view key, const ShaderPair& shader_pair) {
	LoadShader(key, shader_pair, std::nullopt);
}

void AssetManager::Load(std::string_view key, const path& asset_path, impl::AssetKind kind) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot get non-existent ", json(kind),
		" file: ", asset_path.string()
	);

	switch (kind) {
		using enum impl::AssetKind;
		case Texture: LoadTexture(key, asset_path); break;
		case Audio:	  LoadAudio(key, asset_path); break;
		case Font:	  LoadFont(key, asset_path); break;
		case Json:	  LoadJson(key, asset_path); break;

		case Shader:  {
			if (auto shader_content = FileToString(asset_path);
				!HasVertexAndFragmentShader(shader_content)) {
				// Skip shader files that don't contain both vertex and fragment shader code
				// since they can't be loaded as standalone shader assets. This allows for load
				// directory to be used on directories containing shader files that are meant to
				// be used as part of shader pairs without causing errors.
				return;
			}
			LoadShader(key, asset_path, std::nullopt);
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

void AssetManager::Load(std::string_view key, const path& asset_path) {
	auto kind{ impl::GetAssetKind(asset_path) };
	Load(key, asset_path, kind);
}

ecs::Entity AssetManager::CreateAsset() {
	auto asset{ manager_.CreateEntity() };
	manager_.Refresh();
	return asset;
}

template <AssetType T>
bool HasAssetImpl(const ecs::Manager& manager, std::string_view key) {
	auto hash{ Hash(key) };
	using TObject = typename impl::AssetInfo<T>::Object;
	return manager.EntitiesWith<TObject, impl::AssetKey>().AnyOf(
		[hash](auto, const auto&, const auto& asset_key) { return asset_key == hash; }
	);
}

template <AssetType T>
std::optional<T> TryGetAssetImpl(const ecs::Manager& manager, std::string_view key) {
	auto hash{ Hash(key) };

	using TObject = typename impl::AssetInfo<T>::Object;

	for (auto [entity, _asset, asset_key] : manager.EntitiesWith<TObject, impl::AssetKey>()) {
		if (asset_key == hash) {
			return T{ entity, true };
		}
	}
	return std::nullopt;
}

template <AssetType T>
bool UnloadAssetImpl(ecs::Manager& manager, std::string_view key) {
	bool unloaded{ false };

	auto hash{ Hash(key) };

	using TObject = typename impl::AssetInfo<T>::Object;

	for (auto [entity, _asset, asset_key] : manager.EntitiesWith<TObject, impl::AssetKey>()) {
		if (asset_key == hash) {
			unloaded = true;
			entity.Destroy();
		}
	}

	manager.Refresh();
	return unloaded;
}

template <AssetType T>
bool AssetManager::Unload(std::string_view key) {
	if constexpr (std::is_same_v<std::remove_cvref_t<T>, json>) {
		return jsons_.erase(Hash(key)) != 0;
	} else {
		return UnloadAssetImpl<T>(manager_, key);
	}
}

template <AssetType T>
std::optional<ConstAsset<T>> AssetManager::TryGet(std::string_view key) const {
	if constexpr (std::is_same_v<std::remove_cvref_t<T>, json>) {
		auto it{ jsons_.find(Hash(key)) };
		if (it == jsons_.end()) {
			return std::nullopt;
		}
		return std::cref(it->second);
	} else {
		return TryGetAssetImpl<T>(manager_, key);
	}
}

template <AssetType T>
std::optional<Asset<T>> AssetManager::TryGet(std::string_view key) {
	if constexpr (std::is_same_v<std::remove_cvref_t<T>, json>) {
		auto it{ jsons_.find(Hash(key)) };
		if (it == jsons_.end()) {
			return std::nullopt;
		}
		return std::ref(it->second);
	} else {
		return TryGetAssetImpl<T>(manager_, key);
	}
}

template <AssetType T>
ConstAsset<T> AssetManager::Get(std::string_view key) const {
	auto asset{ TryGet<T>(key) };
	PTGN_ASSERT(asset.has_value(), "Asset not found for key: ", key);
	return *asset;
}

template <AssetType T>
Asset<T> AssetManager::Get(std::string_view key) {
	auto asset{ TryGet<T>(key) };
	PTGN_ASSERT(asset.has_value(), "Asset not found for key: ", key);
	return *asset;
}

template <AssetType T>
bool AssetManager::Has(std::string_view key) const {
	if constexpr (std::is_same_v<std::remove_cvref_t<T>, json>) {
		return jsons_.contains(Hash(key));
	} else {
		return HasAssetImpl<T>(manager_, key);
	}
}

std::size_t AssetManager::Size() const {
	return manager_.Size() + jsons_.size();
}

template bool AssetManager::Unload<json>(std::string_view);
template bool AssetManager::Unload<Font>(std::string_view);
template bool AssetManager::Unload<Texture>(std::string_view);
template bool AssetManager::Unload<Audio>(std::string_view);
template bool AssetManager::Unload<Shader>(std::string_view);

template bool AssetManager::Has<json>(std::string_view) const;
template bool AssetManager::Has<Font>(std::string_view) const;
template bool AssetManager::Has<Texture>(std::string_view) const;
template bool AssetManager::Has<Audio>(std::string_view) const;
template bool AssetManager::Has<Shader>(std::string_view) const;

template ConstAsset<json> AssetManager::Get<json>(std::string_view) const;
template ConstAsset<Font> AssetManager::Get<Font>(std::string_view) const;
template ConstAsset<Texture> AssetManager::Get<Texture>(std::string_view) const;
template ConstAsset<Audio> AssetManager::Get<Audio>(std::string_view) const;
template ConstAsset<Shader> AssetManager::Get<Shader>(std::string_view) const;

template Asset<json> AssetManager::Get<json>(std::string_view);
template Asset<Font> AssetManager::Get<Font>(std::string_view);
template Asset<Texture> AssetManager::Get<Texture>(std::string_view);
template Asset<Audio> AssetManager::Get<Audio>(std::string_view);
template Asset<Shader> AssetManager::Get<Shader>(std::string_view);

template std::optional<ConstAsset<json>> AssetManager::TryGet<json>(std::string_view) const;
template std::optional<ConstAsset<Font>> AssetManager::TryGet<Font>(std::string_view) const;
template std::optional<ConstAsset<Texture>> AssetManager::TryGet<Texture>(std::string_view) const;
template std::optional<ConstAsset<Audio>> AssetManager::TryGet<Audio>(std::string_view) const;
template std::optional<ConstAsset<Shader>> AssetManager::TryGet<Shader>(std::string_view) const;

template std::optional<Asset<json>> AssetManager::TryGet<json>(std::string_view);
template std::optional<Asset<Font>> AssetManager::TryGet<Font>(std::string_view);
template std::optional<Asset<Texture>> AssetManager::TryGet<Texture>(std::string_view);
template std::optional<Asset<Audio>> AssetManager::TryGet<Audio>(std::string_view);
template std::optional<Asset<Shader>> AssetManager::TryGet<Shader>(std::string_view);

} // namespace ptgn