#include "runtime/asset/asset_handle.h"

#include <utility>

#include "core/assert.h"
#include "ecs/ecs.h"

namespace ptgn::impl {

Asset::Asset(ecs::Entity entity) : entity_{ entity } {
	PTGN_ASSERT(entity_);
	entity_.Add<impl::RefCount>();
	AddRef();
}

Asset::Asset(const Asset& other) : entity_{ other.entity_ } {
	AddRef();
}

Asset::Asset(Asset&& other) noexcept : entity_{ std::exchange(other.entity_, {}) } {}

Asset& Asset::operator=(const Asset& other) {
	if (this != &other) {
		Release();
		entity_ = other.entity_;
		AddRef();
	}
	return *this;
}

Asset& Asset::operator=(Asset&& other) noexcept {
	if (this != &other) {
		Release();
		entity_ = std::exchange(other.entity_, {});
	}
	return *this;
}

Asset::~Asset() {
	Release();
}

void Asset::AddRef() {
	if (!Valid()) {
		return;
	}
	auto& rc = entity_.Get<RefCount>();
	rc.value++;
}

void Asset::Release() {
	if (!Valid()) {
		return;
	}

	auto& rc = entity_.Get<RefCount>();

	if (--rc.value == 0 && !entity_.Has<PersistentTag>()) {
		entity_.Destroy();
	}
}

bool Asset::Valid() const {
	return entity_.Has<RefCount>();
}

} // namespace ptgn::impl