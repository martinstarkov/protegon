#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/ecs/entity.h"
#include "math/vector2.h"
#include "physics/bounding_aabb.h"

namespace ptgn {

namespace impl {

enum class KDAxis {
	X = 0,
	Y = 1
};

struct KDObject {
	Entity entity;
	BoundingAABB aabb;
	// "deleted" flag for lazy removals used inside partial updates
	bool deleted{ false };

	[[nodiscard]] float GetCenter(KDAxis axis) const;
};

struct KDNode {
	KDAxis split_axis{ KDAxis::X };
	float split_value{ 0.0f };

	std::vector<KDObject> objects; // only populated on leaves
	std::unique_ptr<KDNode> left;
	std::unique_ptr<KDNode> right;
};

class KDTree {
public:
	KDTree(std::size_t max_objects_per_node = 64, float rebuild_threshold = 0.25f);

	// (Re)Build KD-tree from scratch (clears moved list).
	void Build(const std::vector<KDObject>& objects);

	// TODO: In the future consider moving to a cached KD-tree where the following events will
	// trigger an entity to be updated within the KD-tree.
	/*
	 * Entity moved (own transform changed)	-> Mark as dirty.
	 * Entity's parent moved -> Mark entity and descendants as dirty.
	 * Transform added/removed -> Mark entity as dirty.
	 * Parent changed (reparenting)	-> Mark entity and descendants as dirty.
	 * Shape changed -> Mark entity as dirty.
	 * Shape added -> Insert into KD-tree.
	 * Shape removed -> Remove from KD-tree.
	 * Entity destroyed -> Remove from KD-tree (use a Spatial tag component with hooks).
	 */
	// Mark an entity as moved during the frame. Doesn't touch the tree immediately.
	void UpdateBoundingAABB(const Entity& e, const BoundingAABB& aabb);

	// Insert new entity immediately (optional). Also mark as moved to ensure it's processed.
	void Insert(const Entity& e, const BoundingAABB& aabb);

	// Remove entity immediately (mark for removal), processed at EndFrame.
	void Remove(const Entity& e);

	// Should be called once per frame after all
	// UpdateBoundingAABB()/Insert()/Remove()
	void EndFrameUpdate();

	// Note: If region is a bounding volume inside of the KD-tree, Query will return that region
	// entity as well (in other words, you must check for self collisions).
	std::vector<Entity> Query(const BoundingAABB& region) const;

	std::vector<Entity> Query(const V2_float& point) const;

	// @param entity passed to avoid raycasting against itself.
	std::vector<Entity> Raycast(const Entity& entity, const V2_float& dir, const BoundingAABB& aabb)
		const;

	// @param entity passed to avoid raycasting against itself.
	Entity RaycastFirst(const Entity& entity, const V2_float& dir, const BoundingAABB& aabb) const;

private:
	std::unique_ptr<KDNode> root;
	std::unordered_map<Entity, KDObject> entity_map;
	std::unordered_set<Entity> moved_entities;

	std::size_t max_objects_per_node{ 64 };
	float rebuild_threshold{ 0.25f };

	template <typename Func>
	void Traverse(const KDNode* node, Func&& visit) const {
		if (!node) {
			return;
		}
		for (const auto& obj : node->objects) {
			if (!obj.deleted) {
				visit(obj);
			}
		}
		if (node->left) {
			Traverse(node->left.get(), visit);
		}
		if (node->right) {
			Traverse(node->right.get(), visit);
		}
	}

	std::unique_ptr<KDNode> BuildRecursive(const std::vector<KDObject>& objects, int depth);

	// Strategy:
	// 1) For each moved entity that exists in the tree, find the leaf it currently resides in
	// (using the
	//    *old* rect stored in entity_map before the move). We use the entity_map's rect because
	//    when the user called UpdateRect we already wrote the new rect there. To find the old tree
	//    leaf we need the "previous" rect; to keep it simple here, we assume UpdateRect replaced
	//    rect in entity_map but we have also kept a copy of the previous rect in the node's objects
	//    (we will match by entity id). So we traverse the tree like in Remove() to find the leaf
	//    and mark that object as deleted (swap-remove) so the node keeps compact storage.
	// 2) Collect the moved objects from entity_map (current rect) and insert them into leaf nodes
	// in bulk. 3) For any leaf nodes that exceed capacity, call SplitNode once per such node
	// (recursive splitting)
	void PartialUpdate();

	// Find and remove the object with entity id from the tree by traversing to leaves using the
	// object's position inside the node (we search node->objects for the entity). When found, we
	// swap-pop to remove it quickly and record the leaf pointer.
	bool RemoveFromTree(KDNode* node, Entity e, int depth, std::vector<KDNode*>& touched_leaves);

	void CompactTree(KDNode* node);

	// Insert object into a leaf (descend using object's rect). We do NOT split here.
	void InsertIntoLeaf(KDNode* node, const KDObject& obj, int depth);

	// Compute depth of a target leaf by walking tree; returns -1 if not found.
	[[nodiscard]] int ComputeDepth(KDNode* current, KDNode* target, int depth);

	// External SplitNode: we'll emulate the same logic as your original SplitNode but accept a
	// pointer to an existing leaf.
	void SplitNodeExternal(KDNode* node, int depth);
};

} // namespace impl

} // namespace ptgn