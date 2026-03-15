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
#include "renderer/renderer.h"
#include "runtime/asset/font_system.h"
#include "runtime/audio/audio.h"
#include "runtime/audio/audio_system.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/text.h"
#include "serialization/json/json.h"

#ifdef CreateFont
#undef CreateFont
#endif
#include "renderer/primitives/texture_format.h"

// TODO: Add async asset loading.

namespace ptgn {

namespace impl {

void AddAssetKey(ecs::Entity asset, std::string_view key, std::optional<path> path) {
	asset.Add<impl::AssetName>(key);
	asset.Add<impl::AssetKey>(Hash(key));
	if (path.has_value()) {
		asset.Add<ptgn::path>(*path);
	}
}

} // namespace impl

Texture AssetManager::CreateTexture(bool persistent, const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create texture from invalid path: ", asset_path.string()
	);

	impl::Surface surface{ asset_path };

	Texture texture{ CreateAsset(), persistent };
	texture.GetEntity().Add<impl::TextureObject>(
		ctx_->renderer.CreateTexture(surface.pixels.data(), surface.size, TextureFormat::RGBA8)
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
	font.GetEntity().Add<impl::FontSize>(font_size);
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
	audio.entity_.Add<std::shared_ptr<MIX_Audio>>(a);

	return audio;
}

Audio AssetManager::CreateAudio(const path& asset_path) {
	return CreateAudio(false, asset_path);
}

Audio AssetManager::LoadAudio(std::string_view key, const path& asset_path) {
	auto audio{ CreateAudio(true, asset_path) };
	impl::AddAssetKey(audio.entity_, key, asset_path);
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

std::optional<Audio> AssetManager::GetAudio(std::string_view key) const {
	return GetAssetImpl<std::shared_ptr<MIX_Audio>, Audio>(manager_, Hash(key));
}

std::optional<Shader> AssetManager::GetShader(std::string_view key) const {
	return GetAssetImpl<impl::ShaderObject, Shader>(manager_, Hash(key));
}

std::optional<Texture> AssetManager::GetTexture(std::string_view key) const {
	return GetAssetImpl<impl::TextureObject, Texture>(manager_, Hash(key));
}

std::optional<Font> AssetManager::GetFont(std::string_view key) const {
	return GetFont(Hash(key));
}

std::optional<Font> AssetManager::GetFont(std::size_t key) const {
	return GetAssetImpl<std::shared_ptr<TTF_Font>, Font>(manager_, key);
}

std::optional<std::reference_wrapper<const json>> AssetManager::GetJson(std::string_view key
) const {
	auto hash{ Hash(key) };

	auto it = jsons_.find(hash);
	if (it == jsons_.end()) {
		return std::nullopt;
	}

	return std::cref(it->second);
}

std::optional<std::reference_wrapper<json>> AssetManager::GetJson(std::string_view key) {
	auto hash{ Hash(key) };

	auto it = jsons_.find(hash);
	if (it == jsons_.end()) {
		return std::nullopt;
	}

	return std::ref(it->second);
}

template <typename ResourceComponent>
bool HasAssetImpl(const ecs::Manager& manager, std::string_view key) {
	auto hash{ Hash(key) };

	for (auto [entity, k, resource] : manager.EntitiesWith<impl::AssetKey, ResourceComponent>()) {
		if (k.hash == hash) {
			return true;
		}
	}

	return false;
}

bool AssetManager::HasJson(std::string_view key) const {
	return jsons_.contains(Hash(key));
}

bool AssetManager::HasAudio(std::string_view key) const {
	return HasAssetImpl<std::shared_ptr<MIX_Audio>>(manager_, key);
}

bool AssetManager::HasShader(std::string_view key) const {
	return HasAssetImpl<impl::ShaderObject>(manager_, key);
}

bool AssetManager::HasTexture(std::string_view key) const {
	return HasAssetImpl<impl::TextureObject>(manager_, key);
}

bool AssetManager::HasFont(std::string_view key) const {
	return HasAssetImpl<std::shared_ptr<TTF_Font>>(manager_, key);
}

Shader AssetManager::ToShader(std::variant<Shader, std::string_view> shader) const {
	return std::visit(
		[&]<typename T>(const T& arg) -> Shader {
			if constexpr (std::is_same_v<T, Shader>) {
				return arg;
			} else if constexpr (std::is_same_v<T, std::string_view>) {
				PTGN_ASSERT(
					HasShader(arg),
					"Shader key must be loaded in the asset manager before retrieval"
				);

				return *GetShader(arg);
			} else {
				static_assert(false, "Incomplete visitor!");
			}
		},
		shader
	);
}

std::optional<Texture> AssetManager::ToTexture(
	std::variant<std::monostate, Texture, std::string_view> texture
) const {
	return std::visit(
		[&]<typename T>(const T& arg) -> std::optional<Texture> {
			if constexpr (std::is_same_v<T, std::monostate>) {
				return std::nullopt;
			} else if constexpr (std::is_same_v<T, Texture>) {
				return arg;
			} else if constexpr (std::is_same_v<T, std::string_view>) {
				PTGN_ASSERT(
					HasTexture(arg),
					"Texture key must be loaded in the asset manager before retrieval"
				);
				return GetTexture(arg);
			} else {
				static_assert(false, "Incomplete visitor!");
			}
		},
		texture
	);
}

Texture AssetManager::ToTexture(std::variant<Texture, std::string_view> texture) const {
	return std::visit(
		[&]<typename T>(const T& arg) -> Texture {
			if constexpr (std::is_same_v<T, Texture>) {
				return arg;
			} else if constexpr (std::is_same_v<T, std::string_view>) {
				PTGN_ASSERT(
					HasTexture(arg),
					"Texture key must be loaded in the asset manager before retrieval"
				);

				return *GetTexture(arg);
			} else {
				static_assert(false, "Incomplete visitor!");
			}
		},
		texture
	);
}

std::optional<Font> AssetManager::ToFont(std::variant<std::monostate, Font, std::string_view> font
) const {
	return std::visit(
		[&]<typename T>(const T& arg) -> std::optional<Font> {
			if constexpr (std::is_same_v<T, std::monostate>) {
				// Default engine font
				return std::nullopt;
			} else if constexpr (std::is_same_v<T, Font>) {
				return arg;
			} else if constexpr (std::is_same_v<T, std::string_view>) {
				PTGN_ASSERT(
					HasFont(arg), "Font key must be loaded in the asset manager before retrieval"
				);
				return *GetFont(arg);
			} else {
				static_assert(false, "Incomplete visitor!");
			}
		},
		font
	);
}

std::optional<impl::TextureObject> AssetManager::CreateTextTextureObject(
	std::string_view text_content, Color color, float font_size, Font font_asset,
	const TextProperties& properties, float hd_scale, bool hd
) {
	auto surface{ ctx_->font.CreateTextSurface(
		text_content, color, font_size, font_asset, properties, hd_scale, hd
	) };

	if (!surface.has_value()) {
		return {};
	}

	return ctx_->renderer.CreateTexture(
		surface->pixels.data(), surface->size, TextureFormat::RGBA8
	);
}

Texture AssetManager::CreateTextTexture(
	std::string_view text_content, Color color, float font_size, Font font_asset,
	const TextProperties& properties, float hd_scale, bool hd
) {
	Texture texture{ CreateAsset(), false };

	auto texture_object{ CreateTextTextureObject(
		text_content, color, font_size, font_asset, properties, hd_scale, hd
	) };

	if (!texture_object.has_value()) {
		return texture;
	}

	texture.GetEntity().Add<impl::TextureObject>(std::move(*texture_object));

	return texture;
}

void AssetManager::Init(const std::shared_ptr<ApplicationContext>& ctx) {
	ctx_ = ctx;
}

} // namespace ptgn