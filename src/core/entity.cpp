#include "core/entity.h"

#include <memory>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "common/type_info.h"
#include "components/common.h"
#include "components/drawable.h"
#include "components/input.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "components/uuid.h"
#include "core/entity_hierarchy.h"
#include "core/game.h"
#include "core/manager.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "nlohmann/json.hpp"
#include "renderer/api/origin.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_key.h"
#include "scene/scene_manager.h"
#include "serialization/component_registry.h"
#include "serialization/json.h"
#include "serialization/json_archiver.h"
#include "utility/span.h"

namespace ptgn {

Entity::Entity(Scene& scene) : Entity{ scene.CreateEntity() } {}

ecs::impl::Index Entity::GetId() const {
	return Parent::GetId();
}

Entity::Entity(const Parent& entity) : Parent{ entity } {}

void Entity::Clear() const {
	Parent::Clear();
}

bool Entity::IsAlive() const {
	return Parent::IsAlive();
}

Entity& Entity::Destroy(bool orphan_children) {
	if (*this == Entity{}) {
		return *this;
	}

	if (!orphan_children) {
		auto children{ GetChildren(*this) };
		for (Entity child : children) {
			child.Destroy();
		}
	}

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

RenderTarget GetParentRenderTarget(const Entity& entity) {
	if (entity.Has<RenderTarget>()) {
		return entity.Get<RenderTarget>();
	}
	if (HasParent(entity)) {
		return GetParentRenderTarget(GetParent(entity));
	}
	return entity;
}

const Camera& Entity::GetCamera() const {
	Camera* camera{ nullptr };
	if (Has<Camera>()) {
		camera = &Get<Camera>();
	}
	if (camera && *camera) {
		return *camera;
	}
	if (Has<RenderTarget>()) {
		if (auto& rt{ Get<RenderTarget>() }; rt.Has<Camera>()) {
			camera = &rt.Get<Camera>();
		}
	}
	if (camera && *camera) {
		return *camera;
	}
	if (auto rt{ GetParentRenderTarget(*this) }; rt != *this && rt && rt.Has<Camera>()) {
		camera = &rt.Get<Camera>();
	}
	if (camera && *camera) {
		return *camera;
	}
	return GetScene().camera.primary;
}

Camera& Entity::GetCamera() {
	return const_cast<Camera&>(std::as_const(*this).GetCamera());
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

bool Entity::WasCreatedBefore(const Entity& other) const {
	PTGN_ASSERT(other != *this, "Cannot check if an entity was created before itself");
	auto version{ Parent::GetVersion() };
	auto other_version{ other.Parent::GetVersion() };
	if (version != other_version) {
		return version < other_version;
	}
	return Parent::GetId() < other.Parent::GetId();
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

void Entity::ClearInteractables() {
	if (!Has<Interactive>()) {
		return;
	}
	auto& interactive{ GetImpl<Interactive>() };
	// Clear owned entities.
	interactive.Clear();
}

void Entity::SetInteractiveWasInside(bool value) {
	PTGN_ASSERT(IsInteractive());
	auto& interactive{ GetImpl<Interactive>() };
	interactive.was_inside = value;
}

void Entity::SetInteractiveIsInside(bool value) {
	PTGN_ASSERT(IsInteractive());
	auto& interactive{ GetImpl<Interactive>() };
	interactive.is_inside = value;
}

bool Entity::InteractiveWasInside() const {
	PTGN_ASSERT(IsInteractive());
	const auto& interactive{ GetImpl<Interactive>() };
	return interactive.was_inside;
}

bool Entity::InteractiveIsInside() const {
	PTGN_ASSERT(IsInteractive());
	const auto& interactive{ GetImpl<Interactive>() };
	return interactive.is_inside;
}

Entity& Entity::SetInteractive(bool interactive) {
	if (interactive) {
		Add<Interactive>();
		Enable();
	} else {
		ClearInteractables();
		Remove<Interactive>();
	}
	return *this;
}

bool Entity::IsInteractive() const {
	return Has<Interactive>();
}

Entity& Entity::SetInteractable(Entity shape, bool set_parent) {
	ClearInteractables();
	AddInteractable(shape, set_parent);
	return *this;
}

Entity& Entity::AddInteractable(Entity shape, bool set_parent) {
	if (set_parent) {
		SetParent(shape, *this);
	}
	SetInteractive();
	auto& shapes{ GetImpl<Interactive>().shapes };
	PTGN_ASSERT(
		!VectorContains(shapes, shape),
		"Cannot add the same interactable to an entity more than once"
	);
	shapes.emplace_back(shape);
	return *this;
}

Entity& Entity::RemoveInteractable(Entity shape) {
	if (!IsInteractive()) {
		return *this;
	}
	auto& shapes{ GetImpl<Interactive>().shapes };
	VectorErase(shapes, shape);
	return *this;
}

bool Entity::HasInteractable(Entity shape) const {
	if (!IsInteractive()) {
		return false;
	}
	const auto& shapes{ GetImpl<Interactive>().shapes };
	return VectorContains(shapes, shape);
}

std::vector<Entity> Entity::GetInteractables() const {
	if (!IsInteractive()) {
		return {};
	}
	const auto& shapes{ GetImpl<Interactive>().shapes };
	return shapes;
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

Entity& Entity::SetDrawOffset(const V2_float& offset) {
	TryAdd<impl::Offsets>().custom.position = offset;
	return *this;
}

Entity& Entity::AddPostFX(Entity post_fx) {
	post_fx.Hide();
	auto& post_fx_list{ TryAdd<impl::PostFX>().post_fx_ };
	PTGN_ASSERT(
		!VectorContains(post_fx_list, post_fx),
		"Cannot add the same post fx entity to an entity more than once"
	);
	post_fx_list.emplace_back(post_fx);
	return *this;
}

Entity& Entity::AddPreFX(Entity pre_fx) {
	pre_fx.Hide();
	auto& pre_fx_list{ TryAdd<impl::PreFX>().pre_fx_ };
	PTGN_ASSERT(
		!VectorContains(pre_fx_list, pre_fx),
		"Cannot add the same pre fx entity to an entity more than once"
	);
	pre_fx_list.emplace_back(pre_fx);
	return *this;
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
	return GetOrDefault<Visible>(false);
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
	// TODO: This was causing a bug with the mitosis disk background (rock texture) thing in GMTK
	// 2025. Figure out how to fix relative depths.
	/*Depth parent_depth{};
	if (HasParent()) {
		auto parent{ GetParent() };
		if (parent != *this && parent.Has<Depth>()) {
			parent_depth = parent.GetDepth();
		}
	}
	return parent_depth +*/
	return GetOrDefault<Depth>();
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
	if (color != Tint{}) {
		Add<Tint>(color);
	} else {
		Remove<Tint>();
	}
	return *this;
}

Color Entity::GetTint() const {
	return GetOrDefault<Tint>();
}

void Scripts::Update(Scene& scene, float dt) {
	Invoke<&impl::IScript::OnUpdate>(scene, dt);

	scene.Refresh();
}

std::vector<Entity> Scripts::GetEntities(Scene& scene) {
	return scene.EntitiesWith<Scripts>().GetVector();
}

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