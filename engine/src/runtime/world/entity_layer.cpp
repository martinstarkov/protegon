#include "runtime/world/entity_layer.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ranges>
#include <unordered_set>
#include <utility>

#include "core/assert.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/scene/scene.h"
#include "runtime/world/paint_generator.h"
#include "runtime/world/tilemap.h"

namespace ptgn {

namespace {

constexpr std::string_view kDefaultEntityLayerName{ "Entities" };
constexpr std::string_view kDefaultTileLayerName{ "Tile Layer" };
constexpr std::string_view kDefaultAdditionalEntityLayerName{ "Entity Layer" };

} // namespace

SceneLayers::SceneLayers() {
	Reset();
}

void SceneLayers::Reset() {
	layers_.clear();
	memberships_.clear();
	next_layer_id_ = 1;

	default_entity_layer_ = SceneLayerId{ next_layer_id_++ };
	layers_.push_back(SceneLayer{
		.id = default_entity_layer_,
		.name = std::string{ kDefaultEntityLayerName },
		.kind = SceneLayerKind::Entity,
	});
}

const std::vector<SceneLayer>& SceneLayers::GetLayers() const {
	return layers_;
}

SceneLayerId SceneLayers::GetDefaultEntityLayer() const {
	return default_entity_layer_;
}

SceneLayer* SceneLayers::Find(SceneLayerId id) {
	return const_cast<SceneLayer*>(std::as_const(*this).Find(id));
}

const SceneLayer* SceneLayers::Find(SceneLayerId id) const {
	const auto it{ std::ranges::find(layers_, id, &SceneLayer::id) };
	return it == layers_.end() ? nullptr : std::addressof(*it);
}

SceneLayerId SceneLayers::Create(SceneLayerKind kind, std::string name) {
	const SceneLayerId id{ next_layer_id_++ };
	if (name.empty()) {
		name = kind == SceneLayerKind::Tile
			? std::string{ kDefaultTileLayerName }
			: std::string{ kDefaultAdditionalEntityLayerName };
	}

	layers_.push_back(SceneLayer{
		.id = id,
		.name = std::move(name),
		.kind = kind,
	});
	return id;
}

bool SceneLayers::Delete(
	Scene& scene,
	SceneLayerId id,
	std::optional<SceneLayerId> replacement
) {
	if (!id || id == default_entity_layer_) {
		return false;
	}

	const auto layer_it{ std::ranges::find(layers_, id, &SceneLayer::id) };
	if (layer_it == layers_.end()) {
		return false;
	}

	std::vector<Entity> members;
	for (const auto& [uuid, layer] : memberships_) {
		if (layer != id) {
			continue;
		}
		if (Entity entity{ scene.GetEntity(uuid) }) {
			members.emplace_back(entity);
		}
	}

	if (!members.empty()) {
		if (!replacement.has_value() || replacement.value() == id || !Find(replacement.value())) {
			return false;
		}

		for (Entity entity : members) {
			if (!CanAssign(entity, replacement.value())) {
				return false;
			}
		}

		for (Entity entity : members) {
			memberships_[entity.Get<UUID>()] = replacement.value();
		}
	}

	for (auto it{ memberships_.begin() }; it != memberships_.end();) {
		if (it->second == id) {
			it = memberships_.erase(it);
		} else {
			++it;
		}
	}

	layers_.erase(layer_it);
	return true;
}

bool SceneLayers::Move(SceneLayerId id, std::size_t new_index) {
	const auto it{ std::ranges::find(layers_, id, &SceneLayer::id) };
	if (it == layers_.end() || layers_.empty()) {
		return false;
	}

	new_index = std::min(new_index, layers_.size() - 1);
	const std::size_t old_index{ static_cast<std::size_t>(std::distance(layers_.begin(), it)) };
	if (old_index == new_index) {
		return false;
	}

	SceneLayer moved{ std::move(layers_[old_index]) };
	layers_.erase(layers_.begin() + static_cast<std::ptrdiff_t>(old_index));
	layers_.insert(layers_.begin() + static_cast<std::ptrdiff_t>(new_index), std::move(moved));
	return true;
}

bool SceneLayers::Rename(SceneLayerId id, std::string_view name) {
	if (name.empty()) {
		return false;
	}
	SceneLayer* layer{ Find(id) };
	if (!layer || layer->name == name) {
		return false;
	}
	layer->name = name;
	return true;
}

bool SceneLayers::SetVisible(SceneLayerId id, bool visible) {
	SceneLayer* layer{ Find(id) };
	if (!layer || layer->visible == visible) {
		return false;
	}
	layer->visible = visible;
	return true;
}

bool SceneLayers::SetLocked(SceneLayerId id, bool locked) {
	SceneLayer* layer{ Find(id) };
	if (!layer || layer->locked == locked) {
		return false;
	}
	layer->locked = locked;
	return true;
}

bool SceneLayers::SetSelectable(SceneLayerId id, bool selectable) {
	SceneLayer* layer{ Find(id) };
	if (!layer || layer->selectable == selectable) {
		return false;
	}
	layer->selectable = selectable;
	return true;
}

std::optional<SceneLayerId> SceneLayers::GetLayerId(UUID entity) const {
	const auto it{ memberships_.find(entity) };
	return it == memberships_.end()
		? std::nullopt
		: std::optional<SceneLayerId>{ it->second };
}

std::optional<SceneLayerId> SceneLayers::GetLayerId(Entity entity) const {
	if (!entity || !entity.Has<UUID>()) {
		return std::nullopt;
	}
	return GetLayerId(entity.Get<UUID>());
}

SceneLayer* SceneLayers::GetLayer(Entity entity) {
	const auto id{ GetLayerId(entity) };
	return id ? Find(*id) : nullptr;
}

const SceneLayer* SceneLayers::GetLayer(Entity entity) const {
	const auto id{ GetLayerId(entity) };
	return id ? Find(*id) : nullptr;
}

std::vector<Entity> SceneLayers::GetRootEntities(const Scene& scene, SceneLayerId layer) const {
	std::vector<Entity> output;
	if (!Find(layer)) {
		return output;
	}

	for (Entity entity : scene.Entities()) {
		if (HasParent(entity)) {
			continue;
		}
		const auto entity_layer{ GetLayerId(entity) };
		if (entity_layer.has_value() && entity_layer.value() == layer) {
			output.emplace_back(entity);
		}
	}
	return output;
}

bool SceneLayers::IsAllowed(Entity entity, SceneLayerKind kind) const {
	if (!entity) {
		return false;
	}

	const bool tilemap{ IsTilemap(entity) };
	const bool generator{ IsPaintGenerator(entity) };

	if (kind == SceneLayerKind::Tile) {
		return tilemap || generator;
	}

	// Generator entities are deliberately valid in Entity layers. Their layer kind determines
	// whether their eventual recipe emits entities or tiles.
	return !tilemap;
}

bool SceneLayers::CanAssign(Entity entity, SceneLayerId layer) const {
	const SceneLayer* destination{ Find(layer) };
	return destination && IsAllowed(entity, destination->kind);
}

void SceneLayers::CollectSubtree(Entity entity, std::vector<Entity>& output) const {
	if (!entity) {
		return;
	}

	output.emplace_back(entity);
	if (!HasChildren(entity)) {
		return;
	}

	for (Entity child : GetChildren(entity)) {
		CollectSubtree(child, output);
	}
}

bool SceneLayers::Assign(Entity entity, SceneLayerId layer, bool include_children) {
	const SceneLayer* destination{ Find(layer) };
	if (!destination || !entity) {
		return false;
	}

	std::vector<Entity> entities;
	if (include_children) {
		CollectSubtree(entity, entities);
	} else {
		entities.emplace_back(entity);
	}

	for (Entity candidate : entities) {
		if (!IsAllowed(candidate, destination->kind)) {
			return false;
		}
	}

	for (Entity candidate : entities) {
		PTGN_ASSERT(candidate.Has<UUID>(), "Layered entity must have a UUID");
		memberships_[candidate.Get<UUID>()] = layer;
	}
	return true;
}

void SceneLayers::RegisterEntity(Entity entity) {
	if (!entity || !entity.Has<UUID>()) {
		return;
	}

	memberships_.try_emplace(entity.Get<UUID>(), default_entity_layer_);
}

void SceneLayers::UnregisterEntity(UUID entity) {
	memberships_.erase(entity);
}

void SceneLayers::Prune(Scene& scene) {
	std::unordered_set<UUID, SceneLayerUUIDHasher> live_entities;
	live_entities.reserve(scene.GetEntityCount());

	for (Entity entity : scene.Entities()) {
		if (!entity.Has<UUID>()) {
			continue;
		}
		const UUID uuid{ entity.Get<UUID>() };
		live_entities.insert(uuid);
		memberships_.try_emplace(uuid, default_entity_layer_);
	}

	for (auto it{ memberships_.begin() }; it != memberships_.end();) {
		if (!live_entities.contains(it->first)) {
			it = memberships_.erase(it);
		} else {
			++it;
		}
	}
}

bool SceneLayers::IsVisible(Entity entity) const {
	const SceneLayer* layer{ GetLayer(entity) };
	return !layer || layer->visible;
}

bool SceneLayers::IsLocked(Entity entity) const {
	const SceneLayer* layer{ GetLayer(entity) };
	return layer && layer->locked;
}

bool SceneLayers::IsSelectable(Entity entity) const {
	const SceneLayer* layer{ GetLayer(entity) };
	return !layer || layer->selectable;
}

SerializedSceneLayers SceneLayers::Serialize(const Scene& scene) const {
	SerializedSceneLayers serialized{
		.layers = layers_,
		.default_entity_layer = default_entity_layer_,
		.next_layer_id = next_layer_id_,
	};

	serialized.memberships.reserve(scene.GetEntityCount());
	for (Entity entity : scene.Entities()) {
		PTGN_ASSERT(entity.Has<UUID>(), "Serialized layered entity must have a UUID");
		const UUID uuid{ entity.Get<UUID>() };
		const SceneLayerId layer{ GetLayerId(uuid).value_or(default_entity_layer_) };
		serialized.memberships.push_back(SerializedSceneLayerMembership{
			.entity = uuid,
			.layer = layer,
		});
	}
	return serialized;
}

void SceneLayers::Deserialize(const SerializedSceneLayers& serialized) {
	layers_ = serialized.layers;
	memberships_.clear();
	default_entity_layer_ = serialized.default_entity_layer;
	next_layer_id_ = serialized.next_layer_id;

	PTGN_ASSERT(!layers_.empty(), "Serialized scene must contain at least one layer");
	PTGN_ASSERT(default_entity_layer_, "Serialized scene must identify a default entity layer");

	const SceneLayer* default_layer{ Find(default_entity_layer_) };
	PTGN_ASSERT(default_layer, "Serialized default entity layer does not exist");
	PTGN_ASSERT(
		default_layer->kind == SceneLayerKind::Entity,
		"Serialized default scene layer must be an Entity layer"
	);

	for (const auto& membership : serialized.memberships) {
		PTGN_ASSERT(Find(membership.layer), "Serialized entity references a missing scene layer");
		memberships_.insert_or_assign(membership.entity, membership.layer);
	}

	std::uint64_t greatest_id{};
	for (const auto& layer : layers_) {
		PTGN_ASSERT(layer.id, "Scene layer ids must be non-zero");
		greatest_id = std::max(greatest_id, layer.id.value);
	}
	next_layer_id_ = std::max(next_layer_id_, greatest_id + 1);
}

bool SceneLayers::Validate(const Scene& scene) const {
	if (!default_entity_layer_) {
		return false;
	}
	const SceneLayer* default_layer{ Find(default_entity_layer_) };
	if (!default_layer || default_layer->kind != SceneLayerKind::Entity) {
		return false;
	}

	for (std::size_t i{}; i < layers_.size(); ++i) {
		if (!layers_[i].id) {
			return false;
		}
		for (std::size_t j{ i + 1 }; j < layers_.size(); ++j) {
			if (layers_[i].id == layers_[j].id) {
				return false;
			}
		}
	}

	for (Entity entity : scene.Entities()) {
		const auto layer_id{ GetLayerId(entity) };
		if (!layer_id.has_value()) {
			return false;
		}
		const SceneLayer* layer{ Find(layer_id.value()) };
		if (!layer || !IsAllowed(entity, layer->kind)) {
			return false;
		}

		if (IsPaintGenerator(entity)) {
			const auto& generator{ entity.Get<impl::PaintGeneratorData>() };
			if (generator.target_tilemap.has_value()) {
				if (layer->kind != SceneLayerKind::Tile) {
					return false;
				}
				const Entity target{ scene.GetEntity(generator.target_tilemap.value()) };
				if (!target || !IsTilemap(target) || GetLayerId(target) != layer_id) {
					return false;
				}
			}
		}
	}
	return true;
}

} // namespace ptgn
