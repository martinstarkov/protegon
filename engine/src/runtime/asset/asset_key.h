#pragma once

#include <compare>
#include <functional>
#include <ostream>
#include <string>
#include <utility>

#include "core/util/hash.h"
#include "core/util/strong_string.h"
#include "serialization/serialize.h"

namespace ptgn {

enum class AssetKind {
	Texture,
	Audio,
	Font,
	Json,
	Shader,
	Prefab,
	Scene,
	Unknown
};
PTGN_REFLECT_ENUM(AssetKind);

enum class AssetLoadState {
	Unloaded,
	Queued,
	Loading,
	Finalizing,
	Loaded,
	Failed
};
PTGN_REFLECT_ENUM(AssetLoadState);

struct AssetKey : public StrongString<AssetKey> {
	using StrongString::StrongString;

	constexpr AssetKey() = default;

	friend std::ostream& operator<<(std::ostream& os, const AssetKey& key) {
		os << key.value;
		return os;
	}

	PTGN_REFLECT_VALUE(AssetKey, value)
};

struct TextureKey : public AssetKey {
	using AssetKey::AssetKey;

	static constexpr AssetKind kind{ AssetKind::Texture };

	constexpr TextureKey() = default;
	constexpr TextureKey(AssetKey key) : AssetKey{ std::move(key.value) } {}

	constexpr auto operator<=>(const TextureKey&) const = default;

	PTGN_REFLECT_VALUE(TextureKey, value)
};

struct FontKey : public AssetKey {
	using AssetKey::AssetKey;

	static constexpr AssetKind kind{ AssetKind::Font };

	constexpr FontKey() = default;
	constexpr FontKey(AssetKey key) : AssetKey{ std::move(key.value) } {}

	constexpr auto operator<=>(const FontKey&) const = default;

	PTGN_REFLECT_VALUE(FontKey, value)
};

struct AudioKey : public AssetKey {
	using AssetKey::AssetKey;

	static constexpr AssetKind kind{ AssetKind::Audio };

	constexpr AudioKey() = default;
	constexpr AudioKey(AssetKey key) : AssetKey{ std::move(key.value) } {}

	constexpr auto operator<=>(const AudioKey&) const = default;

	PTGN_REFLECT_VALUE(AudioKey, value)
};

struct ShaderKey : public AssetKey {
	using AssetKey::AssetKey;

	static constexpr AssetKind kind{ AssetKind::Shader };

	constexpr ShaderKey() = default;
	constexpr ShaderKey(AssetKey key) : AssetKey{ std::move(key.value) } {}

	constexpr auto operator<=>(const ShaderKey&) const = default;

	PTGN_REFLECT_VALUE(ShaderKey, value)
};

struct JsonKey : public AssetKey {
	using AssetKey::AssetKey;

	static constexpr AssetKind kind{ AssetKind::Json };

	constexpr JsonKey() = default;
	constexpr JsonKey(AssetKey key) : AssetKey{ std::move(key.value) } {}

	constexpr auto operator<=>(const JsonKey&) const = default;

	PTGN_REFLECT_VALUE(JsonKey, value)
};

struct PrefabKey : public AssetKey {
	using AssetKey::AssetKey;

	static constexpr AssetKind kind{ AssetKind::Prefab };

	constexpr PrefabKey() = default;
	constexpr PrefabKey(AssetKey key) : AssetKey{ std::move(key.value) } {}

	constexpr auto operator<=>(const PrefabKey&) const = default;

	PTGN_REFLECT_VALUE(PrefabKey, value)
};

} // namespace ptgn

template <>
struct std::hash<ptgn::AssetKey> {
	std::size_t operator()(const ptgn::AssetKey& key) const {
		return ptgn::Hash(key.value);
	}
};

template <>
struct std::hash<ptgn::TextureKey> {
	std::size_t operator()(const ptgn::TextureKey& key) const {
		return ptgn::Hash(key.value);
	}
};

template <>
struct std::hash<ptgn::FontKey> {
	std::size_t operator()(const ptgn::FontKey& key) const {
		return ptgn::Hash(key.value);
	}
};

template <>
struct std::hash<ptgn::AudioKey> {
	std::size_t operator()(const ptgn::AudioKey& key) const {
		return ptgn::Hash(key.value);
	}
};

template <>
struct std::hash<ptgn::ShaderKey> {
	std::size_t operator()(const ptgn::ShaderKey& key) const {
		return ptgn::Hash(key.value);
	}
};

template <>
struct std::hash<ptgn::JsonKey> {
	std::size_t operator()(const ptgn::JsonKey& key) const {
		return ptgn::Hash(key.value);
	}
};

template <>
struct std::hash<ptgn::PrefabKey> {
	std::size_t operator()(const ptgn::PrefabKey& key) const {
		return ptgn::Hash(key.value);
	}
};
