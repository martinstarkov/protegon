#include "core/util/entity_handle.h"

#include <utility>

#include "core/assert.h"
#include "ecs/ecs.h"

namespace ptgn {

EntityHandle::EntityHandle(ecs::Entity entity, bool persistent) : entity_{ entity } {
	PTGN_ASSERT(entity_);
	if (!persistent) {
		entity_.Add<impl::RefCount>();
	}
	AddRef();
}

EntityHandle::EntityHandle(const EntityHandle& other) : entity_{ other.entity_ } {
	AddRef();
}

EntityHandle::EntityHandle(EntityHandle&& other) noexcept :
	entity_{ std::exchange(other.entity_, {}) } {}

EntityHandle& EntityHandle::operator=(const EntityHandle& other) {
	if (this != &other) {
		Release();
		entity_ = other.entity_;
		AddRef();
	}
	return *this;
}

EntityHandle& EntityHandle::operator=(EntityHandle&& other) noexcept {
	if (this != &other) {
		Release();
		entity_ = std::exchange(other.entity_, {});
	}
	return *this;
}

EntityHandle::~EntityHandle() {
	Release();
}

EntityHandle::operator bool() const {
	return entity_.operator bool();
}

ecs::Entity EntityHandle::GetEntity() const {
	return entity_;
}

void EntityHandle::AddRef() {
	if (!HasRefCount()) {
		return;
	}
	auto& rc = entity_.Get<impl::RefCount>();
	rc.value++;
}

void EntityHandle::Release() {
	if (!HasRefCount()) {
		return;
	}

	auto& rc = entity_.Get<impl::RefCount>();

	if (--rc.value == 0) {
		entity_.Destroy();
	}
}

bool EntityHandle::HasRefCount() const {
	return entity_.Has<impl::RefCount>();
}

} // namespace ptgn