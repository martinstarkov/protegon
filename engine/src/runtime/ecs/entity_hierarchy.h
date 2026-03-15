#pragma once

#include <optional>
#include <string_view>
#include <vector>

// TODO: Add tests for entity hierarchy functions.

namespace ptgn {

class Entity;

/// @return The parent most entity, or *this if no parent exists.
[[nodiscard]] Entity GetRootEntity(Entity entity);

/// @return Parent entity of the object. If object has no parent, returns *this.
[[nodiscard]] Entity GetParent(Entity entity);

[[nodiscard]] bool HasParent(Entity entity);

/// @brief If true, the entity's transform will not be affected by its parent's transform.
void IgnoreParentTransform(Entity entity, bool ignore_parent_transform = true);

/// @brief If true, the entity's transform will not be affected by its parent's position.
void IgnoreParentPosition(Entity entity, bool ignore_parent_position = true);

/// @brief If true, the entity's transform will not be affected by its parent's rotation.
void IgnoreParentRotation(Entity entity, bool ignore_parent_rotation = true);

/// @brief If true, the entity's transform will not be affected by its parent's scale.
void IgnoreParentScale(Entity entity, bool ignore_parent_scale = true);

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

/// @return True if the entity has the given child, false otherwise.
[[nodiscard]] bool HasChild(Entity entity, Entity child);
[[nodiscard]] bool HasChild(Entity entity, std::string_view name);

/// @return Child entity with the given name. Assertion called if entity does not exist
[[nodiscard]] Entity GetChild(Entity entity, std::string_view name);

[[nodiscard]] bool HasChildren(Entity entity);

/// @return All direct children of the object.
[[nodiscard]] const std::vector<Entity>& GetChildren(Entity entity);

namespace impl {

void AddChildImpl(Entity entity, Entity child, std::optional<std::string_view> name);

void SetParentImpl(Entity entity, Entity parent);

void RemoveParentImpl(Entity entity);

} // namespace impl

} // namespace ptgn