#include "runtime/asset/asset_manager.h"

#include <SDL3_mixer/SDL_mixer.h>

#include <filesystem>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "app/context.h"
#include "core/assert.h"
#include "core/log.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "core/util/string.h"
#include "ecs/ecs.h"
#include "renderer/image/surface.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/shader.h"
#include "renderer/primitives/texture.h"
#include "renderer/primitives/texture_format.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset.h"
#include "runtime/asset/font_system.h"
#include "runtime/audio/audio.h"
#include "runtime/audio/audio_system.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/text.h"
#include "serialization/json/json.h"

#ifdef CreateFont
#undef CreateFont
#endif

namespace ptgn {

namespace impl {

void AddAssetKey(ecs::Entity asset, std::size_t key_hash, std::optional<path> path) {
	asset.Add<impl::AssetKey>(key_hash);
	if (path.has_value()) {
		asset.Add<ptgn::path>(*path);
	}
}

void AddAssetKey(ecs::Entity asset, std::string_view key, std::optional<path> path) {
	asset.Add<impl::AssetName>(key);
	auto key_hash{ Hash(key) };
	AddAssetKey(asset, key_hash, path);
}

} // namespace impl

Texture AssetManager::CreateTexture(bool persistent, const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create texture from invalid path: ", asset_path.string()
	);

	impl::Surface surface{ asset_path };

	const auto data{ surface.Data() };
	auto size{ surface.GetSize() };

	Texture texture{ CreateAsset(), persistent };

	texture.GetEntity().Add<impl::TextureObject>(
		ctx_->renderer.CreateTexture(data, size, TextureFormat::RGBA8)
	);

	return texture;
}

Texture AssetManager::CreateTexture(const path& asset_path) {
	return CreateTexture(false, asset_path);
}

Texture AssetManager::LoadTexture(std::string_view key, const path& asset_path) {
	auto texture{ CreateTexture(true, asset_path) };
	impl::AddAssetKey(texture.GetEntity(), key, asset_path);
	return texture;
}

Font AssetManager::CreateFont(bool persistent, const path& asset_path, float font_size) {
	Font font{ CreateAsset(), persistent };

	font.GetEntity().Add<FontSize>(font_size);

	auto f{ FontSystem::CreateFont(asset_path, font_size) };

	font.GetEntity().Add<std::shared_ptr<TTF_Font>>(f);

	return font;
}

Font AssetManager::CreateFont(const path& asset_path, float font_size) {
	return CreateFont(false, asset_path, font_size);
}

Font AssetManager::LoadFont(std::string_view key, const path& asset_path, float font_size) {
	auto font{ CreateFont(true, asset_path, font_size) };
	impl::AddAssetKey(font.GetEntity(), key, asset_path);
	return font;
}

Audio AssetManager::CreateAudio(bool persistent, const path& asset_path) {
	Audio audio{ CreateAsset(), persistent };

	auto a{ ctx_->audio.CreateAudio(asset_path) };

	audio.GetEntity().Add<std::shared_ptr<MIX_Audio>>(a);

	return audio;
}

Audio AssetManager::CreateAudio(const path& asset_path) {
	return CreateAudio(false, asset_path);
}

Audio AssetManager::LoadAudio(std::string_view key, const path& asset_path) {
	auto audio{ CreateAudio(true, asset_path) };
	impl::AddAssetKey(audio.GetEntity(), key, asset_path);
	return audio;
}

Shader AssetManager::CreateShader(
	bool persistent, const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
	std::string_view shader_name
) {
	Shader shader{ CreateAsset(), persistent };

	shader.GetEntity().Add<impl::ShaderObject>(ctx_->renderer.CreateShader(source, shader_name));

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
	auto shader{ CreateShader(true, source, shader_name.value_or(key)) };
	impl::AddAssetKey(shader.GetEntity(), key, {});
	return shader;
}

json AssetManager::CreateJson(const path& asset_path) {
	return ptgn::LoadJson(asset_path);
}

json& AssetManager::LoadJson(std::string_view key, const path& asset_path) {
	auto [it, _] = jsons_.insert_or_assign(Hash(key), ptgn::LoadJson(asset_path));
	return it->second;
}

void AssetManager::LoadDirectory(const path& directory, bool recursive) {
	PTGN_ASSERT(
		FileExists(directory) && DirectoryExists(directory),
		"Provided path is not a valid directory: ", directory.string()
	);

	std::unordered_set<std::size_t> taken_asset_keys;

	auto process_entry = [&](const fs::directory_entry& entry) {
		if (!entry.is_regular_file()) {
			return;
		}

		const path& filepath = entry.path();

		// Use filename without extension as key
		std::string key = filepath.stem().string();
		auto key_hash	= Hash(key);

		PTGN_ASSERT(
			taken_asset_keys.count(key_hash) == 0,
			"Duplicate asset key detected while loading directory: ", key
		);

		taken_asset_keys.insert(key_hash);

		Load(key, filepath);
	};

	if (recursive) {
		for (const auto& entry : fs::recursive_directory_iterator(directory)) {
			process_entry(entry);
		}
	} else {
		for (const auto& entry : fs::directory_iterator(directory)) {
			process_entry(entry);
		}
	}
}

void AssetManager::LoadMany(const path& asset_manifest_file) {
	PTGN_ASSERT(
		ToLower(asset_manifest_file.extension().string()) == ".json",
		"Asset manifest file must be json file"
	);

	json assets = ptgn::LoadJson(asset_manifest_file);

	PTGN_ASSERT(
		assets.is_object(),
		"Expected json object, but got something else for assets: ", assets.dump(4)
	);

	std::unordered_set<std::size_t> taken_asset_keys;

	for (const auto& [key, asset_variant] : assets.items()) {
		auto key_hash{ Hash(key) };

		PTGN_ASSERT(
			taken_asset_keys.count(key_hash) == 0,
			"Asset key should not be repeated more than once: ", key
		);

		taken_asset_keys.insert(key_hash);

		if (asset_variant.is_array()) {
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

		path filepath{ asset_variant.get<std::string>() };

		Load(key, filepath);
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

void AssetManager::Load(std::string_view key, const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot load non-existent asset file: ", asset_path.string()
	);

	std::string ext{ ToLower(asset_path.extension().string()) };

	PTGN_ASSERT(!ext.empty(), "Asset file extension is invalid: ", asset_path.string());

	if (ext == ".png" || ext == ".jpg" || ext == ".bmp" || ext == ".gif") {
		LoadTexture(key, asset_path);
	} else if (ext == ".ogg" || ext == ".mp3" || ext == ".wav" || ext == ".opus") {
		LoadAudio(key, asset_path);
	} else if (ext == ".ttf" || ext == ".otf") {
		LoadFont(key, asset_path);
	} else if (ext == ".json") {
		LoadJson(key, asset_path);
	} else if (ext == ".glsl") {
		if (auto shader_content{ FileToString(asset_path) };
			!HasVertexAndFragmentShader(shader_content)) {
			// Skip shader files that don't contain both vertex and fragment shader code since they
			// can't be loaded as standalone shader assets. This allows for load directory to be
			// used on directories containing shader files that are meant to be used as part of
			// shader pairs without causing errors.
			return;
		}
		LoadShader(key, asset_path, std::nullopt);
	} else {
		PTGN_ERROR(
			"Attempting to load unsupported file extension from asset file: ", asset_path.string()
		);
	}
}

template <typename ResourceComponent>
bool UnloadAssetImpl(ecs::Manager& manager, std::string_view key) {
	auto hash{ Hash(key) };

	bool unloaded{ false };

	for (auto [entity, k, resource] : manager.EntitiesWith<impl::AssetKey, ResourceComponent>()) {
		if (k.hash == hash) {
			unloaded = true;
			entity.Destroy();
		}
	}

	manager.Refresh();
	return unloaded;
}

bool AssetManager::UnloadAudio(std::string_view key) {
	return UnloadAssetImpl<std::shared_ptr<MIX_Audio>>(manager_, key);
}

bool AssetManager::UnloadJson(std::string_view key) {
	return jsons_.erase(Hash(key)) != 0;
}

bool AssetManager::UnloadShader(std::string_view key) {
	return UnloadAssetImpl<impl::ShaderObject>(manager_, key);
}

bool AssetManager::UnloadTexture(std::string_view key) {
	return UnloadAssetImpl<impl::TextureObject>(manager_, key);
}

bool AssetManager::UnloadFont(std::string_view key) {
	return UnloadAssetImpl<std::shared_ptr<TTF_Font>>(manager_, key);
}

ecs::Entity AssetManager::CreateAsset() {
	auto asset{ manager_.CreateEntity() };
	manager_.Refresh();
	return asset;
}

template <typename ResourceComponent, typename HandleType>
std::optional<HandleType> GetAssetImpl(const ecs::Manager& manager, std::size_t hash) {
	for (auto [entity, k, resource] : manager.EntitiesWith<impl::AssetKey, ResourceComponent>()) {
		if (k.hash == hash) {
			return HandleType{ entity, true };
		}
	}

	return std::nullopt;
}

std::optional<Audio> AssetManager::GetAudio(std::size_t key_hash) const {
	return GetAssetImpl<std::shared_ptr<MIX_Audio>, Audio>(manager_, key_hash);
}

std::optional<Shader> AssetManager::GetShader(std::size_t key_hash) const {
	return GetAssetImpl<impl::ShaderObject, Shader>(manager_, key_hash);
}

std::optional<Texture> AssetManager::GetTexture(std::size_t key_hash) const {
	return GetAssetImpl<impl::TextureObject, Texture>(manager_, key_hash);
}

std::optional<Font> AssetManager::GetFont(std::size_t key_hash) const {
	return GetAssetImpl<std::shared_ptr<TTF_Font>, Font>(manager_, key_hash);
}

std::optional<std::reference_wrapper<json>> AssetManager::GetJson(std::size_t key_hash) {
	auto it = jsons_.find(key_hash);

	if (it == jsons_.end()) {
		return std::nullopt;
	}

	return std::ref(it->second);
}

std::optional<std::reference_wrapper<const json>> AssetManager::GetJson(std::size_t key_hash
) const {
	auto it = jsons_.find(key_hash);

	if (it == jsons_.end()) {
		return std::nullopt;
	}

	return std::cref(it->second);
}

std::optional<std::reference_wrapper<const json>> AssetManager::GetJson(std::string_view key
) const {
	return GetJson(Hash(key));
}

std::optional<std::reference_wrapper<json>> AssetManager::GetJson(std::string_view key) {
	return GetJson(Hash(key));
}

std::optional<Audio> AssetManager::GetAudio(std::string_view key) const {
	return GetAudio(Hash(key));
}

std::optional<Shader> AssetManager::GetShader(std::string_view key) const {
	return GetShader(Hash(key));
}

std::optional<Texture> AssetManager::GetTexture(std::string_view key) const {
	return GetTexture(Hash(key));
}

std::optional<Font> AssetManager::GetFont(std::string_view key) const {
	return GetFont(Hash(key));
}

template <typename ResourceComponent>
bool HasAssetImpl(const ecs::Manager& manager, std::size_t key_hash) {
	for (auto [entity, k, resource] : // NOSONAR
		 manager.EntitiesWith<impl::AssetKey, ResourceComponent>()) {
		if (k.hash == key_hash) {
			return true;
		}
	}

	return false;
}

template <>
bool AssetManager::Unload<json>(std::string_view key) {
	return UnloadJson(key);
}

template <>
bool AssetManager::Unload<std::reference_wrapper<const json>>(std::string_view key) {
	return UnloadJson(key);
}

template <>
bool AssetManager::Unload<Shader>(std::string_view key) {
	return UnloadShader(key);
}

template <>
bool AssetManager::Unload<Texture>(std::string_view key) {
	return UnloadTexture(key);
}

template <>
bool AssetManager::Unload<Audio>(std::string_view key) {
	return UnloadAudio(key);
}

template <>
bool AssetManager::Unload<Font>(std::string_view key) {
	return UnloadFont(key);
}

template <>
std::optional<std::reference_wrapper<const json>>
AssetManager::Get<std::reference_wrapper<const json>>(std::size_t key_hash) const {
	return GetJson(key_hash);
}

template <>
std::optional<Shader> AssetManager::Get<Shader>(std::size_t key_hash) const {
	return GetShader(key_hash);
}

template <>
std::optional<Texture> AssetManager::Get<Texture>(std::size_t key_hash) const {
	return GetTexture(key_hash);
}

template <>
std::optional<Audio> AssetManager::Get<Audio>(std::size_t key_hash) const {
	return GetAudio(key_hash);
}

template <>
std::optional<Font> AssetManager::Get<Font>(std::size_t key_hash) const {
	return GetFont(key_hash);
}

template <>
std::optional<std::reference_wrapper<const json>>
AssetManager::Get<std::reference_wrapper<const json>>(std::string_view key) const {
	return GetJson(key);
}

template <>
std::optional<Shader> AssetManager::Get<Shader>(std::string_view key) const {
	return GetShader(key);
}

template <>
std::optional<Texture> AssetManager::Get<Texture>(std::string_view key) const {
	return GetTexture(key);
}

template <>
std::optional<Audio> AssetManager::Get<Audio>(std::string_view key) const {
	return GetAudio(key);
}

template <>
std::optional<Font> AssetManager::Get<Font>(std::string_view key) const {
	return GetFont(key);
}

template <>
bool AssetManager::Has<json>(std::string_view key) const {
	return HasJson(key);
}

template <>
bool AssetManager::Has<std::reference_wrapper<const json>>(std::string_view key) const {
	return HasJson(key);
}

template <>
bool AssetManager::Has<Shader>(std::string_view key) const {
	return HasShader(key);
}

template <>
bool AssetManager::Has<Texture>(std::string_view key) const {
	return HasTexture(key);
}

template <>
bool AssetManager::Has<Audio>(std::string_view key) const {
	return HasAudio(key);
}

template <>
bool AssetManager::Has<Font>(std::string_view key) const {
	return HasFont(key);
}

template <>
bool AssetManager::Has<json>(std::size_t key_hash) const {
	return jsons_.contains(key_hash);
}

template <>
bool AssetManager::Has<std::reference_wrapper<const json>>(std::size_t key_hash) const {
	return jsons_.contains(key_hash);
}

template <>
bool AssetManager::Has<Shader>(std::size_t key_hash) const {
	return HasAssetImpl<impl::ShaderObject>(manager_, key_hash);
}

template <>
bool AssetManager::Has<Texture>(std::size_t key_hash) const {
	return HasAssetImpl<impl::TextureObject>(manager_, key_hash);
}

template <>
bool AssetManager::Has<Audio>(std::size_t key_hash) const {
	return HasAssetImpl<std::shared_ptr<MIX_Audio>>(manager_, key_hash);
}

template <>
bool AssetManager::Has<Font>(std::size_t key_hash) const {
	return HasAssetImpl<std::shared_ptr<TTF_Font>>(manager_, key_hash);
}

bool AssetManager::HasJson(std::string_view key) const {
	return Has<json>(Hash(key));
}

bool AssetManager::HasAudio(std::string_view key) const {
	return Has<Audio>(Hash(key));
}

bool AssetManager::HasShader(std::string_view key) const {
	return Has<Shader>(Hash(key));
}

bool AssetManager::HasTexture(std::string_view key) const {
	return Has<Texture>(Hash(key));
}

bool AssetManager::HasFont(std::string_view key) const {
	return Has<Font>(Hash(key));
}

std::optional<impl::TextureObject> AssetManager::CreateTextTextureObject(
	std::string_view text_content, Color color, float font_size, FontOrKey font,
	const TextProperties& properties, std::optional<float> hd_scale
) {
	auto surface{
		ctx_->font.CreateTextSurface(text_content, color, font_size, font, properties, hd_scale)
	};

	if (!surface.has_value()) {
		return {};
	}

	const auto pixel_data{ surface->Data() };
	auto size{ surface->GetSize() };

	return ctx_->renderer.CreateTexture(pixel_data, size, TextureFormat::RGBA8);
}

Texture AssetManager::CreateTextTexture(
	std::string_view text_content, Color color, float font_size, FontOrKey font,
	const TextProperties& properties, std::optional<float> hd_scale
) {
	Texture texture{ CreateAsset(), false };

	auto texture_object{
		CreateTextTextureObject(text_content, color, font_size, font, properties, hd_scale)
	};

	if (!texture_object.has_value()) {
		return texture;
	}

	texture.GetEntity().Add<impl::TextureObject>(std::move(*texture_object));

	return texture;
}

// TODO: Get rid of this in favor of pointer to renderer and audio system.
void AssetManager::Init(const std::shared_ptr<ApplicationContext>& ctx) {
	ctx_ = ctx;
}

} // namespace ptgn