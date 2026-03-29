#pragma once

#include <cstdint>
#include <ostream>

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
	explicit EntityHandle(ecs::Entity entity, bool persistent);

	EntityHandle(const EntityHandle& other);

	EntityHandle(EntityHandle&& other) noexcept;

	EntityHandle& operator=(const EntityHandle& other);

	EntityHandle& operator=(EntityHandle&& other) noexcept;

	bool operator==(const EntityHandle&) const = default;

	~EntityHandle();

	explicit operator bool() const;

	friend std::ostream& operator<<(std::ostream& os, const EntityHandle& e) {
		os << e.entity_.GetId();
		os << "-";
		os << e.entity_.GetVersion();
		return os;
	}

	ecs::Entity GetEntity() const;

private:
	ecs::Entity entity_;

	void AddRef();

	void Release();

	bool HasRefCount() const;
};

} // namespace ptgn