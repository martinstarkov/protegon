#pragma once

#include <memory>
#include <utility>

#include "core/assert.h"

namespace ptgn {

class AssetManager;

enum class Asset {
	Shader,
	Texture,
	Audio,
	Font,
	Json,
};

namespace impl {

struct ShaderAsset;
struct TextureAsset;
struct AudioAsset;
struct FontAsset;
struct JsonAsset;

template <Asset>
struct AssetTraits;

#define PTGN_DEFINE_ASSET_TRAIT(EnumName, TypeName) \
	template <>                                     \
	struct AssetTraits<EnumName> {                  \
		using Type = TypeName;                      \
	};

PTGN_DEFINE_ASSET_TRAIT(Asset::Shader, ShaderAsset)
PTGN_DEFINE_ASSET_TRAIT(Asset::Texture, TextureAsset)
PTGN_DEFINE_ASSET_TRAIT(Asset::Audio, AudioAsset)
PTGN_DEFINE_ASSET_TRAIT(Asset::Font, FontAsset)
PTGN_DEFINE_ASSET_TRAIT(Asset::Json, JsonAsset)

#undef PTGN_DEFINE_ASSET_TRAIT

} // namespace impl

template <Asset T>
class Handle {
public:
	Handle() = default;

	bool operator==(const Handle&) const = default;

	explicit operator bool() const {
		return static_cast<bool>(asset_);
	}

	auto& Get() {
		PTGN_ASSERT(asset_);
		return *asset_;
	}

	const auto& Get() const {
		PTGN_ASSERT(asset_);
		return *asset_;
	}

private:
	friend class AssetManager;

	using AssetType = typename impl::AssetTraits<T>::Type;

	explicit Handle(std::shared_ptr<AssetType> asset) : asset_{ std::move(asset) } {}

	std::shared_ptr<AssetType> asset_;
};

} // namespace ptgn