#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "core/util/concepts.h"
#include "renderer/primitives/shader.h"
#include "renderer/primitives/texture.h"
#include "runtime/audio/audio.h"
#include "runtime/graphics/font.h"
#include "serialization/json/fwd.h"

namespace ptgn {

class Scene;
class AssetManager;

template <typename T>
concept AssetType =
	IsAnyOf<T, Texture, Font, Audio, Shader, json, std::reference_wrapper<const json>>;

template <AssetType T>
class AssetOrKey {
public:
	AssetOrKey() = default;

	AssetOrKey(const T& asset);			// NOSONAR

	AssetOrKey(const char* key);		// NOSONAR

	AssetOrKey(std::string_view key);	// NOSONAR

	AssetOrKey(const std::string& key); // NOSONAR

	AssetOrKey(std::size_t key_hash);	// NOSONAR

	[[nodiscard]] constexpr bool IsHashKey() const {
		return std::holds_alternative<std::size_t>(value_);
	}

	[[nodiscard]] constexpr bool IsAsset() const {
		return std::holds_alternative<T>(value_);
	}

	constexpr std::size_t GetHashKey() const {
		return std::get<std::size_t>(value_);
	}

	constexpr T GetAsset() const {
		return std::get<T>(value_);
	}

	T Get(const Scene& scene) const;

	T Get(const AssetManager& assets) const;

	const std::variant<std::size_t, T>& GetVariant() const;

private:
	/// @brief Type or hashed key of the asset.
	std::variant<std::size_t, T> value_{ std::size_t{ 0 } };
};

using TextureOrKey = AssetOrKey<Texture>;
using AudioOrKey   = AssetOrKey<Audio>;
using ShaderOrKey  = AssetOrKey<Shader>;
using JsonOrKey	   = AssetOrKey<std::reference_wrapper<const json>>;

/// @brief Defaults to default engine font.
using FontOrKey = AssetOrKey<Font>;

[[nodiscard]] std::size_t HashAsset(TextureOrKey texture);
[[nodiscard]] std::size_t HashAsset(AudioOrKey audio);
[[nodiscard]] std::size_t HashAsset(ShaderOrKey shader);
[[nodiscard]] std::size_t HashAsset(FontOrKey font);

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::TextureOrKey> {
	std::size_t operator()(const ptgn::TextureOrKey& texture) const {
		return ptgn::HashAsset(texture);
	}
};

template <>
struct hash<ptgn::ShaderOrKey> {
	std::size_t operator()(const ptgn::ShaderOrKey& shader) const {
		return ptgn::HashAsset(shader);
	}
};

template <>
struct hash<ptgn::AudioOrKey> {
	std::size_t operator()(const ptgn::AudioOrKey& audio) const {
		return ptgn::HashAsset(audio);
	}
};

template <>
struct hash<ptgn::FontOrKey> {
	std::size_t operator()(const ptgn::FontOrKey& font) const {
		return ptgn::HashAsset(font);
	}
};

} // namespace std