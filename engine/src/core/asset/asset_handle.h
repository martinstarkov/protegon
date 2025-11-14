#pragma once

#include <memory>
#include <utility>

#include "core/assert.h"

namespace ptgn {

class AssetManager;

enum Asset {
	Shader,
	Texture,
	Sound,
	Music,
	Font,
	Json,
};

namespace impl {

struct ShaderAsset;
struct TextureAsset;
struct SoundAsset;
struct MusicAsset;
struct FontAsset;
struct JsonAsset;

template <Asset>
struct AssetTraits;

#define PTGN_DEFINE_ASSET_TRAIT(EnumName, TypeName) \
	template <>                                     \
	struct AssetTraits<EnumName> {                  \
		using Type = TypeName;                      \
	};

PTGN_DEFINE_ASSET_TRAIT(Shader, ShaderAsset)
PTGN_DEFINE_ASSET_TRAIT(Texture, TextureAsset)
PTGN_DEFINE_ASSET_TRAIT(Sound, SoundAsset)
PTGN_DEFINE_ASSET_TRAIT(Music, MusicAsset)
PTGN_DEFINE_ASSET_TRAIT(Font, FontAsset)
PTGN_DEFINE_ASSET_TRAIT(Json, JsonAsset)

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