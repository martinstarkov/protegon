#include "physics/collision/broadphase.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "components/transform.h"
#include "core/entity.h"
#include "math/geometry/rect.h"
#include "math/raycast.h"
#include "math/vector2.h"
#include "physics/collision/bounding_aabb.h"

namespace ptgn {

namespace impl {

float KDObject::GetCenter(KDAxis axis) const {
	float center = (axis == KDAxis::X) ? ((aabb.min.x + aabb.max.x) * 0.5f)
									   : ((aabb.min.y + aabb.max.y) * 0.5f);
	return center;
}

KDTree::KDTree(std::size_t max_objects_per_node, float rebuild_threshold) :
	max_objects_per_node{ max_objects_per_node }, rebuild_threshold{ rebuild_threshold } {}

void KDTree::Build(const std::vector<KDObject>& objects) {
	entity_map.clear();
	std::vector<KDObject> objs = objects; // copy
	for (const auto& o : objs) {
		entity_map[o.entity] = o;
	}
	root = BuildRecursive(objs, 0);
	moved_entities.clear();
}

void KDTree::UpdateBoundingAABB(const Entity& e, const BoundingAABB& aabb) {
	auto it = entity_map.find(e);
	if (it != entity_map.end()) {
		it->second.aabb = aabb; // update map (source of truth)
		moved_entities.insert(e);
	} else {
		// new entity; treat as inserted
		KDObject o{ e, aabb, false };
		entity_map[e] = o;
		moved_entities.insert(e);
	}
}

void KDTree::Insert(const Entity& e, const BoundingAABB& aabb) {
	KDObject o{ e, aabb, false };
	entity_map[e] = o;
	moved_entities.insert(e);
}

void KDTree::Remove(const Entity& e) {
	entity_map.erase(e);
	// mark so partial update will remove it if applicable
	moved_entities.insert(e);
}

void KDTree::EndFrameUpdate() {
	std::size_t moved = moved_entities.size();
	std::size_t total = entity_map.size();

	if (moved == 0) {
		return;
	}

	if (total == 0) {
		// nothing to do
		root.reset();
		moved_entities.clear();
		return;
	}

	// If too many changed, rebuild fully from entity_map (fast, cache-friendly)
	if (moved >= std::max<std::size_t>(1, static_cast<std::size_t>(rebuild_threshold * total))) {
		std::vector<KDObject> all;
		all.reserve(entity_map.size());
		for (const auto& kv : entity_map) {
			all.push_back(kv.second);
		}
		root = BuildRecursive(all, 0);
		moved_entities.clear();
		return;
	}

	// Otherwise, do a partial update (bulk remove + bulk insert)
	PartialUpdate();

	CompactTree(root.get());

	moved_entities.clear();
}

std::vector<Entity> KDTree::Query(const BoundingAABB& region) const {
	std::vector<Entity> result;
	Traverse(root.get(), [&](const KDObject& obj) {
		if (obj.aabb.Overlaps(region)) {
			result.emplace_back(obj.entity);
		}
	});
	return result;
}

std::vector<Entity> KDTree::Query(const V2_float& point) const {
	std::vector<Entity> result;
	Traverse(root.get(), [&](const KDObject& obj) {
		if (obj.aabb.Overlaps(point)) {
			result.emplace_back(obj.entity);
		}
	});
	return result;
}

std::vector<Entity> KDTree::Raycast(
	const Entity& entity, const V2_float& dir, const BoundingAABB& aabb
) const {
	std::vector<Entity> hits;
	Rect rect{ aabb.min, aabb.max };
	Traverse(root.get(), [&](const KDObject& obj) {
		if (obj.entity == entity) {
			return;
		}
		auto raycast{
			RaycastRectRect(dir, Transform{}, rect, Transform{}, Rect{ obj.aabb.min, obj.aabb.max })
		};
		if (raycast.Occurred()) {
			hits.emplace_back(obj.entity);
		}
	});
	return hits;
}

Entity KDTree::RaycastFirst(
	const Entity& entity, const V2_float& dir, const BoundingAABB& aabb
) const {
	Entity closest_hit;
	float closest_t{ 1.0f };
	Rect rect{ aabb.min, aabb.max };
	Traverse(root.get(), [&](const KDObject& obj) {
		if (obj.entity == entity) {
			return;
		}
		auto raycast{
			RaycastRectRect(dir, Transform{}, rect, Transform{}, Rect{ obj.aabb.min, obj.aabb.max })
		};
		if (raycast.Occurred() && raycast.t < closest_t) {
			closest_t	= raycast.t;
			closest_hit = obj.entity;
		}
	});
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
	for (std::size_t i{ 0 }; i < objects.size(); ++i) {
		centers[i] = objects[i].GetCenter(node->split_axis);
	}

	std::size_t mid{ centers.size() / 2 };
	// Every center below mid index is smaller than mid center, everything above is larger, but
	// not completely sorting the centers (faster).
	std::nth_element(centers.begin(), centers.begin() + mid, centers.end());
	node->split_value = centers[mid];

	std::vector<KDObject> left_objs, right_objs;
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

	node->left	= BuildRecursive(left_objs, depth + 1);
	node->right = BuildRecursive(right_objs, depth + 1);

	return node;
}

void KDTree::PartialUpdate() {
	if (!root) {
		// No existing tree; build from scratch from entity_map
		std::vector<KDObject> all;
		all.reserve(entity_map.size());
		for (const auto& kv : entity_map) {
			all.push_back(kv.second);
		}
		root = BuildRecursive(all, 0);
		return;
	}

	// 1) Mark/removal step: for each moved entity, try to find it in the tree and remove by
	// swap-pop. We'll also record which leaves were touched so we can later split them if
	// needed.
	std::vector<KDNode*> touched_leaves;
	touched_leaves.reserve(moved_entities.size());

	for (Entity e : moved_entities) {
		// if entity isn't present in the tree (inserted this frame), skip removal
		// We'll insert it below from entity_map
		bool found_and_removed = RemoveFromTree(root.get(), e, 0, touched_leaves);
		(void)found_and_removed; // fine if not found
	}

	// 2) Bulk-insert: gather moved objects from entity_map and insert into leaves without
	// splitting yet We insert directly into leaves to avoid repeated traversals doing node
	// splitting mid-flight.
	for (Entity e : moved_entities) {
		auto it = entity_map.find(e);
		if (it == entity_map.end()) {
			continue; // removed completely by user
		}
		InsertIntoLeaf(root.get(), it->second, 0);
	}

	// 3) Split all touched leaves (and recursively if children need splitting)
	// We deduplicate touched leaves
	std::ranges::sort(touched_leaves);
	touched_leaves.erase(
		std::unique(touched_leaves.begin(), touched_leaves.end()), touched_leaves.end()
	);

	for (KDNode* leaf : touched_leaves) {
		// if leaf still exists and is over capacity, split it
		if (leaf && leaf->objects.size() > max_objects_per_node) {
			// We need to call SplitNode with a depth. We don't store depths in nodes, so we
			// compute it by walking from root.
			int depth = ComputeDepth(root.get(), leaf, 0);
			if (depth >= 0) {
				SplitNodeExternal(leaf, depth);
			}
		}
	}
}

bool KDTree::RemoveFromTree(
	KDNode* node, Entity e, int depth, std::vector<KDNode*>& touched_leaves
) {
	if (!node) {
		return false;
	}

	// If leaf, search its vector and mark deleted if found
	if (!node->left && !node->right) {
		auto& vec = node->objects;
		for (std::size_t i = 0; i < vec.size(); ++i) {
			if (vec[i].entity == e) {
				vec[i].deleted = true; // Lazy delete
				touched_leaves.push_back(node);
				return true;
			}
		}
		return false;
	}

	// Continue traversal
	auto it = entity_map.find(e);
	if (it == entity_map.end()) {
		return false;
	}
	float val = it->second.GetCenter(node->split_axis);
	if (val < node->split_value) {
		return RemoveFromTree(node->left.get(), e, depth + 1, touched_leaves);
	} else {
		return RemoveFromTree(node->right.get(), e, depth + 1, touched_leaves);
	}
}

void KDTree::CompactTree(KDNode* node) {
	if (!node) {
		return;
	}
	if (!node->left && !node->right) {
		auto& vec = node->objects;
		vec.erase(
			std::remove_if(vec.begin(), vec.end(), [](const KDObject& o) { return o.deleted; }),
			vec.end()
		);
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
		node->objects.push_back({ obj.entity, obj.aabb, false });
		return;
	}
	float val{ obj.GetCenter(node->split_axis) };
	if (val < node->split_value) {
		InsertIntoLeaf(node->left.get(), obj, depth + 1);
	} else {
		InsertIntoLeaf(node->right.get(), obj, depth + 1);
	}
}

int KDTree::ComputeDepth(KDNode* current, KDNode* target, int depth) {
	if (!current) {
		return -1;
	}
	if (current == target) {
		return depth;
	}
	int d = ComputeDepth(current->left.get(), target, depth + 1);
	if (d >= 0) {
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

	bool all_same{ std::all_of(centers.begin(), centers.end(), [&](float v) {
		return v == centers[0];
	}) };

	if (all_same) {
		return;
	}

	std::size_t mid = centers.size() / 2;
	std::nth_element(centers.begin(), centers.begin() + mid, centers.end());
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

	// Recursively split children if they are still oversized
	if (node->left && node->left->objects.size() > max_objects_per_node) {
		SplitNodeExternal(node->left.get(), depth + 1);
	}
	if (node->right && node->right->objects.size() > max_objects_per_node) {
		SplitNodeExternal(node->right.get(), depth + 1);
	}
}

} // namespace impl

} // namespace ptgn