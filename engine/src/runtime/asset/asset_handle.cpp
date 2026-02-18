#include "runtime/asset/asset_handle.h"

#include "core/assert.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

namespace impl {

template <typename Derived>
RefCountedAsset<Derived>::RefCountedAsset(Entity e, AssetManager* asset_manager) :
	entity_{ e }, asset_manager_{ asset_manager } {
	PTGN_ASSERT(asset_manager_ != nullptr);
	AddRef();
}

template <typename Derived>
RefCountedAsset<Derived>::RefCountedAsset(const RefCountedAsset& other) :
	entity_{ other.entity_ }, asset_manager_{ other.asset_manager_ } {
	AddRef();
}

template <typename Derived>
RefCountedAsset<Derived>::RefCountedAsset(RefCountedAsset&& other) noexcept :
	entity_{ other.entity_ }, asset_manager_{ other.asset_manager_ } {
	other.entity_		 = {};
	other.asset_manager_ = nullptr;
}

template <typename Derived>
RefCountedAsset<Derived>& RefCountedAsset<Derived>::operator=(const RefCountedAsset& other) {
	if (this != &other) {
		Release();
		entity_		   = other.entity_;
		asset_manager_ = other.asset_manager_;
		AddRef();
	}
	return *this;
}

template <typename Derived>
RefCountedAsset<Derived>& RefCountedAsset<Derived>::operator=(RefCountedAsset&& other) noexcept {
	if (this != &other) {
		Release();
		entity_				 = other.entity_;
		asset_manager_		 = other.asset_manager_;
		other.entity_		 = {};
		other.asset_manager_ = nullptr;
	}
	return *this;
}

template <typename Derived>
RefCountedAsset<Derived>::~RefCountedAsset() {
	Release();
}

template <typename Derived>
void RefCountedAsset<Derived>::AddRef() {
	if (!Valid()) {
		return;
	}
	entity_.Get<RefCount>().value++;
}

template <typename Derived>
void RefCountedAsset<Derived>::Release() {
	if (!Valid()) {
		return;
	}

	auto& rc = entity_.Get<RefCount>();

	if (--rc.value == 0 && !entity_.Has<PersistentTag>()) {
		static_cast<Derived*>(this)->Destroy();
	}
}

template <typename Derived>
bool RefCountedAsset<Derived>::Valid() const {
	return entity_.Has<RefCount>();
}

} // namespace impl

} // namespace ptgn