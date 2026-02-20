#pragma once

#include <cstdint>

#include "ecs/ecs.h"

namespace ptgn {

namespace impl {

/// Component for reference counting the EntityHandle.
struct RefCount {
	std::uint32_t value{ 0 };
};

} // namespace impl

/// An optionally reference counted object that owns an entity.
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

protected:
	ecs::Entity entity_;

private:
	void AddRef();

	void Release();

	bool Valid() const;
};

} // namespace ptgn