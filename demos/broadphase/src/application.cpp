#include <algorithm>
#include <cassert>
#include <cstddef>
#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "components/draw.h"
#include "components/movement.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "debug/profiling.h"
#include "input/input_handler.h"
#include "math/geometry/rect.h"
#include "math/overlap.h"
#include "math/raycast.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "physics/physics.h"
#include "physics/rigid_body.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

// TODO: Move all of this into the collision system.

struct BoundingAABB {
	V2_float min;
	V2_float max;

	[[nodiscard]] bool Overlaps(const BoundingAABB& other) const {
		return !(
			max.x < other.min.x || min.x > other.max.x || max.y < other.min.y || min.y > other.max.y
		);
	}
};

struct KDObject {
	Entity entity;
	BoundingAABB aabb;
	// "deleted" flag for lazy removals used inside partial updates
	bool deleted{ false };
};

enum class KDAxis {
	X = 0,
	Y = 1
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
	KDTree(std::size_t max_objects_per_node = 64, float rebuild_threshold = 0.25f) :
		max_objects_per_node{ max_objects_per_node }, rebuild_threshold{ rebuild_threshold } {}

	// (Re)Build KD-tree from scratch (clears moved list).
	void Build(const std::vector<KDObject>& objects) {
		entity_map.clear();
		std::vector<KDObject> objs = objects; // copy
		for (const auto& o : objs) {
			entity_map[o.entity] = o;
		}
		root = BuildRecursive(objs, 0);
		moved_entities.clear();
	}

	// TODO: In the future consider moving to a cached KD-tree where the following events will
	// trigger an entity to be updated within the KD-tree.
	/*
	 * Entity moved (own transform changed)	-> Mark as dirty.
	 * Entity’s parent moved -> Mark entity and descendants as dirty.
	 * Transform added/removed -> Mark entity as dirty.
	 * Parent changed (reparenting)	-> Mark entity and descendants as dirty.
	 * Shape changed -> Mark entity as dirty.
	 * Shape added -> Insert into KD-tree.
	 * Shape removed -> Remove from KD-tree.
	 * Entity destroyed -> Remove from KD-tree (use a Spatial tag component with hooks).
	 */
	// Mark an entity as moved during the frame. Doesn't touch the tree immediately.
	void UpdateBoundingAABB(const Entity& e, const BoundingAABB& aabb) {
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

	// Insert new entity immediately (optional). Also mark as moved to ensure it's processed.
	void Insert(const Entity& e, const BoundingAABB& aabb) {
		KDObject o{ e, aabb, false };
		entity_map[e] = o;
		moved_entities.insert(e);
	}

	// Remove entity immediately (mark for deletion), processed at EndFrame.
	void Remove(const Entity& e) {
		entity_map.erase(e);
		// mark so partial update will remove it if applicable
		moved_entities.insert(e);
	}

	// Should be called once per frame after all
	// UpdateBoundingAABB()/Insert()/Remove()
	void EndFrameUpdate() {
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
		if (moved >=
			std::max<std::size_t>(1, static_cast<std::size_t>(rebuild_threshold * total))) {
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

	std::vector<Entity> Query(const BoundingAABB& region) const {
		std::vector<Entity> result;
		Query(root.get(), region, result);
		return result;
	}

	std::vector<Entity> Raycast(const Entity& entity, const V2_float& dir, const BoundingAABB& aabb)
		const {
		std::vector<Entity> hits;
		Rect rect{ aabb.min, aabb.max };
		Raycast(entity, root.get(), dir, rect, hits);
		return hits;
	}

	Entity RaycastFirst(const Entity& entity, const V2_float& dir, const BoundingAABB& aabb) const {
		Entity closest_hit{};
		float closest_t = 1.0f;
		Rect rect{ aabb.min, aabb.max };
		RaycastFirst(entity, root.get(), dir, rect, closest_t, closest_hit);
		return closest_hit;
	}

private:
	std::unique_ptr<KDNode> root;
	std::unordered_map<Entity, KDObject> entity_map;
	std::unordered_set<Entity> moved_entities;

	std::size_t max_objects_per_node{ 64 };
	float rebuild_threshold{ 0.25f };

	void Query(const KDNode* node, const BoundingAABB& region, std::vector<Entity>& result) const {
		if (!node) {
			return;
		}
		for (const auto& obj : node->objects) {
			if (!obj.deleted) {
				if (obj.aabb.Overlaps(region)) {
					result.push_back(obj.entity);
				}
			}
		}
		if (node->left) {
			Query(node->left.get(), region, result);
		}
		if (node->right) {
			Query(node->right.get(), region, result);
		}
	}

	void Raycast(
		const Entity& entity, const KDNode* node, const V2_float& dir, const Rect& rect,
		std::vector<Entity>& result
	) const {
		if (!node) {
			return;
		}
		for (const auto& obj : node->objects) {
			if (obj.deleted) {
				continue;
			}
			if (obj.entity == entity) {
				continue;
			}
			auto raycast{ ptgn::impl::RaycastRectRect(
				dir, Transform{}, rect, Transform{}, Rect{ obj.aabb.min, obj.aabb.max }
			) };
			if (raycast.Occurred()) {
				result.push_back(obj.entity);
			}
		}
		if (node->left) {
			Raycast(entity, node->left.get(), dir, rect, result);
		}
		if (node->right) {
			Raycast(entity, node->right.get(), dir, rect, result);
		}
	}

	void RaycastFirst(
		const Entity& entity, const KDNode* node, const V2_float& dir, const Rect& rect,
		float& closest_t, Entity& closest_entity
	) const {
		if (!node) {
			return;
		}
		for (const auto& obj : node->objects) {
			if (obj.deleted) {
				continue;
			}
			if (obj.entity == entity) {
				continue;
			}
			auto raycast{ ptgn::impl::RaycastRectRect(
				dir, Transform{}, rect, Transform{}, Rect{ obj.aabb.min, obj.aabb.max }
			) };
			if (!raycast.Occurred()) {
				continue;
			}
			if (raycast.t < closest_t) {
				closest_t	   = raycast.t;
				closest_entity = obj.entity;
			}
		}
		if (node->left) {
			RaycastFirst(entity, node->left.get(), dir, rect, closest_t, closest_entity);
		}
		if (node->right) {
			RaycastFirst(entity, node->right.get(), dir, rect, closest_t, closest_entity);
		}
	}

	// --- Helpers ---

	std::unique_ptr<KDNode> BuildRecursive(const std::vector<KDObject>& objects, int depth) {
		if (objects.empty()) {
			return nullptr;
		}
		auto node		 = std::make_unique<KDNode>();
		node->split_axis = static_cast<KDAxis>(depth % 2);

		if (objects.size() <= max_objects_per_node) {
			node->objects = objects;
			return node;
		}

		std::vector<float> centers(objects.size());
		for (std::size_t i = 0; i < objects.size(); ++i) {
			centers[i] = GetObjectSplitValue(objects[i], node->split_axis);
		}

		std::size_t mid = centers.size() / 2;
		std::nth_element(centers.begin(), centers.begin() + mid, centers.end());
		node->split_value = centers[mid];

		std::vector<KDObject> left_objs, right_objs;
		left_objs.reserve(objects.size() / 2);
		right_objs.reserve(objects.size() / 2);

		for (const auto& obj : objects) {
			float v = GetObjectSplitValue(obj, node->split_axis);
			if (v < node->split_value) {
				left_objs.push_back(obj);
			} else {
				right_objs.push_back(obj);
			}
		}

		node->left	= BuildRecursive(left_objs, depth + 1);
		node->right = BuildRecursive(right_objs, depth + 1);

		return node;
	}

	float GetObjectSplitValue(const KDObject& obj, KDAxis axis) const {
		float center = (axis == KDAxis::X) ? ((obj.aabb.min.x + obj.aabb.max.x) * 0.5f)
										   : ((obj.aabb.min.y + obj.aabb.max.y) * 0.5f);
		return center;
	}

	// --- Partial update implementation ---
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

	void PartialUpdate() {
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
		std::sort(touched_leaves.begin(), touched_leaves.end());
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

	// Find and remove the object with entity id from the tree by traversing to leaves using the
	// object's position inside the node (we search node->objects for the entity). When found, we
	// swap-pop to remove it quickly and record the leaf pointer.
	bool RemoveFromTree(KDNode* node, Entity e, int depth, std::vector<KDNode*>& touched_leaves) {
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
		float val = GetObjectSplitValue(it->second, node->split_axis);
		if (val < node->split_value) {
			return RemoveFromTree(node->left.get(), e, depth + 1, touched_leaves);
		} else {
			return RemoveFromTree(node->right.get(), e, depth + 1, touched_leaves);
		}
	}

	void CompactTree(KDNode* node) {
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

	// Insert object into a leaf (descend using object's rect). We do NOT split here.
	void InsertIntoLeaf(KDNode* node, const KDObject& obj, int depth) {
		if (!node) {
			// This shouldn't normally happen as tree exists; if it does, create a small node on the
			// heap.
			return;
		}
		if (!node->left && !node->right) {
			node->objects.push_back({ obj.entity, obj.aabb, false });
			return;
		}
		float val = GetObjectSplitValue(obj, node->split_axis);
		if (val < node->split_value) {
			InsertIntoLeaf(node->left.get(), obj, depth + 1);
		} else {
			InsertIntoLeaf(node->right.get(), obj, depth + 1);
		}
	}

	// Compute depth of a target leaf by walking tree; returns -1 if not found.
	int ComputeDepth(KDNode* current, KDNode* target, int depth) {
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

	// External SplitNode: we'll emulate the same logic as your original SplitNode but accept a
	// pointer to an existing leaf.
	void SplitNodeExternal(KDNode* node, int depth) {
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
			centers.push_back(GetObjectSplitValue(o, node->split_axis));
		}

		if (centers.empty()) {
			return;
		}
		bool all_same =
			std::all_of(centers.begin(), centers.end(), [&](float v) { return v == centers[0]; });
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
			float v = GetObjectSplitValue(o, node->split_axis);
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
};

[[nodiscard]] BoundingAABB GetBoundingAABB(const Shape& shape, const Transform& transform) {
	return std::visit(
		[&](const auto& s) -> BoundingAABB {
			using T = std::decay_t<decltype(s)>;

			std::vector<V2_float> vertices;
			if constexpr (std::is_same_v<T, Circle>) {
				auto v = s.GetExtents(transform);
				vertices.assign(v.begin(), v.end());
			} else if constexpr (std::is_same_v<T, Rect>) {
				auto v = s.GetWorldVertices(transform);
				vertices.assign(v.begin(), v.end());
			} else if constexpr (std::is_same_v<T, Polygon>) {
				vertices = s.GetWorldVertices(transform);
			} else if constexpr (std::is_same_v<T, Triangle>) {
				auto v = s.GetWorldVertices(transform);
				vertices.assign(v.begin(), v.end());
			} else if constexpr (std::is_same_v<T, Capsule>) {
				auto v = s.GetWorldVertices(transform);
				V2_float r{ s.GetRadius(transform) };
				// Treat capsule as two circles and a rectangle between them
				vertices.emplace_back(v[0] - r);
				vertices.emplace_back(v[0] + r);
				vertices.emplace_back(v[1] - r);
				vertices.emplace_back(v[1] + r);
			} else if constexpr (std::is_same_v<T, Line>) {
				auto v = s.GetWorldVertices(transform);
				vertices.assign(v.begin(), v.end());
			} else if constexpr (std::is_same_v<T, Point>) {
				// Assume Point is a single position with no size
				V2_float p = transform.position;
				vertices.emplace_back(p);
			}

			V2_float min{ vertices[0] };
			V2_float max{ vertices[0] };

			for (const auto& v : vertices) {
				min.x = std::min(min.x, v.x);
				min.y = std::min(min.y, v.y);
				max.x = std::max(max.x, v.x);
				max.y = std::max(max.y, v.y);
			}

			return { min, max };
		},
		shape
	);
}

[[nodiscard]] BoundingAABB GetBoundingAABB(const Entity& entity) {
	return GetBoundingAABB(entity.Get<Rect>(), GetTransform(entity));
}

Entity AddEntity(
	Scene& scene, const V2_float& center, const V2_float& size, const Color& color,
	bool induce_random_velocity = true
) {
	Entity entity = CreateRect(scene, center, size, color);
	if (induce_random_velocity) {
		auto& rb{ entity.Add<RigidBody>() };
		V2_float dir{ V2_float::RandomNormalized(-0.5f, 0.5f) };
		float speed = 60.0f;
		rb.velocity = dir * speed;
	}
	return entity;
}

#define KDTREE 0

struct BroadphaseScene : public Scene {
	KDTree tree{ 100 };

	std::size_t entity_count{ 10000 };

	Entity player;
	V2_float player_size{ 20, 20 };

	RNG<float> rngx{ 0.0f, (float)window_size.x };
	RNG<float> rngy{ 0.0f, (float)window_size.y };
	RNG<float> rngsize{ 5.0f, 30.0f };

	void Enter() override {
		physics.SetBounds({}, window_size, BoundaryBehavior::ReflectVelocity);

		player = AddEntity(*this, window_size * 0.5f, player_size, color::Purple, false);
		SetDepth(player, 1);

		for (std::size_t i{ 0 }; i < entity_count; ++i) {
			AddEntity(
				*this, { rngx(), rngy() }, { rngsize(), rngsize() }, color::Green,
				false // FlipCoin() // false
			);
		}
		Refresh();
		for (auto [e, rect] : EntitiesWith<Rect>()) {
			// TODO: Only update if entity moved.
			tree.UpdateBoundingAABB(e, GetBoundingAABB(e));
		}
		tree.EndFrameUpdate();
	}

	void Update() override {
		MoveWASD(GetPosition(player), V2_float{ 100.0f } * game.dt(), false);

		for (auto [e, tint] : EntitiesWith<Tint>()) {
			tint = color::Green;
		}

		SetTint(player, color::Purple);

		auto player_volume{ GetBoundingAABB(player) };

#ifdef KDTREE

		if (KDTREE) {
			PTGN_PROFILE_FUNCTION();
			// Check only collisions with relevant k-d tree nodes.

			// TODO: Only update if player moved.
			tree.UpdateBoundingAABB(player, GetBoundingAABB(player));

			// for (auto [e, rect] : EntitiesWith<Rect>()) {
			//	// TODO: Only update if entity moved.
			//	tree.UpdateBoundingAABB(e, GetBoundingAABB(e));
			// }
			tree.EndFrameUpdate();
		} else {
			PTGN_PROFILE_FUNCTION();
			std::vector<KDObject> objects;
			objects.reserve(Size());

			for (auto [e, rect] : EntitiesWith<Rect>()) {
				// TODO: Only update if entity moved.
				objects.emplace_back(e, GetBoundingAABB(e));
			}

			tree.Build(objects);
		}

		// For overlap / trigger tests:

		// PTGN_LOG("---------------------");
		// for (auto [e1, rect1] : EntitiesWith<Rect>()) {
		//	auto b1{ GetBoundingAABB(e1) };
		//	Rect rectb1{ b1.min, b1.max };
		//	auto candidates = tree.Query(b1);
		//	// PTGN_LOG(candidates.size());
		//	for (auto& e2 : candidates) {
		//		if (e1 == e2) {
		//			continue;
		//		}
		//		auto bounding{ GetBoundingAABB(e2) };
		//		Rect rectb2{ bounding.min, bounding.max };
		//		if (Overlap(Transform{}, rectb1, Transform{}, rectb2)) {
		//			SetTint(e1, color::Red);
		//			SetTint(e2, color::Red);
		//		}
		//	}
		//}

		// For full raycasts:

		auto player_pos{ GetPosition(player) };
		auto mouse_pos{ game.input.GetMousePosition() };
		auto dir{ mouse_pos - player_pos };

		auto player_rect{ GetBoundingAABB(player) };

		auto candidates = tree.Raycast(player, dir, player_rect);
		for (auto& candidate : candidates) {
			if (candidate && candidate != player) {
				SetTint(candidate, color::Orange);
			}
		}

		// For first only raycasts:

		auto candidate = tree.RaycastFirst(player, dir, player_rect);
		if (candidate && candidate != player) {
			SetTint(candidate, color::Red);
		}

		DrawDebugLine(player_pos, mouse_pos, color::Gold, 2.0f);

#else
		PTGN_PROFILE_FUNCTION();
		for (auto [e1, rect1] : EntitiesWith<Rect>()) {
			auto b1{ GetBoundingAABB(e1) };
			for (auto [e2, rect2] : EntitiesWith<Rect>()) {
				if (e1 == e2) {
					continue;
				}
				if (Overlap(Transform{}, b1, Transform{}, GetBoundingAABB(e2)) {
					SetTint(e1, color::Red);
					SetTint(e2, color::Red);
				}
			}
		}
#endif
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("BroadphaseScene", window_size);
	game.scene.Enter<BroadphaseScene>("");
	return 0;
}

// Rect GetSweptAABB(const Transform& transform, const V2_float& velocity, float dt) {
//	// TODO: Include rotation and scale.
//	Rect aabb	   = GetBoundingAABB(transform);
//	V2_float delta = velocity * dt;
//
//	V2_float newMin = { std::min(aabb.min.x, aabb.min.x + delta.x),
//						std::min(aabb.min.y, aabb.min.y + delta.y) };
//
//	V2_float newMax = { std::max(aabb.max.x, aabb.max.x + delta.x),
//						std::max(aabb.max.y, aabb.max.y + delta.y) };
//
//	return Rect{ newMin, newMax };
// }