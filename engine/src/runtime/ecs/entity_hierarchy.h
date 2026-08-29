#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "core/log.h"
#include "core/util/concepts.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Scene;

inline constexpr std::size_t kMaxParentDepth{ 256 };

/// @return The parent most entity, or *this if no parent exists.
Entity GetRootEntity(Entity entity);

/// @return Parent entity of the object. If object has no parent, returns *this.
Entity GetParent(Entity entity);

[[nodiscard]] bool HasParent(Entity entity);

/// @brief Sets the parent entity for the specified entity.
/// @param entity The entity whose parent is being set.
/// @param parent The entity to set as the parent.
/// @param ignore_parent_transform If true, the entity's transform will not be affected by the
/// parent's transform.
void SetParent(Entity entity, Entity parent, bool ignore_parent_transform = false);

void RemoveParent(Entity entity);

void ClearChildren(Entity entity);

/// @brief Adds a child entity to a parent entity.
/// @param entity The parent entity to which the child will be added.
/// @param child The child entity to be added to the parent.
/// @param name An optional name to associate with the child entity.
void AddChild(Entity entity, Entity child, std::optional<std::string_view> name = {});

void RemoveChild(Entity entity, Entity child);
void RemoveChild(Entity entity, std::string_view name);

/// @brief Moves an existing direct child to a new index in the parent's ordered child list.
void MoveChild(Entity entity, Entity child, std::size_t index);

/// @return True if the entity has the given child, false otherwise.
[[nodiscard]] bool HasChild(Entity entity, Entity child);
[[nodiscard]] bool HasChild(Entity entity, std::string_view name);

/// @return Child entity with the given name. Assertion called if entity does not exist
Entity GetChild(Entity entity, std::string_view name);

/// @return True if the entity has any children, false otherwise.
[[nodiscard]] bool HasChildren(Entity entity);

/// @return All direct children of the object.
const std::vector<Entity>& GetChildren(Entity entity);

/// @brief Walks the parent hierarchy of the entity, calling the provided function for each parent
/// entity until the function returns false or the maximum parent depth is reached. The function
/// should return true to continue walking up the hierarchy, or false to stop.
/// @param entity The entity whose parent hierarchy will be walked.
/// @param should_ignore_parent A function that takes a parent entity as an argument and returns
/// true if the parent should be ignored.
template <InvocableR<bool, Entity> ShouldIgnoreParentFunc, InvocableR<bool, Entity> Func>
void ForEachParent(Entity entity, ShouldIgnoreParentFunc&& should_ignore_parent, Func&& func) {
	for (auto i{ 0uz }; i < kMaxParentDepth; ++i) {
		if (should_ignore_parent(entity)) {
			return;
		}

		if (!HasParent(entity)) {
			return;
		}

		auto parent{ GetParent(entity) };

		if (parent == entity) {
			return;
		}

		if (!func(parent)) {
			return;
		}

		entity = parent;
	}

	PTGN_ERROR("Maximum parent depth exceeded while walking entity parents");
}

namespace impl {

struct Orphan {};

void OrphanChildren(Scene& scene);

void ClearDeadChildren(Scene& scene);

void OrphanChild(Entity entity);

} // namespace impl

} // namespace ptgn
