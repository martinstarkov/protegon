#include "runtime/physics/broadphase.h"

#include <algorithm>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/math/geometry/rect.h"
#include "core/math/raycast.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/physics/bounding_aabb.h"

namespace ptgn {

namespace impl {

float KDObject::GetCenter(KDAxis axis) const {
	float center = (axis == KDAxis::X) ? ((aabb.min.x + aabb.max.x) * 0.5f)
									   : ((aabb.min.y + aabb.max.y) * 0.5f);
	return center;
}

KDTree::KDTree(std::size_t max_objects, float threshold) :
	max_objects_per_node{ max_objects }, rebuild_threshold{ threshold } {}

void KDTree::Build(const std::vector<KDObject>& objects) {
	entity_map.clear();
	std::vector<KDObject> alive;
	alive.reserve(objects.size());

	for (const auto& object : objects) {
		if (!object.entity) {
			continue;
		}

		UUID uuid{ object.entity.Get<UUID>() };
		entity_map.insert_or_assign(uuid, object);
		alive.push_back(object);
	}

	root = BuildRecursive(alive, 0);
	RebuildNodeBounds(root.get());
	moved_entities.clear();
	previous_aabbs_.clear();
}

void KDTree::UpdateBoundingAABB(Entity entity, BoundingAABB aabb) {
	if (!entity) {
		return;
	}

	UUID uuid{ entity.Get<UUID>() };
	auto existing{ entity_map.find(uuid) };

	if (existing != entity_map.end() && existing->second.entity != entity) {
		// This UUID now belongs to a different handle: evict the old leaf first.
		Remove(uuid);
		EndFrameUpdate();
		existing = entity_map.end();
	}

	if (existing != entity_map.end()) {
		previous_aabbs_.try_emplace(uuid, existing->second.aabb);
		existing->second = KDObject{ entity, aabb, false };
	} else {
		entity_map.emplace(uuid, KDObject{ entity, aabb, false });
	}

	moved_entities.insert_or_assign(uuid, entity);
}

void KDTree::Insert(Entity entity, BoundingAABB aabb) {
	UpdateBoundingAABB(entity, aabb);
}

void KDTree::Remove(Entity entity) {
	if (!entity) {
		// A destroyed handle has no readable UUID. This path is uncommon,
		// so a linear search is preferable to storing unsafe Entity hash keys.
		for (const auto& [uuid, object] : entity_map) {
			if (object.entity == entity) {
				Remove(uuid);
				return;
			}
		}

		return;
	}

	Remove(entity.Get<UUID>());
}

void KDTree::Remove(UUID uuid) {
	auto existing{ entity_map.find(uuid) };

	if (existing == entity_map.end()) {
		return;
	}

	// Capture the old position and handle BEFORE erasing. The handle can already
	// be destroyed; its component data will never be accessed during removal.
	previous_aabbs_.try_emplace(uuid, existing->second.aabb);
	moved_entities.insert_or_assign(uuid, existing->second.entity);
	entity_map.erase(existing);
}

void KDTree::EndFrameUpdate() {
	// Prune destroyed handles even if the caller has not explicitly removed them.
	// The cached UUID is safe to access; the Entity's components are not.
	for (auto it{ entity_map.begin() }; it != entity_map.end();) {
		if (it->second.entity) {
			++it;
			continue;
		}

		UUID uuid{ it->first };
		previous_aabbs_.try_emplace(uuid, it->second.aabb);
		moved_entities.insert_or_assign(uuid, it->second.entity);
		it = entity_map.erase(it);
	}

	std::size_t moved = moved_entities.size();
	std::size_t total = entity_map.size();

	if (moved == 0) {
		return;
	}

	if (total == 0) {
		// nothing to do
		root.reset();
		moved_entities.clear();
		previous_aabbs_.clear();
		return;
	}

	// If too many changed, rebuild fully from entity_map (fast, cache friendly)
	if (moved >=
		std::max(1uz, static_cast<std::size_t>(rebuild_threshold * static_cast<float>(total)))) {
		std::vector<KDObject> all;
		all.reserve(entity_map.size());
		for (const auto& [_, object] : entity_map) {
			if (object.entity) {
				all.emplace_back(object);
			}
		}
		root = BuildRecursive(all, 0);
		RebuildNodeBounds(root.get());
		moved_entities.clear();
		previous_aabbs_.clear();
		return;
	}

	// Otherwise, do a partial update (bulk remove + bulk insert)
	PartialUpdate();

	CompactTree(root.get());
	RebuildNodeBounds(root.get());

	moved_entities.clear();
	previous_aabbs_.clear();
}

void KDTree::RebuildNodeBounds(KDNode* node) {
	if (!node) {
		return;
	}
	node->has_bounds = false;
	auto include	 = [node](BoundingAABB other) {
		if (!node->has_bounds) {
			node->bounds	 = other;
			node->has_bounds = true;
		} else {
			node->bounds.min.x = std::min(node->bounds.min.x, other.min.x);
			node->bounds.min.y = std::min(node->bounds.min.y, other.min.y);
			node->bounds.max.x = std::max(node->bounds.max.x, other.max.x);
			node->bounds.max.y = std::max(node->bounds.max.y, other.max.y);
		}
	};
	for (const auto& object : node->objects) {
		if (!object.deleted) {
			include(object.aabb);
		}
	}
	for (KDNode* child : { node->left.get(), node->right.get() }) {
		if (child) {
			RebuildNodeBounds(child);
			if (child->has_bounds) {
				include(child->bounds);
			}
		}
	}
}

void KDTree::QueryNode(const KDNode* node, BoundingAABB region, std::vector<Entity>& result) {
	if (!node || !node->has_bounds || !node->bounds.Overlaps(region)) {
		return;
	}
	for (const auto& object : node->objects) {
		if (!object.deleted && object.entity && object.aabb.Overlaps(region)) {
			result.push_back(object.entity);
		}
	}
	QueryNode(node->left.get(), region, result);
	QueryNode(node->right.get(), region, result);
}

void KDTree::QueryNode(const KDNode* node, V2_float point, std::vector<Entity>& result) {
	if (!node || !node->has_bounds || !node->bounds.Overlaps(point)) {
		return;
	}
	for (const auto& object : node->objects) {
		if (!object.deleted && object.entity && object.aabb.Overlaps(point)) {
			result.push_back(object.entity);
		}
	}
	QueryNode(node->left.get(), point, result);
	QueryNode(node->right.get(), point, result);
}

std::vector<Entity> KDTree::Query(BoundingAABB region) const {
	std::vector<Entity> result;
	QueryNode(root.get(), region, result);
	return result;
}

std::vector<Entity> KDTree::Query(V2_float point) const {
	std::vector<Entity> result;
	QueryNode(root.get(), point, result);
	return result;
}

std::vector<Entity> KDTree::Raycast(Entity entity, V2_float dir, BoundingAABB aabb) const {
	std::vector<Entity> hits;
	Rect rect{ aabb.min, aabb.max };
	BoundingAABB sweep{ aabb.ExpandByVelocity(dir) };
	std::function<void(const KDNode*)> visit = [&](const KDNode* node) {
		if (!node || !node->has_bounds || !node->bounds.Overlaps(sweep)) {
			return;
		}
		for (const auto& obj : node->objects) {
			if (obj.deleted || !obj.entity || obj.entity == entity) {
				continue;
			}
			auto raycast{ RaycastRectRect(
				dir, Transform{}, rect, Transform{}, Rect{ obj.aabb.min, obj.aabb.max }
			) };
			if (raycast.Occurred()) {
				hits.emplace_back(obj.entity);
			}
		}
		visit(node->left.get());
		visit(node->right.get());
	};
	visit(root.get());
	return hits;
}

Entity KDTree::RaycastFirst(Entity entity, V2_float dir, BoundingAABB aabb) const {
	Entity closest_hit;
	float closest_t{ 1.0f };
	Rect rect{ aabb.min, aabb.max };
	BoundingAABB sweep{ aabb.ExpandByVelocity(dir) };
	std::function<void(const KDNode*)> visit = [&](const KDNode* node) {
		if (!node || !node->has_bounds || !node->bounds.Overlaps(sweep)) {
			return;
		}
		for (const auto& obj : node->objects) {
			if (obj.deleted || !obj.entity || obj.entity == entity) {
				continue;
			}
			auto raycast{ RaycastRectRect(
				dir, Transform{}, rect, Transform{}, Rect{ obj.aabb.min, obj.aabb.max }
			) };
			if (raycast.Occurred() && raycast.t < closest_t) {
				closest_t	= raycast.t;
				closest_hit = obj.entity;
			}
		}
		visit(node->left.get());
		visit(node->right.get());
	};
	visit(root.get());
	return closest_hit;
}

std::unique_ptr<KDNode> KDTree::BuildRecursive(const std::vector<KDObject>& objects, int depth) {
	if (objects.empty()) {
		return nullptr;
	}
	auto node{ std::make_unique<KDNode>() };
	// Alternate split axis each time the KD-tree splits.
	node->split_axis = static_cast<KDAxis>(depth % 2);

	// Stop splitting if the KDNode can hold the remaining objects.
	if (objects.size() <= max_objects_per_node) {
		node->objects = objects;
		return node;
	}

	// Get the centers of all the KDObjects.
	std::vector<float> centers(objects.size());
	for (auto i{ 0uz }; i < objects.size(); ++i) {
		centers[i] = objects[i].GetCenter(node->split_axis);
	}

	std::size_t mid{ centers.size() / 2 };
	// Every center below mid index is smaller than mid center, everything above is larger, but
	// not completely sorting the centers (faster).
	std::nth_element(centers.begin(), centers.begin() + static_cast<std::int64_t>(mid), centers.end());
	node->split_value = centers[mid];

	std::vector<KDObject> left_objs;
	std::vector<KDObject> right_objs;
	left_objs.reserve(objects.size() / 2);
	right_objs.reserve(objects.size() / 2);

	// Split objects array into two based on median center. This is "repeated" after nth_element
	// because that only does it for the centers vector, not the objects themselves.
	for (const auto& obj : objects) {
		float center{ obj.GetCenter(node->split_axis) };
		if (center < node->split_value) {
			left_objs.push_back(obj);
		} else {
			right_objs.push_back(obj);
		}
	}

	if (left_objs.empty() || right_objs.empty()) {
		node->objects = objects;
		return node;
	}

	node->left	= BuildRecursive(left_objs, depth + 1);
	node->right = BuildRecursive(right_objs, depth + 1);

	return node;
}

void KDTree::PartialUpdate() {
	if (!root) {
		// No existing tree; build from scratch from entity_map
		std::vector<KDObject> all;
		all.reserve(entity_map.size());
		for (const auto& [_, object] : entity_map) {
			if (object.entity) {
				all.emplace_back(object);
			}
		}
		root = BuildRecursive(all, 0);
		return;
	}

	// Mark/removal step: for each moved entity, try to find it in the tree and remove by
	// swap pop. Also record which leaves were touched so leaves can later be split if
	// needed.
	std::vector<KDNode*> touched_leaves;
	touched_leaves.reserve(moved_entities.size());

	for (const auto& [uuid, entity] : moved_entities) {
		[[maybe_unused]] bool removed{ RemoveFromTree(
			root.get(), uuid, entity, 0, touched_leaves
		) };
	}

	// Bulk insert: gather moved objects from entity_map and insert into leaves without
	// splitting yet Insert directly into leaves to avoid repeated traversals doing node
	// splitting mid flight.
	for (const auto& [uuid, entity] : moved_entities) {
		auto it = entity_map.find(uuid);
		if (it == entity_map.end()) {
			continue; // removed completely by user
		}
		InsertIntoLeaf(root.get(), it->second, 0);
	}

	// Split all touched leaves (and recursively if children need splitting)
	// Deduplicate touched leaves
	std::ranges::sort(touched_leaves);
	touched_leaves.erase(
		std::unique(touched_leaves.begin(), touched_leaves.end()), touched_leaves.end()
	);

	for (KDNode* leaf : touched_leaves) {
		// If leaf still exists and is over capacity, split it
		if (leaf && leaf->objects.size() > max_objects_per_node) {
			// Need to call SplitNode with a depth. Don't store depths in nodes, so
			// compute it by walking from root.
			auto depth = ComputeDepth(root.get(), leaf, 0);
			if (depth.has_value() && depth.value() >= 0) {
				SplitNodeExternal(leaf, depth.value());
			}
		}
	}
}

bool KDTree::RemoveFromTree(
	KDNode* node, UUID uuid, Entity entity, int depth,
	std::vector<KDNode*>& touched_leaves
) {
	if (!node) {
		return false;
	}

	// If leaf, search its vector and mark deleted if found
	if (!node->left && !node->right) {
		auto& vec = node->objects;
		for (auto i{ 0uz }; i < vec.size(); ++i) {
			if (vec[i].entity == entity) {
				vec[i].deleted = true; // Lazy delete
				touched_leaves.push_back(node);
				return true;
			}
		}
		return false;
	}

	// Continue traversal using the entity's PREVIOUS bounds, not its new position.
	auto old{ previous_aabbs_.find(uuid) };
	if (old == previous_aabbs_.end()) {
		return false; // Newly inserted this frame.
	}
	float val = node->split_axis == KDAxis::X ? (old->second.min.x + old->second.max.x) * 0.5f
											  : (old->second.min.y + old->second.max.y) * 0.5f;
	if (val < node->split_value) {
		return RemoveFromTree(node->left.get(), uuid, entity, depth + 1, touched_leaves);
	} else {
		return RemoveFromTree(node->right.get(), uuid, entity, depth + 1, touched_leaves);
	}
}

void KDTree::CompactTree(KDNode* node) {
	if (!node) {
		return;
	}
	if (!node->left && !node->right) {
		std::erase_if(node->objects, [](const KDObject& o) { return o.deleted; });
	} else {
		CompactTree(node->left.get());
		CompactTree(node->right.get());
	}
}

void KDTree::InsertIntoLeaf(KDNode* node, const KDObject& obj, int depth) {
	if (!node) {
		// This shouldn't normally happen as tree exists; if it does, create a small node on the
		// heap.
		return;
	}
	if (!node->left && !node->right) {
		node->objects.emplace_back(obj.entity, obj.aabb, false);
		return;
	}
	float val{ obj.GetCenter(node->split_axis) };
	if (val < node->split_value) {
		InsertIntoLeaf(node->left.get(), obj, depth + 1);
	} else {
		InsertIntoLeaf(node->right.get(), obj, depth + 1);
	}
}

std::optional<int> KDTree::ComputeDepth(const KDNode* current, KDNode* target, int depth) {
	if (!current) {
		return std::nullopt;
	}
	if (current == target) {
		return depth;
	}
	if (auto d = ComputeDepth(current->left.get(), target, depth + 1);
		d.has_value() && d.value() >= 0) {
		return d;
	}
	return ComputeDepth(current->right.get(), target, depth + 1);
}

void KDTree::SplitNodeExternal(KDNode* node, int depth) {
	if (!node) {
		return;
	}
	// If node already has children, do nothing
	if (node->left || node->right) {
		return;
	}

	node->split_axis = static_cast<KDAxis>(depth % 2);
	std::vector<float> centers;
	centers.reserve(node->objects.size());
	for (const auto& o : node->objects) {
		centers.push_back(o.GetCenter(node->split_axis));
	}

	if (centers.empty()) {
		return;
	}

	if (bool all_same{
			std::all_of(centers.begin(), centers.end(), [&](float v) { return v == centers[0]; }) };
		all_same) {
		return;
	}

	std::size_t mid = centers.size() / 2;
	std::nth_element(centers.begin(), centers.begin() + static_cast<std::int64_t>(mid), centers.end());
	node->split_value = centers[mid];

	// Move objects into left/right
	std::vector<KDObject> old = std::move(node->objects);
	node->objects.clear();
	for (const auto& o : old) {
		float v = o.GetCenter(node->split_axis);
		if (v < node->split_value) {
			if (!node->left) {
				node->left = std::make_unique<KDNode>();
			}
			node->left->objects.push_back(o);
		} else {
			if (!node->right) {
				node->right = std::make_unique<KDNode>();
			}
			node->right->objects.push_back(o);
		}
	}

	// Recursively split children if still oversized
	if (node->left && node->left->objects.size() > max_objects_per_node) {
		SplitNodeExternal(node->left.get(), depth + 1);
	}
	if (node->right && node->right->objects.size() > max_objects_per_node) {
		SplitNodeExternal(node->right.get(), depth + 1);
	}
}

} // namespace impl

} // namespace ptgn