#pragma once

#include <ostream>

#include "core/util/hash.h"
#include "core/util/strong_string.h"
#include "serialization/serialize.h"

namespace ptgn {

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

	constexpr TextureKey() = default;

	constexpr auto operator<=>(const TextureKey&) const = default;

	PTGN_REFLECT_VALUE(TextureKey, value)
};

struct FontKey : public AssetKey {
	using AssetKey::AssetKey;

	constexpr FontKey() = default;

	constexpr auto operator<=>(const FontKey&) const = default;

	PTGN_REFLECT_VALUE(FontKey, value)
};

struct AudioKey : public AssetKey {
	using AssetKey::AssetKey;

	constexpr AudioKey() = default;

	constexpr auto operator<=>(const AudioKey&) const = default;

	PTGN_REFLECT_VALUE(AudioKey, value)
};

struct ShaderKey : public AssetKey {
	using AssetKey::AssetKey;

	constexpr ShaderKey() = default;

	constexpr auto operator<=>(const ShaderKey&) const = default;

	PTGN_REFLECT_VALUE(ShaderKey, value)
};

struct JsonKey : public AssetKey {
	using AssetKey::AssetKey;

	constexpr JsonKey() = default;

	constexpr auto operator<=>(const JsonKey&) const = default;

	PTGN_REFLECT_VALUE(JsonKey, value)
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