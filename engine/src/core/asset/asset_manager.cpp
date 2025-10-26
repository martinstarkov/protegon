#include "core/resource/resource_manager.h"

#include "audio/audio.h"
#include "core/ecs/components/generic.h"
#include "core/util/file.h"
#include "debug/runtime/assert.h"
#include "renderer/materials/texture.h"
#include "renderer/text/font.h"
#include "serialization/json/json.h"
#include "serialization/json/json_manager.h"

namespace ptgn {

template <typename Derived, typename HandleType, typename ItemType>
void ResourceManager<Derived, HandleType, ItemType>::LoadList(const path& json_filepath) {
	auto resources = ptgn::LoadJson(json_filepath);
	LoadJson(resources);
}

template <typename Derived, typename HandleType, typename ItemType>
void ResourceManager<Derived, HandleType, ItemType>::UnloadList(const path& json_filepath) {
	auto resources = ptgn::LoadJson(json_filepath);
	UnloadJson(resources);
}

template <typename Derived, typename HandleType, typename ItemType>
void ResourceManager<Derived, HandleType, ItemType>::LoadJson(const json& resources) {
	for (auto it = resources.begin(); it != resources.end(); ++it) {
		const std::string& resource_key = it.key();
		const auto& resource_path		= it.value();
		if (!resource_path.is_string()) {
			PTGN_ERROR("Failed to load resource: ", resource_path.dump(4));
		}
		Load(resource_key, resource_path);
	}
}

template <typename Derived, typename HandleType, typename ItemType>
void ResourceManager<Derived, HandleType, ItemType>::UnloadJson(const json& resources) {
	for (auto it = resources.begin(); it != resources.end(); ++it) {
		const std::string& resource_key = it.key();
		const auto& resource_path		= it.value();
		if (!resource_path.is_string()) {
			PTGN_ERROR("Failed to load resource: ", resource_path.dump(4));
		}
		Unload(resource_key);
	}
}

template <typename Derived, typename HandleType, typename ItemType>
void ResourceManager<Derived, HandleType, ItemType>::Clear() {
	resources_.clear();
}

template <typename Derived, typename HandleType, typename ItemType>
void ResourceManager<Derived, HandleType, ItemType>::Load(
	const HandleType& key, const path& filepath
) {
	/*static_assert(
		impl::has_static_load_from_file<Derived>::value,
		"Derived resource manager must implement static LoadFromFile(const path&)"
	);*/

	auto [it, inserted] = resources_.try_emplace(key);
	if (inserted) {
		it->second.key		= key;
		it->second.filepath = filepath;
		it->second.resource = Derived::LoadFromFile(filepath); // CRTP call
	}
}

template <typename Derived, typename HandleType, typename ItemType>
void ResourceManager<Derived, HandleType, ItemType>::Unload(const HandleType& key) {
	resources_.erase(key);
}

template <typename Derived, typename HandleType, typename ItemType>
bool ResourceManager<Derived, HandleType, ItemType>::Has(const HandleType& key) const {
	return resources_.find(key) != resources_.end();
}

template <typename Derived, typename HandleType, typename ItemType>
const typename ResourceManager<Derived, HandleType, ItemType>::ResourceInfo&
ResourceManager<Derived, HandleType, ItemType>::GetResourceInfo(const HandleType& key) const {
	PTGN_ASSERT(Has(key), "Cannot get resource which has not been loaded: ", key.GetKey());
	return resources_.find(key)->second;
}

template <typename Derived, typename HandleType, typename ItemType>
const ItemType& ResourceManager<Derived, HandleType, ItemType>::Get(const HandleType& key) const {
	return GetResourceInfo(key).resource;
}

template <typename Derived, typename HandleType, typename ItemType>
const path& ResourceManager<Derived, HandleType, ItemType>::GetPath(const HandleType& key) const {
	return GetResourceInfo(key).filepath;
}

template <typename Derived, typename HandleType, typename ItemType>
void to_json(json& j, const ResourceManager<Derived, HandleType, ItemType>& manager) {
	for (const auto& pair : manager.resources_) {
		// This due to warning: captured structured bindings are a C++20 extension.
		const auto& resource = pair.second;

		const auto& key = resource.key.GetKey();

		/*
		// TODO: Currently the FontManager uses the "" key to indicate the default font. Consider
		readding this back if that ever changes. PTGN_ASSERT( !key.empty(), "Cannot serialize a
		resource without a key: ", resource.filepath.string()
		);
		*/

		if (resource.filepath.empty()) {
			// Do not serialize resouce loaded from binaries (e.g. fonts) or other methods which do
			// not provide a filepath.
			continue;
		}

		j[key] = resource.filepath;
	}
}

template <typename Derived, typename HandleType, typename ItemType>
void from_json(const json& j, ResourceManager<Derived, HandleType, ItemType>& manager) {
	manager.LoadJson(j);
}

template class ResourceManager<impl::TextureManager, TextureHandle, impl::Texture>;
template class ResourceManager<impl::SoundManager, ResourceHandle, impl::Sound>;
template class ResourceManager<impl::MusicManager, ResourceHandle, impl::Music>;
template class ResourceManager<impl::JsonManager, ResourceHandle, json>;
template class ResourceManager<impl::FontManager, ResourceHandle, impl::Font>;

template void
to_json(json&, const ResourceManager<impl::TextureManager, TextureHandle, impl::Texture>&);
template void
from_json(const json&, ResourceManager<impl::TextureManager, TextureHandle, impl::Texture>&);

template void
to_json(json&, const ResourceManager<impl::SoundManager, ResourceHandle, impl::Sound>&);
template void
from_json(const json&, ResourceManager<impl::SoundManager, ResourceHandle, impl::Sound>&);

template void
to_json(json&, const ResourceManager<impl::MusicManager, ResourceHandle, impl::Music>&);
template void
from_json(const json&, ResourceManager<impl::MusicManager, ResourceHandle, impl::Music>&);

template void to_json(json&, const ResourceManager<impl::JsonManager, ResourceHandle, json>&);
template void from_json(const json&, ResourceManager<impl::JsonManager, ResourceHandle, json>&);

template void to_json(json&, const ResourceManager<impl::FontManager, ResourceHandle, impl::Font>&);
template void
from_json(const json&, ResourceManager<impl::FontManager, ResourceHandle, impl::Font>&);

} // namespace ptgn