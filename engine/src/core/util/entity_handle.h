#pragma once

#include <cstdint>
#include <ostream>
#include <utility>

#include "core/assert.h"
#include "ecs/ecs.h"

namespace ptgn {

namespace impl {

/// @brief Component for reference counting the EntityHandle.
struct RefCount {
	std::uint32_t value{ 0 };
};

} // namespace impl

/// @brief An optionally reference counted object that owns an entity.
/// This creates a uniform interface for user-owned entities and manager-owned entities.
class EntityHandle {
public:
	EntityHandle() = default;

	/// @param entity Entity to take control of.
	/// @param persistent If true, entity is not reference counted.
	explicit EntityHandle(ecs::Entity entity, bool persistent) : entity_{ entity } {
		PTGN_ASSERT(entity_);
		if (!persistent) {
			entity_.Add<impl::RefCount>();
		}
		AddRef();
	}

	EntityHandle(const EntityHandle& other) : entity_{ other.entity_ } {
		AddRef();
	}

	EntityHandle(EntityHandle&& other) noexcept : entity_{ std::exchange(other.entity_, {}) } {}

	EntityHandle& operator=(const EntityHandle& other) {
		if (this != &other) {
			Release();
			entity_ = other.entity_;
			AddRef();
		}
		return *this;
	}

	EntityHandle& operator=(EntityHandle&& other) noexcept {
		if (this != &other) {
			Release();
			entity_ = std::exchange(other.entity_, {});
		}
		return *this;
	}

	bool operator==(const EntityHandle&) const = default;

	~EntityHandle() noexcept {
		Release();
	}

	explicit operator bool() const {
		return entity_.operator bool();
	}

	friend std::ostream& operator<<(std::ostream& os, const EntityHandle& e) {
		os << e.entity_.GetId();
		os << "-";
		os << e.entity_.GetVersion();
		return os;
	}

	ecs::Entity GetEntity() const {
		return entity_;
	}

private:
	ecs::Entity entity_;

	void AddRef() {
		if (!HasRefCount()) {
			return;
		}
		auto& rc = entity_.Get<impl::RefCount>();
		rc.value++;
	}

	void Release() {
		if (!HasRefCount()) {
			return;
		}

		auto& rc = entity_.Get<impl::RefCount>();

		if (--rc.value == 0) {
			entity_.Destroy();
		}
	}

	[[nodiscard]] bool HasRefCount() const {
		return entity_.Has<impl::RefCount>();
	}
};

} // namespace ptgn