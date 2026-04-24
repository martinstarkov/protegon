#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "core/util/concepts.h"
#include "core/util/hash.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/audio/audio.h"
#include "runtime/graphics/text/font.h"
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

/// @brief Defaults to default engine font.
using FontOrKey	   = AssetOrKey<Font>;
using TextureOrKey = AssetOrKey<Texture>;
using AudioOrKey   = AssetOrKey<Audio>;
using ShaderOrKey  = AssetOrKey<Shader>;
using JsonOrKey	   = AssetOrKey<std::reference_wrapper<const json>>;

} // namespace ptgn

template <typename T>
	requires(ptgn::AssetType<T> && !std::is_same_v<T, ptgn::json> && !std::is_same_v<T, std::reference_wrapper<const ptgn::json>>)
struct std::hash<ptgn::AssetOrKey<T>> {
	std::size_t operator()(const ptgn::AssetOrKey<T>& asset) const {
		return std::visit(
			[]<typename S>(const S& value) -> std::size_t {
				if constexpr (std::is_same_v<S, T>) {
					return ptgn::Hash(value);
				} else if constexpr (std::is_same_v<S, std::size_t>) {
					return value;
				} else {
					static_assert(false, "Incomplete visitor!");
				}
			},
			asset.GetVariant()
		);
	}
};