#include "core/entity.h"

#include <memory>
#include <unordered_set>
#include <utility>

#include "common/assert.h"
#include "common/type_info.h"
#include "components/common.h"
#include "components/drawable.h"
#include "components/input.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "components/uuid.h"
#include "core/game.h"
#include "core/manager.h"
#include "ecs/ecs.h"
#include "events/event_handler.h"
#include "math/vector2.h"
#include "nlohmann/json.hpp"
#include "rendering/api/origin.h"
#include "scene/scene.h"
#include "scene/scene_key.h"
#include "scene/scene_manager.h"
#include "serialization/component_registry.h"
#include "serialization/json.h"
#include "serialization/json_archiver.h"

namespace ptgn {

Entity::Entity(Scene& scene) : Entity{ scene.CreateEntity() } {}

Entity::Entity(const Parent& entity) : Parent{ entity } {}

void Entity::Clear() const {
	Parent::Clear();
}

bool Entity::IsAlive() const {
	return Parent::IsAlive();
}

Entity& Entity::Destroy() {
	game.event.UnsubscribeAll(*this);
	Parent::Destroy();
	return *this;
}

Manager& Entity::GetManager() {
	return static_cast<Manager&>(Parent::GetManager());
}

const Manager& Entity::GetManager() const {
	return static_cast<const Manager&>(Parent::GetManager());
}

const Scene& Entity::GetScene() const {
	PTGN_ASSERT(Has<impl::SceneKey>());
	const auto& scene_key{ Get<impl::SceneKey>() };
	PTGN_ASSERT(game.scene.HasScene(scene_key));
	return game.scene.Get(scene_key);
}

Scene& Entity::GetScene() {
	return const_cast<Scene&>(std::as_const(*this).GetScene());
}

bool Entity::IsIdenticalTo(const Entity& e) const {
	return Parent::IsIdenticalTo(e);
}

UUID Entity::GetUUID() const {
	PTGN_ASSERT(Has<UUID>(), "Every entity must have a UUID");
	return Get<UUID>();
}

std::size_t Entity::GetHash() const {
	return std::hash<Parent>()(*this);
}

bool Entity::HasDraw() const {
	return Has<IDrawable>();
}

Entity& Entity::RemoveDraw() {
	Remove<IDrawable>();
	return *this;
}

Entity Entity::GetRootEntity() const {
	return HasParent() ? GetParent().GetRootEntity() : *this;
}

Entity Entity::GetParent() const {
	return HasParent() ? Get<impl::Parent>() : *this;
}

bool Entity::HasParent() const {
	return Has<impl::Parent>();
}

void Entity::RemoveParentImpl() {
	Remove<impl::Parent>();
}

void Entity::RemoveParent() {
	if (Has<impl::Parent>()) {
		if (auto& parent{ Get<impl::Parent>() }; parent.Has<impl::Children>()) {
			auto& children{ parent.Get<impl::Children>() };
			children.Remove(*this);
		}
		RemoveParentImpl();
	}
}

void Entity::SetParentImpl(Entity& parent) {
	if (!parent || parent == *this) {
		RemoveParent();
		return;
	}
	if (HasParent()) {
		Get<impl::Parent>() = parent;
	} else {
		Add<impl::Parent>(parent);
	}
}

void Entity::IgnoreParentTransform(bool ignore_parent_transform) {
	AddOrRemove<impl::IgnoreParentTransform>(ignore_parent_transform, ignore_parent_transform);
}

void Entity::SetParent(Entity& parent, bool ignore_parent_transform) {
	IgnoreParentTransform(ignore_parent_transform);
	SetParentImpl(parent);
	if (parent && parent != *this) {
		parent.AddChildImpl(*this);
	}
}

Entity Entity::CreateChild(std::string_view name) {
	auto entity{ GetManager().CreateEntity() };
	AddChild(entity, name);
	return entity;
}

void Entity::SerializeAllImpl(json& j) const {
	JSONArchiver archiver;

	PTGN_ASSERT(manager_ != nullptr);

	const auto& pools{ GetManager().pools_ };

	for (const auto& pool : pools) {
		if (!pool) {
			continue;
		}
		pool->Serialize(archiver, entity_);
	}

	j = archiver.j;
}

void Entity::DeserializeAllImpl(const json& j) {
	JSONArchiver archiver;
	archiver.j = j;

	impl::ComponentRegistry::AddTypes(GetManager());

	auto& manager{ GetManager() };

	for (auto& pool : manager.pools_) {
		if (!pool) {
			continue;
		}
		pool->Deserialize(archiver, manager, entity_);
	}
}

void Entity::AddChildImpl(Entity& child, std::string_view name) {
	PTGN_ASSERT(child, "Cannot add an null entity as a child");
	PTGN_ASSERT(*this != child, "Cannot add an entity as its own child");
	PTGN_ASSERT(
		GetManager() == child.GetManager(), "Cannot set cross manager parent-child relationships"
	);
	auto& children = TryAdd<impl::Children>();
	children.Add(child, name);
}

void Entity::AddChild(Entity& child, std::string_view name) {
	AddChildImpl(child, name);
	child.SetParentImpl(*this);
}

void Entity::ClearChildren() {
	if (!Has<impl::Children>()) {
		return;
	}

	auto& children{ Get<impl::Children>() };
	// Cannot use reference here due to unordered_set const iterator.
	for (Entity child : children.children_) {
		child.Remove<impl::Parent>();
	}
	children.Clear();
}

void Entity::RemoveChild(Entity& child) {
	child.RemoveParent();
}

void Entity::RemoveChild(std::string_view name) {
	if (!Has<impl::Children>()) {
		return;
	}
	const auto& children{ Get<impl::Children>() };
	auto child{ children.Get(name) };
	child.RemoveParent();
}

bool Entity::HasChild(std::string_view name) const {
	if (!Has<impl::Children>()) {
		return false;
	}
	const auto& children{ Get<impl::Children>() };
	return children.Has(name);
}

bool Entity::HasChild(const Entity& child) const {
	if (!Has<impl::Children>()) {
		return false;
	}
	const auto& children{ Get<impl::Children>() };
	return children.Has(child);
}

Entity Entity::GetChild(std::string_view name) const {
	if (!Has<impl::Children>()) {
		return {};
	}
	const auto& children{ Get<impl::Children>() };
	return children.Get(name);
}

std::unordered_set<Entity> Entity::GetChildren() const {
	if (!Has<impl::Children>()) {
		return {};
	}
	const auto& children{ Get<impl::Children>() };
	return children.children_;
}

Entity& Entity::SetEnabled(bool enabled) {
	if (enabled) {
		Add<Enabled>(enabled);
		InvokeScript<&impl::IScript::OnEnable>();
	} else {
		InvokeScript<&impl::IScript::OnDisable>();
		Remove<Enabled>();
	}
	return *this;
}

Entity& Entity::SetInteractive(bool interactive) {
	if (interactive) {
		Add<Interactive>();
		Enable();
	} else {
		Remove<Interactive>();
	}
	return *this;
}

Entity& Entity::Disable() {
	return SetEnabled(false);
}

Entity& Entity::Enable() {
	return SetEnabled(true);
}

bool Entity::IsEnabled() const {
	return GetOrParentOrDefault<Enabled>(false);
}

Entity& Entity::SetTransform(const Transform& transform) {
	if (Has<Transform>()) {
		GetImpl<Transform>() = transform;
	} else {
		Add<Transform>(transform);
	}
	return *this;
}

Transform& Entity::GetTransform() {
	return TryAdd<Transform>();
}

Transform Entity::GetTransform() const {
	if (auto transform{ TryGetImpl<Transform>() }; transform) {
		return *transform;
	}
	return {};
}

Transform Entity::GetAbsoluteTransform() const {
	auto transform{ GetTransform() };
	if (Has<impl::IgnoreParentTransform>() && Get<impl::IgnoreParentTransform>()) {
		return transform;
	}
	return transform.RelativeTo(HasParent() ? GetParent().GetAbsoluteTransform() : Transform{});
}

Transform Entity::GetDrawTransform() const {
	auto offset_transform{ GetOffset(*this) };
	auto transform{ GetAbsoluteTransform() };
	transform = transform.RelativeTo(offset_transform);
	return transform;
}

Entity& Entity::SetDrawOffset(const V2_float& offset) {
	TryAdd<impl::Offsets>().custom.position = offset;
	return *this;
}

Entity& Entity::AddPostFX(Entity post_fx) {
	post_fx.Hide();
	TryAdd<impl::PostFX>().post_fx_.insert(post_fx);
	return *this;
}

Entity& Entity::AddPreFX(Entity pre_fx) {
	pre_fx.Hide();
	TryAdd<impl::PreFX>().pre_fx_.insert(pre_fx);
	return *this;
}

Entity& Entity::SetPosition(const V2_float& position) {
	if (Has<Transform>()) {
		GetImpl<Transform>().position = position;
	} else {
		Add<Transform>(position);
	}
	return *this;
}

V2_float Entity::GetPosition() const {
	return GetTransform().position;
}

V2_float& Entity::GetPosition() {
	return GetTransform().position;
}

V2_float Entity::GetAbsolutePosition() const {
	return GetAbsoluteTransform().position;
}

Entity& Entity::SetRotation(float rotation) {
	if (Has<Transform>()) {
		GetImpl<Transform>().rotation = rotation;
	} else {
		Add<Transform>(V2_float{}, rotation);
	}
	return *this;
}

float Entity::GetRotation() const {
	return GetTransform().rotation;
}

float& Entity::GetRotation() {
	return GetTransform().rotation;
}

float Entity::GetAbsoluteRotation() const {
	return GetAbsoluteTransform().rotation;
}

Entity& Entity::SetScale(float scale) {
	return SetScale(V2_float{ scale });
}

Entity& Entity::SetScale(const V2_float& scale) {
	if (Has<Transform>()) {
		GetImpl<Transform>().scale = scale;
	} else {
		Add<Transform>(V2_float{}, 0.0f, scale);
	}
	return *this;
}

V2_float Entity::GetScale() const {
	return GetTransform().scale;
}

V2_float& Entity::GetScale() {
	return GetTransform().scale;
}

V2_float Entity::GetAbsoluteScale() const {
	return GetAbsoluteTransform().scale;
}

Entity& Entity::SetOrigin(Origin origin) {
	if (Has<Origin>()) {
		Get<Origin>() = origin;
	} else {
		Add<Origin>(origin);
	}
	return *this;
}

Origin Entity::GetOrigin() const {
	return GetOrDefault<Origin>(Origin::Center);
}

Entity& Entity::SetVisible(bool visible) {
	if (visible) {
		Add<Visible>(visible);
		InvokeScript<&impl::IScript::OnShow>();
	} else {
		InvokeScript<&impl::IScript::OnHide>();
		Remove<Visible>();
	}
	return *this;
}

Entity& Entity::Show() {
	return SetVisible(true);
}

Entity& Entity::Hide() {
	return SetVisible(false);
}

bool Entity::IsVisible() const {
	return GetOrParentOrDefault<Visible>(false);
}

Entity& Entity::SetDepth(const Depth& depth) {
	if (Has<Depth>()) {
		GetImpl<Depth>() = depth;
	} else {
		Add<Depth>(depth);
	}
	return *this;
}

Depth Entity::GetDepth() const {
	Depth parent_depth{};
	if (HasParent()) {
		auto parent{ GetParent() };
		if (parent != *this && parent.Has<Depth>()) {
			parent_depth = parent.GetDepth();
		}
	}
	return parent_depth + GetOrDefault<Depth>();
}

Entity& Entity::SetBlendMode(BlendMode blend_mode) {
	if (Has<BlendMode>()) {
		Get<BlendMode>() = blend_mode;
	} else {
		Add<BlendMode>(blend_mode);
	}
	return *this;
}

BlendMode Entity::GetBlendMode() const {
	return GetOrDefault<BlendMode>(BlendMode::Blend);
}

Entity& Entity::SetTint(const Color& color) {
	AddOrRemove<Tint>(color != Tint{}, color);
	return *this;
}

Color Entity::GetTint() const {
	return GetOrDefault<Tint>();
}

void Scripts::Update(Scene& scene, float dt) {
	for (auto [entity, scripts] : scene.EntitiesWith<Scripts>()) {
		auto s = scripts;
		s.Invoke<&impl::IScript::OnUpdate>(dt);
	}

	scene.Refresh();
}

namespace impl {

Parent::Parent(const Entity& entity) : Entity{ entity } {}

void Children::Clear() {
	children_.clear();
}

void Children::Add(Entity& child, std::string_view name) {
	if (!name.empty()) {
		child.Add<ChildKey>(name);
	}
	children_.emplace(child);
}

void Children::Remove(const Entity& child) {
	children_.erase(child);
	// TODO: Consider adding a use count to ChildKey so it can be removed once an entity is no
	// longer a child of any other entity.
}

void Children::Remove(std::string_view name) {
	ChildKey k{ name };
	for (auto it = children_.begin(); it != children_.end();) {
		if (it->Has<ChildKey>() && it->Get<ChildKey>() == k) {
			it = children_.erase(it);
		} else {
			++it;
		}
	}
}

[[nodiscard]] Entity Children::Get(std::string_view name) const {
	ChildKey k{ name };
	for (const auto& entity : children_) {
		if (entity.Has<ChildKey>() && entity.Get<ChildKey>() == k) {
			return entity;
		}
	}
	return {};
}

[[nodiscard]] bool Children::IsEmpty() const {
	return children_.empty();
}

[[nodiscard]] bool Children::Has(const Entity& child) const {
	return children_.count(child) > 0;
}

[[nodiscard]] bool Children::Has(std::string_view name) const {
	ChildKey k{ name };
	for (const auto& entity : children_) {
		if (entity.Has<ChildKey>() && entity.Get<ChildKey>() == k) {
			return true;
		}
	}
	return false;
}

} // namespace impl

void to_json(json& j, const Entity& entity) {
	j = json{};

	if (!entity) {
		return;
	}

	constexpr auto uuid_name{ type_name_without_namespaces<UUID>() };

	j[uuid_name] = entity.GetUUID();

	if (entity.Has<impl::SceneKey>()) {
		constexpr auto scene_key_name{ type_name_without_namespaces<impl::SceneKey>() };
		j[scene_key_name] = entity.Get<impl::SceneKey>();
	}
}

void from_json(const json& j, Entity& entity) {
	PTGN_ASSERT(entity, "Cannot read JSON into null entity");

	constexpr auto uuid_name{ type_name_without_namespaces<UUID>() };

	PTGN_ASSERT(
		j.contains(uuid_name), "Cannot create entity from JSON which does not contain a UUID"
	);

	UUID uuid;

	j[uuid_name].get_to(uuid);

	const auto& manager{ entity.GetManager() };

	auto found_entity{ manager.GetEntityByUUID(uuid) };

	PTGN_ASSERT(!found_entity || (found_entity && found_entity == entity));

	PTGN_ASSERT(entity, "Failed to find entity with UUID: ", uuid);

	constexpr auto scene_key_name{ type_name_without_namespaces<impl::SceneKey>() };

	if (j.contains(scene_key_name)) {
		impl::SceneKey scene_key;
		j[scene_key_name].get_to(scene_key);
		entity.Add<impl::SceneKey>(scene_key);
	}
}

} // namespace ptgn