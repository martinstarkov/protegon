#include "runtime/asset/prefab.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/math/transform.h"
#include "core/util/hash.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/entity_serialization.h"
#include "runtime/ecs/relatives.h"
#include "runtime/ecs/tag.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "serialization/json/json_file.h"

namespace ptgn {

namespace {

[[nodiscard]] bool IsPrefabInstanceComponent(const RegisteredComponent& component) {
	return component.type_id == Hash<PrefabInstance>();
}

void DestroyEntityTree(Entity entity) {
	if (entity) {
		entity.Destroy();
	}
}

Entity InstantiatePrefabEntity(
	Scene& scene,
	const SerializedEntity& definition,
	Entity parent,
	const PrefabKey& prefab_key,
	SerializedEntityPath path,
	bool linked
) {
	Entity entity{ scene.CreateEntity(Tag{ definition.tag }) };
	DeserializeEntity(definition, entity);
	if (linked) {
		entity.Add<PrefabInstance>(PrefabInstance{
			.prefab = prefab_key,
			.entity_path = path,
		});
	}
	if (parent) {
		SetParent(entity, parent);
	}
	for (std::size_t index{}; index < definition.children.size(); ++index) {
		SerializedEntityPath child_path{ path };
		child_path.push_back(index);
		(void)InstantiatePrefabEntity(
			scene,
			definition.children[index],
			entity,
			prefab_key,
			std::move(child_path),
			linked
		);
	}
	return entity;
}

void RemovePrefabLinks(Entity root) {
	if (!root) {
		return;
	}
	if (root.Has<PrefabInstance>()) {
		root.Remove<PrefabInstance>();
	}
	if (HasChildren(root)) {
		for (Entity child : GetChildren(root)) {
			RemovePrefabLinks(child);
		}
	}
}

[[nodiscard]] bool EnsurePrefabResident(
	Scene& scene,
	const PrefabKey& key
) {
	return scene.ctx().asset.EnsurePrefabResident(key);
}

[[nodiscard]] std::optional<Prefab> ResolvePrefab(
	Scene& scene,
	const PrefabKey& key
) {
	if (!EnsurePrefabResident(scene, key)) {
		return std::nullopt;
	}
	auto asset{
		::ptgn::impl::AssetAccessor{
			scene.ctx().asset
		}.Get<Prefab>(key)
	};
	return asset.get();
}

void ApplyPrefabDefinition(
	Entity entity,
	const SerializedEntity& definition,
	const PrefabKey& prefab_key,
	SerializedEntityPath path,
	bool preserve_root_transform
) {
	PTGN_ASSERT(entity);
	const std::optional<Transform> instance_transform{
		preserve_root_transform && entity.Has<Transform>()
			? std::optional<Transform>{ entity.Get<Transform>() }
			: std::nullopt
	};
	// Remove every prefab-owned component before deserializing so deleted prefab components also
	// disappear from linked instances. Root Transform is the one intentional per-instance override.
	for (const auto& component : ComponentRegistry::Components()) {
		if (
			IsPrefabInstanceComponent(component) ||
			!IsPrefabComponentSupported(component) ||
			!component.Has(entity) ||
			(preserve_root_transform && component.type_id == Hash<Transform>())
		) {
			continue;
		}
		(void)component.Remove(entity);
	}
	DeserializeEntity(definition, entity);
	if (instance_transform.has_value()) {
		if (entity.Has<Transform>()) {
			entity.Get<Transform>() = *instance_transform;
		} else {
			entity.Add<Transform>(*instance_transform);
		}
	}
	if (entity.Has<PrefabInstance>()) {
		entity.Get<PrefabInstance>() = PrefabInstance{
			.prefab = prefab_key,
			.entity_path = path,
		};
	} else {
		entity.Add<PrefabInstance>(PrefabInstance{
			.prefab = prefab_key,
			.entity_path = path,
		});
	}
	Scene& scene{ entity.GetScene() };
	std::vector<Entity> existing_children;
	if (HasChildren(entity)) {
		existing_children = GetChildren(entity);
		SortByLocalDepth(existing_children);
	}
	const std::size_t shared_count{
		std::min(existing_children.size(), definition.children.size())
	};
	for (std::size_t index{}; index < shared_count; ++index) {
		SerializedEntityPath child_path{ path };
		child_path.push_back(index);
		ApplyPrefabDefinition(
			existing_children[index],
			definition.children[index],
			prefab_key,
			std::move(child_path),
			false
		);
	}
	for (std::size_t index{ shared_count }; index < definition.children.size(); ++index) {
		SerializedEntityPath child_path{ path };
		child_path.push_back(index);
		(void)InstantiatePrefabEntity(
			scene,
			definition.children[index],
			entity,
			prefab_key,
			std::move(child_path),
			true
		);
	}
	// Destroy from the end so parent/child ordering remains stable while stale nodes are removed.
	for (std::size_t index{ existing_children.size() }; index > definition.children.size(); --index) {
		DestroyEntityTree(existing_children[index - 1]);
	}
}

void StripPrefabInstanceMetadata(SerializedEntity& definition) {
	if (const auto* component{ ComponentRegistry::Find<PrefabInstance>() }) {
		definition.components.erase(component->name);
		std::erase(definition.tags, component->name);
	}
	for (auto& child : definition.children) {
		StripPrefabInstanceMetadata(child);
	}
}

} // namespace
bool IsPrefabComponentSupported(const RegisteredComponent& component) {
	if (
		impl::IsEntityMetadataComponent(component) ||
		IsPrefabInstanceComponent(component)
	) {
		return false;
	}
	if (component.is_empty) {
		return component.default_constructible;
	}
	return component.serializable &&
		   component.deserializable;
}

Prefab CapturePrefab(Entity entity, PrefabKey key, bool include_children) {
	PTGN_ASSERT(entity, "Cannot capture a null entity as a prefab");
	PTGN_ASSERT(!key.value.empty(), "Prefab key cannot be empty");
	SerializedEntity root{
		SerializeEntity(
			entity,
			{
				.include_uuid = false,
				.include_children = include_children,
			}
		)
	};
	StripPrefabInstanceMetadata(root);
	return Prefab{
		.key = std::move(key),
		.root = std::move(root),
	};
}

Entity InstantiatePrefab(
	Scene& scene,
	const Prefab& prefab,
	PrefabInstantiationMode mode
) {
	const bool linked{
		mode == PrefabInstantiationMode::Linked ||
		(mode == PrefabInstantiationMode::Auto && !scene.IsRuntime())
	};
	Entity root{
		InstantiatePrefabEntity(
			scene,
			prefab.root,
			{},
			prefab.key,
			{},
			linked
		)
	};
	scene.Refresh();
	return root;
}

bool IsPrefabInstance(Entity entity) {
	return entity && entity.Has<PrefabInstance>();
}

bool IsPrefabInstanceRoot(Entity entity) {
	if (!IsPrefabInstance(entity)) {
		return false;
	}
	return entity.Get<PrefabInstance>().entity_path.empty();
}

const PrefabInstance* GetPrefabInstance(Entity entity) {
	return entity ? entity.TryGet<PrefabInstance>() : nullptr;
}

Entity GetPrefabInstanceRoot(Entity entity) {
	if (!IsPrefabInstance(entity)) {
		return {};
	}
	const PrefabKey key{ entity.Get<PrefabInstance>().prefab };
	Entity current{ entity };
	while (current) {
		const auto* link{ current.TryGet<PrefabInstance>() };
		if (!link || link->prefab != key) {
			break;
		}
		if (link->entity_path.empty()) {
			return current;
		}
		if (!HasParent(current)) {
			break;
		}
		current = GetParent(current);
	}
	return {};
}

bool SyncPrefabInstance(Entity instance_root, const Prefab& prefab) {
	if (!instance_root || !IsPrefabInstanceRoot(instance_root)) {
		return false;
	}
	ApplyPrefabDefinition(
		instance_root,
		prefab.root,
		prefab.key,
		{},
		true
	);
	instance_root.GetScene().Refresh();
	return true;
}

bool SyncPrefabInstance(Entity instance_root) {
	if (!IsPrefabInstanceRoot(instance_root)) {
		return false;
	}
	Scene& scene{ instance_root.GetScene() };
	const PrefabKey key{ instance_root.Get<PrefabInstance>().prefab };
	const auto prefab{ ResolvePrefab(scene, key) };
	return prefab.has_value()
		? SyncPrefabInstance(instance_root, *prefab)
		: false;
}

std::size_t SyncPrefabInstances(Scene& scene, const PrefabKey& key) {
	const auto prefab{ ResolvePrefab(scene, key) };
	if (!prefab.has_value()) {
		return 0;
	}
	std::vector<UUID> roots;
	for (Entity entity : scene.Entities()) {
		const auto* link{ entity.TryGet<PrefabInstance>() };
		if (
			link &&
			link->prefab == key &&
			link->entity_path.empty() &&
			entity.Has<UUID>()
		) {
			roots.push_back(entity.Get<UUID>());
		}
	}
	std::size_t count{};
	for (UUID uuid : roots) {
		if (Entity root{ scene.GetEntity(uuid) };
			root && SyncPrefabInstance(root, *prefab)) {
			++count;
		}
	}
	return count;
}

std::size_t SyncPrefabInstances(Scene& scene) {
	std::vector<PrefabKey> keys;
	for (Entity entity : scene.Entities()) {
		const auto* link{ entity.TryGet<PrefabInstance>() };
		if (!link || !link->entity_path.empty()) {
			continue;
		}
		if (!std::ranges::contains(keys, link->prefab)) {
			keys.push_back(link->prefab);
		}
	}
	std::size_t count{};
	for (const PrefabKey& key : keys) {
		count += SyncPrefabInstances(scene, key);
	}
	return count;
}

std::size_t RetargetPrefabInstances(
	Scene& scene,
	const PrefabKey& old_key,
	const PrefabKey& new_key
) {
	std::size_t links{};
	for (Entity entity : scene.Entities()) {
		if (auto* link{ entity.TryGet<PrefabInstance>() };
			link && link->prefab == old_key) {
			link->prefab = new_key;
			++links;
		}
	}
	if (links > 0) {
		(void)SyncPrefabInstances(scene, new_key);
	}
	return links;
}

bool BakePrefabInstance(Entity instance_root, const Prefab& prefab) {
	if (!IsPrefabInstanceRoot(instance_root)) {
		return false;
	}
	(void)SyncPrefabInstance(instance_root, prefab);
	RemovePrefabLinks(instance_root);
	instance_root.GetScene().Refresh();
	return true;
}

bool BakePrefabInstance(Entity instance_root) {
	if (!IsPrefabInstanceRoot(instance_root)) {
		return false;
	}
	Scene& scene{ instance_root.GetScene() };
	const PrefabKey key{ instance_root.Get<PrefabInstance>().prefab };
	if (const auto prefab{ ResolvePrefab(scene, key) }) {
		return BakePrefabInstance(instance_root, *prefab);
	}
	// A dangling link should never block runtime. Its last serialized synchronized state is still a
	// valid entity hierarchy, so bake that state even when the source asset has gone missing.
	RemovePrefabLinks(instance_root);
	scene.Refresh();
	return true;
}

std::size_t BakePrefabInstances(Scene& scene) {
	std::vector<UUID> roots;
	for (Entity entity : scene.Entities()) {
		if (
			IsPrefabInstanceRoot(entity) &&
			entity.Has<UUID>()
		) {
			roots.push_back(entity.Get<UUID>());
		}
	}
	std::size_t count{};
	for (UUID uuid : roots) {
		if (Entity root{ scene.GetEntity(uuid) };
			root && BakePrefabInstance(root)) {
			++count;
		}
	}
	return count;
}

Prefab LoadPrefabFile(const path& file_path) {
	Prefab prefab;
	LoadJson(file_path).get_to(prefab);
	return prefab;
}

void SavePrefabFile(const path& file_path, const Prefab& prefab) {
	EnsureDirectory(file_path.parent_path());
	json value = prefab;
	SaveJson(value, file_path);
}

std::string MakePrefabSlug(std::string_view value) {
	std::string output;
	output.reserve(value.size());
	bool separator_pending{ false };
	for (char c : value) {
		const auto character{ static_cast<unsigned char>(c) };
		if (std::isalnum(character)) {
			if (separator_pending && !output.empty()) {
				output.push_back('_');
			}
			separator_pending = false;
			output.push_back(static_cast<char>(std::tolower(character)));
		} else {
			separator_pending = true;
		}
	}
	while (!output.empty() && output.back() == '_') {
		output.pop_back();
	}
	return output.empty() ? "prefab" : output;
}

PrefabKey MakePrefabKey(std::string_view value) {
	if (value.starts_with(kPrefabKeyPrefix)) {
		value.remove_prefix(kPrefabKeyPrefix.size());
	}
	return PrefabKey{
		std::string{ kPrefabKeyPrefix } +
		MakePrefabSlug(value)
	};
}

path GetPrefabSourcePath(const PrefabKey& key) {
	std::string key_value{ key.value };
	if (key_value.starts_with(kPrefabKeyPrefix)) {
		key_value.erase(
			0,
			kPrefabKeyPrefix.size()
		);
	}
	return path{ kPrefabDirectory } /
		   path{
			   MakePrefabSlug(key_value) +
			   std::string{ kPrefabExtension }
		   };
}

path GetPrefabFilePath(
	const path& project_root,
	const PrefabKey& key
) {
	return project_root /
		   GetPrefabSourcePath(key);
}

} // namespace ptgn
