#pragma once

#include <string_view>
#include <vector>

// TODO: Add tests for entity hierarchy functions.

namespace ptgn {

class Entity;

// @return The parent most entity, or *this if no parent exists.
[[nodiscard]] Entity GetRootEntity(Entity entity);

// @return Parent entity of the object. If object has no parent, returns *this.
[[nodiscard]] Entity GetParent(Entity entity);

[[nodiscard]] bool HasParent(Entity entity);

void IgnoreParentTransform(Entity entity, bool ignore_parent_transform);

void SetParent(Entity entity, Entity parent, bool ignore_parent_transform = false);

void RemoveParent(Entity entity);

void ClearChildren(Entity entity);

void AddChild(Entity entity, Entity child, std::string_view name = {});

void RemoveChild(Entity entity, Entity child);
void RemoveChild(Entity entity, std::string_view name);

// @return True if the entity has the given child, false otherwise.
[[nodiscard]] bool HasChild(Entity entity, Entity child);
[[nodiscard]] bool HasChild(Entity entity, std::string_view name);

// @return Child entity with the given name. Assertion called if entity does not exist
[[nodiscard]] Entity GetChild(Entity entity, std::string_view name);

[[nodiscard]] bool HasChildren(Entity entity);

// @return All direct children of the object.
[[nodiscard]] const std::vector<Entity>& GetChildren(Entity entity);

namespace impl {

void AddChildImpl(Entity entity, Entity child, std::string_view name);

void SetParentImpl(Entity entity, Entity parent);

void RemoveParentImpl(Entity entity);

} // namespace impl

} // namespace ptgn