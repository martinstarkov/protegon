#include "runtime/asset/asset.h"

#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "core/assert.h"
#include "core/util/hash.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/audio.h"
#include "runtime/graphics/text/font.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "serialization/json/fwd.h"

namespace ptgn {

template <AssetType T>
AssetOrKey<T>::AssetOrKey(const T& asset) : value_{ asset } {}

template <AssetType T>
AssetOrKey<T>::AssetOrKey(const char* key) :
	value_{ (key == nullptr || *key == '\0') ? 0 : Hash(key) } {}

template <AssetType T>
AssetOrKey<T>::AssetOrKey(std::string_view key) : value_{ key.empty() ? 0 : Hash(key) } {}

template <AssetType T>
AssetOrKey<T>::AssetOrKey(const std::string& key) : value_{ key.empty() ? 0 : Hash(key) } {}

template <AssetType T>
AssetOrKey<T>::AssetOrKey(std::size_t key_hash) : value_{ key_hash } {}

template <AssetType T>
const std::variant<std::size_t, T>& AssetOrKey<T>::GetVariant() const {
	return value_;
}

// TODO: Move to asset manager.
template <AssetType T>
T AssetOrKey<T>::Get(const AssetManager& assets) const {
	return std::visit(
		[&assets]<typename S>(const S& value) {
			if constexpr (std::is_same_v<S, T>) {
				return value;
			} else if constexpr (std::is_same_v<S, std::size_t>) {
				PTGN_ASSERT(
					assets.Has<T>(value),
					"Asset key must be loaded in the asset manager before retrieval"
				);
				return *assets.Get<T>(value);
			} else {
				static_assert(false, "Incomplete visitor!");
			}
		},
		value_
	);
}

template <AssetType T>
T AssetOrKey<T>::Get(const Scene& scene) const {
	const SceneContext& context{ scene.ctx() };
	return Get(context.asset);
}

template class AssetOrKey<Texture>;
template class AssetOrKey<Audio>;
template class AssetOrKey<Shader>;
template class AssetOrKey<Font>;
template class AssetOrKey<std::reference_wrapper<const json>>;

} // namespace ptgn