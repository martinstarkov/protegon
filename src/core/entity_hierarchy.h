#pragma once

#include <string_view>
#include <vector>

// TODO: Add tests for entity hierarchy functions.

namespace ptgn {

class Entity;

// @return The parent most entity, or *this if no parent exists.
[[nodiscard]] const Entity& GetRootEntity(const Entity& entity);
[[nodiscard]] Entity& GetRootEntity(Entity& entity);

// @return Parent entity of the object. If object has no parent, returns *this.
[[nodiscard]] const Entity& GetParent(const Entity& entity);
[[nodiscard]] Entity& GetParent(Entity& entity);

[[nodiscard]] bool HasParent(const Entity& entity);

void IgnoreParentTransform(Entity& entity, bool ignore_parent_transform);

void SetParent(Entity& entity, Entity& parent, bool ignore_parent_transform = false);

void RemoveParent(Entity& entity);

void ClearChildren(Entity& entity);

void AddChild(Entity& entity, Entity& child, std::string_view name = {});

void RemoveChild(Entity& entity, Entity& child);
void RemoveChild(Entity& entity, std::string_view name);

// @return True if the entity has the given child, false otherwise.
[[nodiscard]] bool HasChild(const Entity& entity, const Entity& child);
[[nodiscard]] bool HasChild(const Entity& entity, std::string_view name);

// @return Child entity with the given name, or null entity is no such child exists.
[[nodiscard]] Entity GetChild(const Entity& entity, std::string_view name);

// @return All childs entities tied to the object
[[nodiscard]] const std::vector<Entity>& GetChildren(const Entity& entity);

namespace impl {

void AddChildImpl(Entity& entity, Entity& child, std::string_view name = {});

void SetParentImpl(Entity& entity, Entity& parent);

void RemoveParentImpl(Entity& entity);

} // namespace impl

} // namespace ptgn