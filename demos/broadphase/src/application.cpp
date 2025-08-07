#include <algorithm>
#include <list>
#include <unordered_map>
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
#include "math/raycast.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "physics/physics.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

/*
// TODO: Move all of this into the collision system.

struct AABB {
	V2_float min;
	V2_float max;

	bool intersects(const AABB& other) const {
		return !(
			max.x < other.min.x || min.x > other.max.x || max.y < other.min.y || min.y > other.max.y
		);
	}

	bool contains(const V2_float& point) const {
		return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
	}

	bool contains(const AABB& other) const {
		return min.x <= other.min.x && max.x >= other.max.x && min.y <= other.min.y &&
			   max.y >= other.max.y;
	}
};

// --- Quadtree Node ---
class QuadtreeNode {
public:
	std::vector<Entity> objects;
	QuadtreeNode* children[4] = { nullptr, nullptr, nullptr, nullptr };
	int maxObjects			  = 4;
	int maxLevels			  = 5;
	int level{ 0 };
	AABB bounds;

	QuadtreeNode(int lvl, const AABB& bounds_) : level(lvl), bounds(bounds_) {}

	~QuadtreeNode() {
		for (auto& c : children) {
			if (c) {
				delete c;
			}
		}
	}

	bool isLeaf() const {
		return children[0] == nullptr;
	}

	void subdivide() {
		float halfWidth	 = (bounds.max.x - bounds.min.x) / 2.0f;
		float halfHeight = (bounds.max.y - bounds.min.y) / 2.0f;
		V2_float mid	 = bounds.min + V2_float(halfWidth, halfHeight);

		children[0] = new QuadtreeNode(level + 1, AABB{ bounds.min, mid });
		children[1] = new QuadtreeNode(
			level + 1, AABB{ V2_float(mid.x, bounds.min.y), V2_float(bounds.max.x, mid.y) }
		);
		children[2] = new QuadtreeNode(
			level + 1, AABB{ V2_float(bounds.min.x, mid.y), V2_float(mid.x, bounds.max.y) }
		);
		children[3] = new QuadtreeNode(level + 1, AABB{ mid, bounds.max });
	}

	int getChildIndex(const AABB& box) const {
		for (int i = 0; i < 4; i++) {
			if (children[i] && children[i]->bounds.contains(box)) {
				return i;
			}
		}
		return -1;
	}

	void insert(const Entity& e, std::unordered_map<Entity, QuadtreeNode*>& entityNodeMap) {
		AABB box = e.Get<AABB>();

		if (!isLeaf()) {
			int idx = getChildIndex(box);
			if (idx != -1) {
				children[idx]->insert(e, entityNodeMap);
				return;
			}
		}

		objects.push_back(e);
		entityNodeMap[e] = this;

		if (isLeaf() && objects.size() > maxObjects && level < maxLevels) {
			subdivide();

			for (auto it = objects.begin(); it != objects.end();) {
				int idx = getChildIndex(it->Get<AABB>());
				if (idx != -1) {
					children[idx]->insert(*it, entityNodeMap);
					entityNodeMap.erase(*it);
					it = objects.erase(it);
				} else {
					++it;
				}
			}
		}
	}

	bool remove(const Entity& e, std::unordered_map<Entity, QuadtreeNode*>& entityNodeMap) {
		for (auto it = objects.begin(); it != objects.end(); ++it) {
			if (*it == e) {
				objects.erase(it);
				entityNodeMap.erase(e);
				return true;
			}
		}

		if (!isLeaf()) {
			for (auto c : children) {
				if (c->remove(e, entityNodeMap)) {
					return true;
				}
			}
		}
		return false;
	}

	void update(const Entity& e, std::unordered_map<Entity, QuadtreeNode*>& entityNodeMap) {
		AABB box				  = e.Get<AABB>();
		QuadtreeNode* currentNode = entityNodeMap[e];

		if (currentNode && currentNode->bounds.contains(box)) {
			// Just update the object in place (no stored AABB so nothing needed)
			// No action needed here since we pull AABB fresh on queries.
		} else {
			// Remove and re-insert to correct node
			if (currentNode) {
				currentNode->remove(e, entityNodeMap);
			}
			insert(e, entityNodeMap);
		}
	}

	void retrieve(const AABB& box, std::vector<Entity>& candidates) const {
		for (const auto& e : objects) {
			if (e.Get<AABB>().intersects(box)) {
				candidates.push_back(e);
			}
		}

		if (!isLeaf()) {
			for (auto c : children) {
				if (c->bounds.intersects(box)) {
					c->retrieve(box, candidates);
				}
			}
		}
	}
};

class Quadtree {
	QuadtreeNode* root;
	std::unordered_map<Entity, QuadtreeNode*> entityNodeMap;

public:
	Quadtree(const AABB& bounds) {
		root = new QuadtreeNode(0, bounds);
	}

	~Quadtree() {
		delete root;
	}

	void insert(const Entity& e) {
		root->insert(e, entityNodeMap);
	}

	void remove(const Entity& e) {
		if (entityNodeMap.find(e) != entityNodeMap.end()) {
			QuadtreeNode* node = entityNodeMap[e];
			node->remove(e, entityNodeMap);
		}
	}

	void update(const Entity& e) {
		if (entityNodeMap.find(e) != entityNodeMap.end()) {
			entityNodeMap[e]->update(e, entityNodeMap);
		} else {
			insert(e);
		}
	}

	std::vector<Entity> retrieve(const AABB& box) const {
		std::vector<Entity> candidates;
		root->retrieve(box, candidates);
		return candidates;
	}
};

bool Overlaps(const AABB& a, const AABB& b) {
	return (a.min.x <= b.max.x && a.max.x >= b.min.x) && (a.min.y <= b.max.y && a.max.y >= b.min.y);
}

#define QUADTREE 1

*/

struct AABB {
	V2_float min;
	V2_float max;

	bool Intersects(const AABB& other) const {
		return !(
			max.x < other.min.x || min.x > other.max.x || max.y < other.min.y || min.y > other.max.y
		);
	}

	bool Contains(const V2_float& point) const {
		return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
	}

	bool IntersectsRay(V2_float origin, V2_float dir, float t0 = 0.0f, float t1 = 1.0f) const {
		for (int i = 0; i < 2; ++i) {
			float invD = 1.0f / (i == 0 ? dir.x : dir.y);
			float tMin = ((i == 0 ? min.x : min.y) - (i == 0 ? origin.x : origin.y)) * invD;
			float tMax = ((i == 0 ? max.x : max.y) - (i == 0 ? origin.x : origin.y)) * invD;
			if (invD < 0.0f) {
				std::swap(tMin, tMax);
			}
			t0 = std::max(t0, tMin);
			t1 = std::min(t1, tMax);
			if (t1 <= t0) {
				return false;
			}
		}
		return true;
	}
};

struct Object {
	Object() = default;

	Object(const Entity& entity, const AABB& aabb) : entity{ entity }, aabb{ aabb } {}

	Entity entity;
	AABB aabb; // Bounding volume.
};

enum class Axis {
	X,
	Y
};

struct KDNode {
	Axis split_axis;
	float split_value = 0.0f;

	std::vector<Object> objects;
	std::unique_ptr<KDNode> left;
	std::unique_ptr<KDNode> right;
};

class KDTree {
public:
	KDTree(std::size_t max_objects_per_node = 1000) :
		max_objects_per_node{ max_objects_per_node } {}

	// Build tree from scratch using all objects upfront
	void Build(const std::vector<Object>& objects) {
		// Clear any existing tree and map
		root.reset();
		entity_map.clear();

		// Build recursively from all objects starting at depth 0
		root = BuildRecursive(objects, 0);

		// Build entity map for fast lookup
		for (const auto& obj : objects) {
			entity_map[obj.entity] = obj;
		}
	}

	void Insert(const Entity& entity, const AABB& aabb) {
		Object obj{ entity, aabb };
		root			   = Insert(std::move(root), obj, 0);
		entity_map[entity] = obj;
	}

	void Update(const Entity& entity, const AABB& new_aabb) {
		Remove(entity);
		Insert(entity, new_aabb);
	}

	void Remove(const Entity& entity) {
		if (auto it = entity_map.find(entity); it != entity_map.end()) {
			root = Remove(std::move(root), entity, 0);
			entity_map.erase(it);
		}
	}

	void SplitNode(std::unique_ptr<KDNode>& node, int depth) {
		Axis axis		 = static_cast<Axis>(depth % 2);
		node->split_axis = axis;

		std::vector<float> centers;
		for (const auto& obj : node->objects) {
			centers.push_back(GetObjectSplitValue(obj, axis));
		}

		// Check if all centers are equal, if so don't split
		bool all_same =
			std::all_of(centers.begin(), centers.end(), [&](float v) { return v == centers[0]; });
		if (all_same) {
			// No meaningful split possible
			return;
		}

		std::nth_element(centers.begin(), centers.begin() + centers.size() / 2, centers.end());
		node->split_value = centers[centers.size() / 2];

		// Save old objects, clear current
		std::vector<Object> old_objects = std::move(node->objects);
		node->objects.clear();

		for (const auto& obj : old_objects) {
			float value = GetObjectSplitValue(obj, axis);
			if (value <= node->split_value) {
				node->left = Insert(std::move(node->left), obj, depth + 1);
			} else {
				node->right = Insert(std::move(node->right), obj, depth + 1);
			}
		}
	}

	float GetObjectSplitValue(const Object& obj, Axis axis) {
		const float center = (axis == Axis::X) ? (obj.aabb.min.x + obj.aabb.max.x) * 0.5f
											   : (obj.aabb.min.y + obj.aabb.max.y) * 0.5f;
		return center;
	}

	std::vector<Entity> Query(const AABB& region) const {
		std::vector<Entity> result;
		Query(root.get(), region, result);
		return result;
	}

	std::vector<Entity> Raycast(Entity entity, V2_float origin, V2_float dir) const {
		std::vector<Entity> hits;
		Raycast(entity, root.get(), origin, dir, hits);
		return hits;
	}

	Entity RaycastFirst(Entity entity, V2_float origin, V2_float dir) const {
		Entity closest_hit{};
		float closest_t = 1.0f;
		RaycastFirst(entity, root.get(), origin, dir, closest_t, closest_hit);
		return closest_hit;
	}

private:
	std::unique_ptr<KDNode> root;
	std::unordered_map<Entity, Object> entity_map;
	std::size_t max_objects_per_node{ 0 };

	std::unique_ptr<KDNode> Remove(std::unique_ptr<KDNode> node, Entity entity, int depth) {
		if (!node) {
			return nullptr;
		}

		// Remove from current node's objects
		node->objects.erase(
			std::remove_if(
				node->objects.begin(), node->objects.end(),
				[&](const Object& o) { return o.entity == entity; }
			),
			node->objects.end()
		);

		// Decide which child to recurse into based on entity's AABB center and split
		auto it = entity_map.find(entity);
		if (it == entity_map.end()) {
			return node;
		}

		const Object& obj = it->second;
		float value		  = GetObjectSplitValue(obj, node->split_axis);

		if (node->left && value < node->split_value) {
			node->left = Remove(std::move(node->left), entity, depth + 1);
		} else if (node->right && value >= node->split_value) {
			node->right = Remove(std::move(node->right), entity, depth + 1);
		}

		// Optional: cleanup empty nodes
		if (node->objects.empty() && !node->left && !node->right) {
			return nullptr;
		}
		return node;
	}

	// Insert improved: only add to leaf nodes or split nodes properly
	std::unique_ptr<KDNode> Insert(std::unique_ptr<KDNode> node, const Object& obj, int depth) {
		if (!node) {
			auto new_node		  = std::make_unique<KDNode>();
			new_node->split_axis  = static_cast<Axis>(depth % 2);
			new_node->split_value = GetObjectSplitValue(obj, new_node->split_axis);
			new_node->objects.push_back(obj);
			return new_node;
		}

		if (!node->left && !node->right) {
			node->objects.push_back(obj);
			if (node->objects.size() > max_objects_per_node) {
				SplitNode(node, depth);
			}
			return node;
		}

		// If node has children, insert into correct child
		float val = GetObjectSplitValue(obj, node->split_axis);
		if (val < node->split_value) {
			node->left = Insert(std::move(node->left), obj, depth + 1);
		} else {
			node->right = Insert(std::move(node->right), obj, depth + 1);
		}

		return node;
	}

	void Query(const KDNode* node, const AABB& region, std::vector<Entity>& result) const {
		if (!node) {
			return;
		}
		for (const auto& obj : node->objects) {
			if (obj.aabb.Intersects(region)) {
				result.push_back(obj.entity);
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
		Entity entity, const KDNode* node, V2_float origin, V2_float dir,
		std::vector<Entity>& result
	) const {
		if (!node) {
			return;
		}
		for (const auto& obj : node->objects) {
			if (obj.entity == entity) {
				continue;
			}
			auto raycast = ptgn::impl::RaycastRect(
				origin, origin + dir, Transform{}, Rect{ obj.aabb.min, obj.aabb.max }
			);
			if (raycast.Occurred()) {
				result.push_back(obj.entity);
			}
		}
		if (node->left) {
			Raycast(entity, node->left.get(), origin, dir, result);
		}
		if (node->right) {
			Raycast(entity, node->right.get(), origin, dir, result);
		}
	}

	void RaycastFirst(
		Entity entity, const KDNode* node, V2_float origin, V2_float dir, float& closest_t,
		Entity& closest_entity
	) const {
		if (!node) {
			return;
		}
		for (const auto& obj : node->objects) {
			if (obj.entity == entity) {
				continue;
			}
			auto raycast = ptgn::impl::RaycastRect(
				origin, origin + dir, Transform{}, Rect{ obj.aabb.min, obj.aabb.max }
			);
			if (raycast.Occurred()) {
				if (raycast.t < closest_t) {
					closest_t	   = raycast.t;
					closest_entity = obj.entity;
				}
			}
		}
		if (node->left) {
			RaycastFirst(entity, node->left.get(), origin, dir, closest_t, closest_entity);
		}
		if (node->right) {
			RaycastFirst(entity, node->right.get(), origin, dir, closest_t, closest_entity);
		}
	}

	std::unique_ptr<KDNode> BuildRecursive(const std::vector<Object>& objects, int depth) {
		// PTGN_LOG(depth);
		if (objects.empty()) {
			return nullptr;
		}

		// Create node
		auto node		 = std::make_unique<KDNode>();
		node->split_axis = static_cast<Axis>(depth % 2);

		// Stop splitting if number of objects is small enough
		if (objects.size() <= max_objects_per_node) {
			node->objects = objects;
			return node;
		}

		// Find median split value
		std::vector<float> centers(objects.size());
		for (size_t i = 0; i < objects.size(); ++i) {
			centers[i] = GetObjectSplitValue(objects[i], node->split_axis);
		}
		size_t mid = centers.size() / 2;
		std::nth_element(centers.begin(), centers.begin() + mid, centers.end());
		node->split_value = centers[mid];

		// Partition objects into left and right sets
		std::vector<Object> left_objs, right_objs;
		for (const auto& obj : objects) {
			float val = GetObjectSplitValue(obj, node->split_axis);
			if (val < node->split_value) {
				left_objs.push_back(obj);
			} else {
				right_objs.push_back(obj);
			}
		}

		// Recursively build children
		node->left	= BuildRecursive(left_objs, depth + 1);
		node->right = BuildRecursive(right_objs, depth + 1);

		return node;
	}
};

AABB GetBoundingVolume(Entity entity) {
	auto position{ GetPosition(entity) };
	// TODO: Use collider size.
	auto half{ entity.Get<Rect>().GetSize() * 0.5f };
	auto center{ position - impl::GetOriginOffsetHalf(GetDrawOrigin(entity), half) };
	return { center - half, center + half };
}

Entity AddEntity(
	Scene& scene, const V2_float& center, const V2_float& size, const Color& color,
	bool induce_random_velocity = true
) {
	Entity entity		  = CreateRect(scene, center, size, color);
	const auto random_vel = []() {
		V2_float dir{ V2_float::Random(-0.5f, 0.5f) };
		float speed = 60.0f;

		if (dir.x != 0 || dir.y != 0) {
			return dir.Normalized() * speed;
		} else {
			return V2_float{ speed, 0.0f };
		}
	};
	if (induce_random_velocity) {
		auto& rb{ entity.Add<RigidBody>() };
		rb.velocity = random_vel();
	}
	AABB bounds{ GetBoundingVolume(entity) };
	return entity;
}

#define KDTREE 0

struct BroadphaseScene : public Scene {
	KDTree tree{ 100 };

	std::size_t entity_count{ 1000 };

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
				*this, { rngx(), rngy() }, { rngsize(), rngsize() }, color::Green, FlipCoin()
			);
		}
	}

	void Update() override {
		PTGN_PROFILE_FUNCTION();

		MoveWASD(GetPosition(player), V2_float{ 100.0f } * game.dt(), false);

		for (auto [e, tint] : EntitiesWith<Tint>()) {
			tint = color::Green;
		}

		SetTint(player, color::Purple);

		auto player_volume{ GetBoundingVolume(player) };

#ifdef KDTREE

		if (KDTREE) {
			// Check only collisions with relevant k-d tree nodes.

			// TODO: Only update if player moved.
			tree.Update(player, GetBoundingVolume(player));

			for (auto [e, rect] : EntitiesWith<Rect>()) {
				// TODO: Only update if entity moved.
				tree.Update(e, GetBoundingVolume(e));
			}
		} else {
			std::vector<Object> objects;
			objects.reserve(Size());

			for (auto [e, rect] : EntitiesWith<Rect>()) {
				// TODO: Only update if entity moved.
				objects.emplace_back(e, GetBoundingVolume(e));
			}

			tree.Build(objects);
		}

		// For overlap / trigger tests:

		// PTGN_LOG("---------------------");
		for (auto [e1, rect1] : EntitiesWith<Rect>()) {
			auto b1{ GetBoundingVolume(e1) };
			auto candidates = tree.Query(b1);
			// PTGN_LOG(candidates.size());
			for (auto& e2 : candidates) {
				if (e1 == e2) {
					continue;
				}
				if (b1.Intersects(GetBoundingVolume(e2))) {
					SetTint(e1, color::Red);
					SetTint(e2, color::Red);
				}
			}
		}

		// For full raycasts:

		auto player_pos{ GetPosition(player) };
		auto mouse_pos{ game.input.GetMousePosition() };
		auto dir{ mouse_pos - player_pos };

		auto candidates = tree.Raycast(player, player_pos, dir);
		for (auto& candidate : candidates) {
			if (candidate && candidate != player) {
				SetTint(candidate, color::Orange);
			}
		}

		// For first only raycasts:

		auto candidate = tree.RaycastFirst(player, player_pos, dir);
		if (candidate && candidate != player) {
			SetTint(candidate, color::Red);
		}

		DrawDebugLine(player_pos, mouse_pos, color::Gold, 2.0f);
#else
		for (auto [e1, rect1] : EntitiesWith<Rect>()) {
			auto b1{ GetBoundingVolume(e1) };
			for (auto [e2, rect2] : EntitiesWith<Rect>()) {
				if (e1 == e2) {
					continue;
				}
				if (b1.Intersects(GetBoundingVolume(e2))) {
					e1.SetTint(color::Red);
					e2.SetTint(color::Red);
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

/*

#include <unordered_set>

#include "core/entity.h"
#include "debug/profiling.h"
#include "math/overlap.h"

struct Object {
	Object() = default;

	Object(const Entity& entity, const Rect& aabb) : entity{ entity }, aabb{ aabb } {}

	Entity entity;
	Rect aabb; // Bounding volume.
};

enum class Axis {
	X,
	Y
};

struct KDNode {
	::Axis split_axis;
	float split_value = 0.0f;

	std::vector<Object> objects;
	std::unique_ptr<KDNode> left;
	std::unique_ptr<KDNode> right;
};

class KDTree {
public:
	KDTree(std::size_t max_objects_per_node = 1000) :
		max_objects_per_node{ max_objects_per_node } {}

	void DebugDraw(const Rect& bounds, int maxDepth = -1) const {
		DebugDrawNode(root.get(), bounds, 0, maxDepth);
	}

	void DebugDrawNode(const KDNode* node, const Rect& bounds, int depth, int maxDepth) const {
		if (!node) {
			return;
		}
		if (maxDepth != -1 && depth > maxDepth) {
			return;
		}

		Color lineColor = (node->split_axis == ::Axis::X) ? color::Red : color::Blue;

		// Draw the splitting line
		if (node->split_axis == ::Axis::X) {
			float x			= node->split_value;
			V2_float top	= { x, bounds.min.y };
			V2_float bottom = { x, bounds.max.y };
			DrawDebugLine(top, bottom, lineColor);
		} else {
			float y		   = node->split_value;
			V2_float left  = { bounds.min.x, y };
			V2_float right = { bounds.max.x, y };
			DrawDebugLine(left, right, lineColor);
		}

		// Draw each AABB at this node (optional)
		for (const auto& obj : node->objects) {
			const V2_float center = obj.aabb.GetCenter(Transform{});
			const V2_float size	  = obj.aabb.GetSize();
			DrawDebugRect(center, size, color::Green);
		}

		// Recurse into children with subdivided bounds
		if (node->split_axis == ::Axis::X) {
			Rect leftBounds	 = bounds;
			leftBounds.max.x = node->split_value;
			DebugDrawNode(node->left.get(), leftBounds, depth + 1, maxDepth);

			Rect rightBounds  = bounds;
			rightBounds.min.x = node->split_value;
			DebugDrawNode(node->right.get(), rightBounds, depth + 1, maxDepth);
		} else {
			Rect bottomBounds  = bounds;
			bottomBounds.max.y = node->split_value;
			DebugDrawNode(node->left.get(), bottomBounds, depth + 1, maxDepth);

			Rect topBounds	= bounds;
			topBounds.min.y = node->split_value;
			DebugDrawNode(node->right.get(), topBounds, depth + 1, maxDepth);
		}
	}

	// Build tree from scratch using all objects upfront
	void Build(const std::vector<Object>& objects) {
		// Clear any existing tree and map
		root.reset();
		entity_map.clear();

		// Build recursively from all objects starting at depth 0
		root = BuildRecursive(objects, 0);

		// Build entity map for fast lookup
		for (const auto& obj : objects) {
			entity_map[obj.entity] = obj;
		}
	}

	void Insert(const Entity& entity, const Rect& aabb) {
		Object obj{ entity, aabb };
		root			   = Insert(std::move(root), obj, 0);
		entity_map[entity] = obj;
	}

	void Update(const Entity& entity, const Rect& new_aabb) {
		Remove(entity);
		Insert(entity, new_aabb);
	}

	void Remove(const Entity& entity) {
		if (auto it = entity_map.find(entity); it != entity_map.end()) {
			root = Remove(std::move(root), entity, 0);
			entity_map.erase(it);
		}
	}

	void SplitNode(std::unique_ptr<KDNode>& node, int depth) {
		::Axis axis		 = static_cast<::Axis>(depth % 2);
		node->split_axis = axis;

		std::vector<float> centers;
		for (const auto& obj : node->objects) {
			centers.push_back(GetObjectSplitValue(obj, axis));
		}

		// Check if all centers are equal, if so don't split
		bool all_same =
			std::all_of(centers.begin(), centers.end(), [&](float v) { return v == centers[0]; });
		if (all_same) {
			// No meaningful split possible
			return;
		}

		std::nth_element(centers.begin(), centers.begin() + centers.size() / 2, centers.end());
		node->split_value = centers[centers.size() / 2];

		// Save old objects, clear current
		std::vector<Object> old_objects = std::move(node->objects);
		node->objects.clear();

		for (const auto& obj : old_objects) {
			float value = GetObjectSplitValue(obj, axis);
			if (value <= node->split_value) {
				node->left = Insert(std::move(node->left), obj, depth + 1);
			} else {
				node->right = Insert(std::move(node->right), obj, depth + 1);
			}
		}
	}

	float GetObjectSplitValue(const Object& obj, ::Axis axis) {
		const float center = (axis == ::Axis::X) ? (obj.aabb.min.x + obj.aabb.max.x) * 0.5f
												 : (obj.aabb.min.y + obj.aabb.max.y) * 0.5f;
		return center;
	}

	std::vector<Entity> Query(const Rect& region) const {
		std::vector<Entity> result;
		Query(root.get(), region, result);
		return result;
	}

	std::vector<Entity> Raycast(Entity entity, const Rect& aabb, V2_float origin, V2_float dir)
		const {
		std::vector<Entity> hits;
		Raycast(entity, aabb, root.get(), origin, dir, hits);
		return hits;
	}

	Entity RaycastFirst(Entity entity, V2_float origin, V2_float dir) const {
		Entity closest_hit{};
		float closest_t = 1.0f;
		RaycastFirst(entity, root.get(), origin, dir, closest_t, closest_hit);
		return closest_hit;
	}

private:
	std::unique_ptr<KDNode> root;
	std::unordered_map<Entity, Object> entity_map;
	std::size_t max_objects_per_node{ 0 };

	std::unique_ptr<KDNode> Remove(std::unique_ptr<KDNode> node, Entity entity, int depth) {
		if (!node) {
			return nullptr;
		}

		// Remove from current node's objects
		node->objects.erase(
			std::remove_if(
				node->objects.begin(), node->objects.end(),
				[&](const Object& o) { return o.entity == entity; }
			),
			node->objects.end()
		);

		// Decide which child to recurse into based on entity's AABB center and split
		auto it = entity_map.find(entity);
		if (it == entity_map.end()) {
			return node;
		}

		const Object& obj = it->second;
		float value		  = GetObjectSplitValue(obj, node->split_axis);

		if (node->left && value < node->split_value) {
			node->left = Remove(std::move(node->left), entity, depth + 1);
		} else if (node->right && value >= node->split_value) {
			node->right = Remove(std::move(node->right), entity, depth + 1);
		}

		// Optional: cleanup empty nodes
		if (node->objects.empty() && !node->left && !node->right) {
			return nullptr;
		}
		return node;
	}

	// Insert improved: only add to leaf nodes or split nodes properly
	std::unique_ptr<KDNode> Insert(std::unique_ptr<KDNode> node, const Object& obj, int depth) {
		if (!node) {
			auto new_node		  = std::make_unique<KDNode>();
			new_node->split_axis  = static_cast<::Axis>(depth % 2);
			new_node->split_value = GetObjectSplitValue(obj, new_node->split_axis);
			new_node->objects.push_back(obj);
			return new_node;
		}

		if (!node->left && !node->right) {
			node->objects.push_back(obj);
			if (node->objects.size() > max_objects_per_node) {
				SplitNode(node, depth);
			}
			return node;
		}

		// If node has children, insert into correct child
		float val = GetObjectSplitValue(obj, node->split_axis);
		if (val < node->split_value) {
			node->left = Insert(std::move(node->left), obj, depth + 1);
		} else {
			node->right = Insert(std::move(node->right), obj, depth + 1);
		}

		return node;
	}

	void Query(const KDNode* node, const Rect& region, std::vector<Entity>& result) const {
		if (!node) {
			return;
		}
		for (const auto& obj : node->objects) {
			if (Overlap(Transform{}, obj.aabb, Transform{}, region)) {
				result.push_back(obj.entity);
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
		Entity entity, const Rect& aabb, const KDNode* node, V2_float origin, V2_float dir,
		std::vector<Entity>& result
	) const {
		if (!node) {
			return;
		}
		for (const auto& obj : node->objects) {
			if (obj.entity == entity) {
				continue;
			}
			auto raycast = ptgn::Raycast(dir, Transform{}, aabb, Transform{}, obj.aabb);
			if (raycast.Occurred()) {
				result.push_back(obj.entity);
			}
		}
		if (node->left) {
			Raycast(entity, aabb, node->left.get(), origin, dir, result);
		}
		if (node->right) {
			Raycast(entity, aabb, node->right.get(), origin, dir, result);
		}
	}

	void RaycastFirst(
		Entity entity, const KDNode* node, V2_float origin, V2_float dir, float& closest_t,
		Entity& closest_entity
	) const {
		if (!node) {
			return;
		}
		for (const auto& obj : node->objects) {
			if (obj.entity == entity) {
				continue;
			}
			auto raycast = ptgn::impl::RaycastRect(origin, origin + dir, Transform{}, obj.aabb);
			if (raycast.Occurred()) {
				if (raycast.t < closest_t) {
					closest_t	   = raycast.t;
					closest_entity = obj.entity;
				}
			}
		}
		if (node->left) {
			RaycastFirst(entity, node->left.get(), origin, dir, closest_t, closest_entity);
		}
		if (node->right) {
			RaycastFirst(entity, node->right.get(), origin, dir, closest_t, closest_entity);
		}
	}

	std::unique_ptr<KDNode> BuildRecursive(const std::vector<Object>& objects, int depth) {
		// PTGN_LOG(depth);
		if (objects.empty()) {
			return nullptr;
		}

		// Create node
		auto node		 = std::make_unique<KDNode>();
		node->split_axis = static_cast<::Axis>(depth % 2);

		// Stop splitting if number of objects is small enough
		if (objects.size() <= max_objects_per_node) {
			node->objects = objects;
			return node;
		}

		// Find median split value
		std::vector<float> centers(objects.size());
		for (size_t i = 0; i < objects.size(); ++i) {
			centers[i] = GetObjectSplitValue(objects[i], node->split_axis);
		}
		size_t mid = centers.size() / 2;
		std::nth_element(centers.begin(), centers.begin() + mid, centers.end());
		node->split_value = centers[mid];

		// Partition objects into left and right sets
		std::vector<Object> left_objs, right_objs;
		for (const auto& obj : objects) {
			float val = GetObjectSplitValue(obj, node->split_axis);
			if (val < node->split_value) {
				left_objs.push_back(obj);
			} else {
				right_objs.push_back(obj);
			}
		}

		// Recursively build children
		node->left	= BuildRecursive(left_objs, depth + 1);
		node->right = BuildRecursive(right_objs, depth + 1);

		return node;
	}
};

enum class CollisionType {
	Solid,
	Overlap,
	Sweep, // new type for swept collision
};

struct RaycastHit {
	Entity entity;
	float distance{ 0.0f };
	V2_float position;
	// Other info as needed
};

struct MyCollider {
	Rect rect{};
	CollisionType type{ CollisionType::Sweep }; // Add this to filter queries later
};

struct HashGridKey {
	int x, y;

	bool operator==(const HashGridKey& other) const {
		return x == other.x && y == other.y;
	}
};

namespace std {
template <>
struct hash<HashGridKey> {
	std::size_t operator()(const HashGridKey& key) const {
		// Boost hash combine trick.
		std::size_t seed{ 0 };
		seed ^= std::hash<int>()(key.x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<int>()(key.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		return seed;
	}
};
} // namespace std

class SpatialHash {
public:
	std::unordered_map<Entity, Rect> entity_aabbs; // <--- Add this

	void DrawDebug() const {
		for (const auto& [key, entities] : grid) {
			V2_float cellMin	= { static_cast<float>(key.x) * cell_size,
									static_cast<float>(key.y) * cell_size };
			V2_float cellSize	= { cell_size, cell_size };
			V2_float cellCenter = cellMin + cellSize * 0.5f;

			DrawDebugRect(cellCenter, cellSize, color::Red, Origin::Center, 1.0f);
		}
	}

	SpatialHash(float cellSize = 64.0f) : cell_size(cellSize) {}

	void Clear() {
		grid.clear();
		entity_to_keys.clear();
	}

	void Insert(Entity entity, const Rect& aabb) {
		auto keys			   = GetKeysForAABB(aabb);
		entity_to_keys[entity] = keys;
		entity_aabbs[entity]   = aabb; // <--- Save exact AABB

		for (auto& key : keys) {
			grid[key].push_back(entity);
		}
	}

	void Update(Entity entity, const Rect& aabb) {
		Remove(entity);
		Insert(entity, aabb);
	}

	void Remove(Entity entity) {
		auto keyIt = entity_to_keys.find(entity);
		if (keyIt != entity_to_keys.end()) {
			for (auto& key : keyIt->second) {
				auto& bucket = grid[key];
				bucket.erase(std::remove(bucket.begin(), bucket.end(), entity), bucket.end());
			}
			entity_to_keys.erase(keyIt);
		}
		entity_aabbs.erase(entity); // <--- Remove stored AABB
	}

	std::vector<Entity> Query(const Rect& aabb) const {
		std::unordered_set<Entity> result;
		auto keys = GetKeysForAABB(aabb);

		for (auto& key : keys) {
			auto it = grid.find(key);
			if (it != grid.end()) {
				for (auto& e : it->second) {
					result.insert(e);
				}
			}
		}
		return { result.begin(), result.end() };
	}

	std::vector<Entity> Raycast(Entity entity, Rect rayAABB, V2_float origin, V2_float velocity)
		const {
		std::vector<Entity> hits;
		std::unordered_set<Entity> visited;

		if (velocity.x == 0 && velocity.y == 0) {
			return hits;
		}

		V2_float dir		= velocity.Normalized();
		float totalDistance = velocity.Magnitude();

		// Start cell and initial t values
		HashGridKey cell = GetCell(origin);

		V2_int step = { dir.x > 0 ? 1 : (dir.x < 0 ? -1 : 0),
						dir.y > 0 ? 1 : (dir.y < 0 ? -1 : 0) };

		V2_float nextBoundary = { (step.x > 0) ? ((cell.x + 1) * cell_size) : (cell.x * cell_size),
								  (step.y > 0) ? ((cell.y + 1) * cell_size)
											   : (cell.y * cell_size) };

		V2_float tMax = { (step.x != 0) ? (nextBoundary.x - origin.x) / dir.x
										: std::numeric_limits<float>::infinity(),
						  (step.y != 0) ? (nextBoundary.y - origin.y) / dir.y
										: std::numeric_limits<float>::infinity() };

		V2_float tDelta = {
			(step.x != 0) ? cell_size / std::abs(dir.x) : std::numeric_limits<float>::infinity(),
			(step.y != 0) ? cell_size / std::abs(dir.y) : std::numeric_limits<float>::infinity()
		};

		float t = 0.0f;

		// --- Initial cell check ---
		if (auto it = grid.find(cell); it != grid.end()) {
			for (Entity e : it->second) {
				if (!visited.insert(e).second) {
					continue;
				}

				auto entityIt = entity_to_keys.find(e);
				if (entityIt == entity_to_keys.end()) {
					continue;
				}

				auto aabbIt = entity_aabbs.find(e);
				if (aabbIt == entity_aabbs.end()) {
					continue;
				}

				const Rect& targetAABB = aabbIt->second;

				RaycastResult result =
					ptgn::Raycast(velocity, Transform{}, rayAABB, Transform{}, targetAABB);
				if (result.Occurred() && result.t <= 1.0f) {
					hits.push_back(e);
				}
			}
		}

		// --- Grid stepping loop ---
		while (t <= totalDistance) {
			// Step to next cell
			if (tMax.x < tMax.y) {
				cell.x += step.x;
				t		= tMax.x;
				tMax.x += tDelta.x;
			} else {
				cell.y += step.y;
				t		= tMax.y;
				tMax.y += tDelta.y;
			}

			if (t > totalDistance) {
				break;
			}

			auto it = grid.find(cell);
			if (it == grid.end()) {
				continue;
			}

			for (Entity e : it->second) {
				if (!visited.insert(e).second) {
					continue;
				}

				auto entityIt = entity_to_keys.find(e);
				if (entityIt == entity_to_keys.end()) {
					continue;
				}

				auto aabbIt = entity_aabbs.find(e);
				if (aabbIt == entity_aabbs.end()) {
					continue;
				}

				const Rect& targetAABB = aabbIt->second;

				RaycastResult result =
					ptgn::Raycast(velocity, Transform{}, rayAABB, Transform{}, targetAABB);
				if (result.Occurred() && result.t <= 1.0f) {
					hits.push_back(e);
				}
			}
		}

		return hits;
	}

private:
	float cell_size;

	std::unordered_map<HashGridKey, std::vector<Entity>> grid;
	std::unordered_map<Entity, std::vector<HashGridKey>> entity_to_keys;

	HashGridKey GetCell(V2_float point) const {
		return { static_cast<int>(std::floor(point.x / cell_size)),
				 static_cast<int>(std::floor(point.y / cell_size)) };
	}

	std::vector<HashGridKey> GetKeysForAABB(const Rect& aabb) const {
		std::vector<HashGridKey> keys;

		int minX = static_cast<int>(std::floor(aabb.min.x / cell_size));
		int maxX = static_cast<int>(std::floor(aabb.max.x / cell_size));
		int minY = static_cast<int>(std::floor(aabb.min.y / cell_size));
		int maxY = static_cast<int>(std::floor(aabb.max.y / cell_size));

		for (int y = minY; y <= maxY; ++y) {
			for (int x = minX; x <= maxX; ++x) {
				keys.push_back({ x, y });
			}
		}
		return keys;
	}
};

enum class CollisionEventType {
	Start,
	Stay,
	End,
	OverlapStart,
	OverlapStay,
	OverlapEnd,
	RaycastHit,
};

struct CollisionPair {
	Entity a;
	Entity b;
	CollisionEventType event;

	CollisionPair(Entity e1, Entity e2, CollisionEventType event) : event{ event } {
		if (e1.WasCreatedBefore(e2)) {
			a = e1;
			b = e2;
		} else {
			a = e2;
			b = e1;
		}
	}

	bool operator==(const CollisionPair& other) const {
		return a == other.a && b == other.b;
	}

	bool operator<(const CollisionPair& other) const {
		if (a == other.a) {
			if (b == other.b) {
				PTGN_ASSERT(event != other.event, "Duplicate collision");
				return event < other.event;
			}
			return b.WasCreatedBefore(other.b);
		}
		return a.WasCreatedBefore(other.a);
	}
};

struct EntityPair {
	Entity a;
	Entity b;

	EntityPair(Entity e1, Entity e2) {
		if (e1.WasCreatedBefore(e2)) {
			a = e1;
			b = e2;
		} else {
			a = e2;
			b = e1;
		}
	}

	bool operator==(const EntityPair& other) const {
		return a == other.a && b == other.b;
	}

	bool operator<(const EntityPair& other) const {
		if (a == other.a) {
			if (b == other.b) {
				return false;
			}
			return b.WasCreatedBefore(other.b);
		}
		return a.WasCreatedBefore(other.a);
	}
};

// #define KDTREE 1

class CollisionSystem {
public:
#ifndef KDTREE
	SpatialHash broadphase{ 64 };
#else

	KDTree broadphase{ 1000 };
#endif

	EntityPair MakeOrderedPair(Entity a, Entity b) {
		return (a.WasCreatedBefore(b)) ? EntityPair(a, b) : EntityPair(b, a);
	}

	std::unordered_map<std::size_t, std::shared_ptr<ptgn::impl::IScript>>
	GetScriptsAttachedToEntity(Entity entity) {
		std::unordered_map<std::size_t, std::shared_ptr<ptgn::impl::IScript>> result;

		if (auto* scriptComp = entity.TryGet<Scripts>()) {
			result = scriptComp->scripts;
		}

		return result;
	}

	std::set<EntityPair> last_overlaps_;
	std::set<CollisionPair> deferred_events_;
	std::set<EntityPair> last_collisions_;

	void DispatchCollisionStartEvent(Entity a, Entity b) {
		// Notify both entities that collision started
		SendEventToEntity(a, CollisionEventType::Start, b);
		SendEventToEntity(b, CollisionEventType::Start, a);
	}

	void DispatchCollisionStayEvent(Entity a, Entity b) {
		SendEventToEntity(a, CollisionEventType::Stay, b);
		SendEventToEntity(b, CollisionEventType::Stay, a);
	}

	void DispatchCollisionEndEvent(Entity a, Entity b) {
		SendEventToEntity(a, CollisionEventType::End, b);
		SendEventToEntity(b, CollisionEventType::End, a);
	}

	void DispatchOverlapStartEvent(Entity a, Entity b) {
		SendEventToEntity(a, CollisionEventType::OverlapStart, b);
		SendEventToEntity(b, CollisionEventType::OverlapStart, a);
	}

	void DispatchOverlapStayEvent(Entity a, Entity b) {
		SendEventToEntity(a, CollisionEventType::OverlapStay, b);
		SendEventToEntity(b, CollisionEventType::OverlapStay, a);
	}

	void DispatchOverlapEndEvent(Entity a, Entity b) {
		SendEventToEntity(a, CollisionEventType::OverlapEnd, b);
		SendEventToEntity(b, CollisionEventType::OverlapEnd, a);
	}

	void DispatchRaycastHitEvent(Entity source, Entity hit) {
		// Raycast hit is directional, so only source likely cares about the hit entity
		SendEventToEntity(source, CollisionEventType::RaycastHit, hit);
	}

	void SendEventToEntity(Entity receiver, CollisionEventType event_type, Entity other) {
		V2_float normal{ 1.0f, 1.0f };
		V2_float raycast{ 2.0f, 2.0f };
		// Example pseudo-code:
		auto scripts = GetScriptsAttachedToEntity(receiver);
		for (auto& [name, script] : scripts) {
			switch (event_type) {
				case CollisionEventType::OverlapStart:
					script->OnCollisionStart(Collision{ other, {} });
					break;
				case CollisionEventType::OverlapStay:
					script->OnCollision(Collision{ other, {} });
					break;
				case CollisionEventType::OverlapEnd:
					script->OnCollisionStop(Collision{ other, {} });
					break;
				case CollisionEventType::Start:
					script->OnCollisionStart(Collision{ other, normal });
					break;
				case CollisionEventType::Stay:
					script->OnCollision(Collision{ other, normal });
					break;
				case CollisionEventType::End:
					script->OnCollisionStop(Collision{ other, normal });
					break;
				case CollisionEventType::RaycastHit:
					script->OnRaycastHit(Collision{ other, raycast });
					break;
			}
		}
	}

	Rect GetBoundingVolume(const Transform& t) {
		// TODO: Include rotation and scale.
		constexpr float halfSize = 16.0f;
		return Rect{ t.position - V2_float{ halfSize, halfSize },
					 t.position + V2_float{ halfSize, halfSize } };
	}

	Rect GetSweptAABB(const Transform& transform, const V2_float& velocity, float dt) {
		// TODO: Include rotation and scale.
		Rect aabb	   = GetBoundingVolume(transform);
		V2_float delta = velocity * dt;

		V2_float newMin = { std::min(aabb.min.x, aabb.min.x + delta.x),
							std::min(aabb.min.y, aabb.min.y + delta.y) };

		V2_float newMax = { std::max(aabb.max.x, aabb.max.x + delta.x),
							std::max(aabb.max.y, aabb.max.y + delta.y) };

		return Rect{ newMin, newMax };
	}

	void Update(Scene& scene, float delta_time) {
		// PTGN_PROFILE_FUNCTION();

		deferred_events_.clear();

		std::set<EntityPair> current_collisions;
		std::set<EntityPair> current_overlaps;
		std::vector<std::tuple<Entity, Entity>> raycast_hits; // (source, hit)

															  // 1. Gather entities with colliders

#ifndef KDTREE

		std::vector<Entity> entities;
		for (auto [e, collider] : scene.EntitiesWith<MyCollider>()) {
			entities.push_back(e);
			broadphase.Update(e, collider.rect);
		}
		// broadphase.DrawDebug();
#else
		std::vector<Entity> entities;
		std::vector<Object> objects;
		for (auto [e, c] : scene.EntitiesWith<MyCollider>()) {
			entities.push_back(e);
			// TODO: Only update if entity moved.
			objects.emplace_back(e, c.rect);
		}
		broadphase.Build(objects);
		// broadphase.DebugDraw(Rect{ V2_float{ 0, 0 }, window_size });
#endif

		// 2. Sweep collisions (solid raycast to prevent tunneling)
		for (auto e : entities) {
			const auto& collider = e.Get<MyCollider>();
			if (!e.Has<RigidBody>()) {
				continue;
			}

			if (collider.type != CollisionType::Sweep) {
				continue;
			}

			const auto& aabb	 = e.Get<MyCollider>().rect;
			const auto& velocity = e.Get<RigidBody>().velocity;
			if (velocity.MagnitudeSquared() < 1e-6f) {
				continue;
			}

			V2_float origin = (aabb.min + aabb.max) * 0.5f;
			V2_float dir	= velocity * delta_time;

			// TODO: Fix.
			// auto candidates = spatial_hash_.Raycast(origin, dir);
			auto candidates = broadphase.Raycast(e, aabb, origin, dir);
			for (auto candidate : candidates) {
				if (candidate == e) {
					continue;
				}

				const auto& candidate_aabb = candidate.Get<MyCollider>().rect;

				auto raycast = Raycast(dir, Transform{}, aabb, Transform{}, candidate_aabb);

				PTGN_LOG(
					"Raycasted from ", e.GetId(), " to ", candidate.GetId(), ": t: ", raycast.t,
					", normal: ", raycast.normal
				);

				if (raycast.Occurred()) {
					// Stop entity from tunneling by zeroing velocity (or adjust as needed)
					const_cast<V2_float&>(velocity) = { 0, 0 };

					current_collisions.insert(MakeOrderedPair(e, candidate));
					raycast_hits.emplace_back(e, candidate);
				}
			}
		}

		// 3. Broadphase overlap for triggers and non-sweep collisions
		for (auto e : entities) {
			const auto& collider = e.Get<MyCollider>();
			const auto& aabb	 = collider.rect;

			// auto candidates = spatial_hash_.Query(aabb);
			auto candidates = broadphase.Query(aabb);

			for (auto candidate : candidates) {
				if (candidate == e) {
					continue;
				}

				const auto& candidate_collider = candidate.Get<MyCollider>();
				const auto& candidate_aabb	   = candidate_collider.rect;

				if (!Overlap(Transform{}, aabb, Transform{}, candidate_aabb)) {
					continue;
				}

				if (collider.type == CollisionType::Overlap ||
					candidate_collider.type == CollisionType::Overlap) {
					// Treat as overlap event
					current_overlaps.insert(MakeOrderedPair(e, candidate));
				} else if (collider.type != CollisionType::Sweep &&
						   candidate_collider.type != CollisionType::Sweep) {
					// Solid collision
					current_collisions.insert(MakeOrderedPair(e, candidate));
				}
			}
		}

		// 4. Dispatch collision start/stay/end events
		// Start and stay collisions
		for (const auto& pair : current_collisions) {
			if (last_collisions_.count(pair) == 0) {
				deferred_events_.emplace(pair.a, pair.b, CollisionEventType::Start);
			} else {
				deferred_events_.emplace(pair.a, pair.b, CollisionEventType::Stay);
			}
		}
		// End collisions
		for (const auto& pair : last_collisions_) {
			if (current_collisions.count(pair) == 0) {
				deferred_events_.emplace(pair.a, pair.b, CollisionEventType::End);
			}
		}

		// 5. Dispatch overlap start/stay/end events (same logic)
		for (const auto& pair : current_overlaps) {
			if (last_overlaps_.count(pair) == 0) {
				deferred_events_.emplace(pair.a, pair.b, CollisionEventType::OverlapStart);
			} else {
				deferred_events_.emplace(pair.a, pair.b, CollisionEventType::OverlapStay);
			}
		}
		for (const auto& pair : last_overlaps_) {
			if (current_overlaps.count(pair) == 0) {
				deferred_events_.emplace(pair.a, pair.b, CollisionEventType::OverlapEnd);
			}
		}

		// 6. Dispatch raycast collision events immediately or deferred
		for (auto& [source, hit] : raycast_hits) {
			deferred_events_.emplace(source, hit, CollisionEventType::RaycastHit);
		}

		// PTGN_LOG(deferred_events_.size());

		// 7. Dispatch all deferred events
		for (const auto& evt : deferred_events_) {
			switch (evt.event) {
				case CollisionEventType::Start: DispatchCollisionStartEvent(evt.a, evt.b); break;
				case CollisionEventType::Stay:	DispatchCollisionStayEvent(evt.a, evt.b); break;
				case CollisionEventType::End:	DispatchCollisionEndEvent(evt.a, evt.b); break;
				case CollisionEventType::OverlapStart:
					DispatchOverlapStartEvent(evt.a, evt.b);
					break;
				case CollisionEventType::OverlapStay: DispatchOverlapStayEvent(evt.a, evt.b); break;
				case CollisionEventType::OverlapEnd:  DispatchOverlapEndEvent(evt.a, evt.b); break;
				case CollisionEventType::RaycastHit:  DispatchRaycastHitEvent(evt.a, evt.b); break;
			}
		}
		deferred_events_.clear();

		// 8. Save collision and overlap sets for next frame
		last_collisions_ = std::move(current_collisions);
		last_overlaps_	 = std::move(current_overlaps);
	}
};

struct E1Script : public Script<E1Script> {
	void OnCollisionStart([[maybe_unused]] Collision collision) {
		PTGN_LOG(
			"Collision start between: ", entity.GetId(), " and ", collision.entity.GetId(),
			", type: ", collision.normal
		);
	}

	void OnCollision([[maybe_unused]] Collision collision) {
		PTGN_LOG(
			"Collision between: ", entity.GetId(), " and ", collision.entity.GetId(),
			", type: ", collision.normal
		);
	}

	void OnCollisionStop([[maybe_unused]] Collision collision) {
		PTGN_LOG(
			"Collision stop between: ", entity.GetId(), " and ", collision.entity.GetId(),
			", type: ", collision.normal
		);
	}

	void OnRaycastHit([[maybe_unused]] Collision collision) {
		PTGN_LOG(
			"Raycast hit between: ", entity.GetId(), " and ", collision.entity.GetId(),
			", type: ", collision.normal
		);
	}
};

Rect GetBoundingVolume(Entity entity) {
	auto position{ entity.GetPosition() };
	// TODO: Use collider size.
	auto half{ entity.Get<MyCollider>().rect.GetSize() * 0.5f };
	auto center{ position - impl::GetOriginOffsetHalf(entity.GetOrigin(), half) };
	return Rect{ center - half, center + half };
}

Entity AddEntity(
	Scene& scene, const V2_float& center, const V2_float& size, const Color& color,
	bool induce_random_velocity = true
) {
	Entity entity  = CreateRect(scene, center, size, color);
	auto& collider = entity.Add<MyCollider>();
	collider.rect  = Rect{ size };
	collider.type  = CollisionType::Overlap;
	entity.Enable();
	const auto random_vel = []() {
		V2_float dir{ V2_float::Random(-0.5f, 0.5f) };
		float speed = 60.0f;

		if (dir.x != 0 || dir.y != 0) {
			return dir.Normalized() * speed;
		} else {
			return V2_float{ speed, 0.0f };
		}
	};
	if (induce_random_velocity) {
		auto& rb{ entity.Add<RigidBody>() };
		rb.velocity = random_vel();
	}
	return entity;
}

class BroadphaseScene : public Scene {
public:
	std::size_t entity_count{ 1000 };

	Entity player;
	V2_float player_size{ 20, 20 };

	RNG<float> rngx{ 0.0f, (float)window_size.x };
	RNG<float> rngy{ 0.0f, (float)window_size.y };
	RNG<float> rngsize{ 5.0f, 30.0f };

	void Enter() override {
#ifndef KDTREE
		game.window.SetTitle("BroadphaseScene: Spatial Hash");
#else
		game.window.SetTitle("BroadphaseScene: KDTree");
#endif

		physics.SetBounds({}, window_size, BoundaryBehavior::ReflectVelocity);

		player = AddEntity(*this, window_size * 0.5f, player_size, color::Purple, false);
		player.SetDepth(1);

		player.Add<Scripts>().AddScript<E1Script>().entity = player;

		for (std::size_t i{ 0 }; i < entity_count; ++i) {
			AddEntity(
				*this, { rngx(), rngy() }, { rngsize(), rngsize() }, color::Green, FlipCoin()
			);
		}
	}

	CollisionSystem collision;

	void Update() override {
		V2_float sizea{ 50, 50 };

		MoveWASD(player.GetPosition(), V2_float{ 100.0f } * game.dt(), false);

		for (auto [e, tint] : EntitiesWith<Tint>()) {
			tint = color::Green;
		}

		player.SetTint(color::Purple);

		for (auto [e, transform, collider] : EntitiesWith<Transform, MyCollider>()) {
			auto size{ collider.rect.GetSize() };
			collider.rect = Rect{ transform.position - size, transform.position + size };
		}

		collision.Update(*this, game.dt());
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("BroadphaseScene", window_size);
	game.scene.Enter<BroadphaseScene>("");
	return 0;
}
*/

/*
struct Box;

struct EndPoint {
	float value{ 0.0f };
	bool isMin{ false };
	Box* box{ nullptr };
	EndPoint(float value_, bool isMin_);
};

struct AABB;

struct Box {
	std::array<EndPoint*, 2> minEndPoints;
	std::array<EndPoint*, 2> maxEndPoints;
	AABB* userData{ nullptr };

	Box(EndPoint* minX, EndPoint* minY, EndPoint* maxX, EndPoint* maxY);

	bool overlaps(Box* box) const {
		float l1X = minEndPoints[0]->value;
		float u1X = maxEndPoints[0]->value;
		float l1Y = minEndPoints[1]->value;
		float u1Y = maxEndPoints[1]->value;
		float l2X = box->minEndPoints[0]->value;
		float u2X = box->maxEndPoints[0]->value;
		float l2Y = box->minEndPoints[1]->value;
		float u2Y = box->maxEndPoints[1]->value;
		return !(l2X > u1X || u2X < l1X || u2Y < l1Y || l2Y > u1Y);
	}
};

EndPoint::EndPoint(float value_, bool isMin_) {
	box	  = nullptr;
	value = value_;
	isMin = isMin_;
}

Box::Box(EndPoint* minX, EndPoint* minY, EndPoint* maxX, EndPoint* maxY) {
	minEndPoints = { minX, minY };
	maxEndPoints = { maxX, maxY };
	userData	 = nullptr;
}

struct AABB {
	V2_float min;
	V2_float max;
	V2_float velocity;
	std::int64_t index = -1;
	Box* sapBox{ nullptr };

	AABB(V2_float min_, V2_float max_) : min(min_), max(max_), velocity(randomVelocity()) {}

private:
	static V2_float randomVelocity() {
		V2_float dir{ V2_float::Random(-0.5f, 0.5f) };
		float speed = 60.0f;

		if (dir.x != 0 || dir.y != 0) {
			return dir.Normalized() * speed;
		} else {
			return V2_float{ speed, 0.0f };
		}
	}
};

class SweepAndPrune {
public:
	std::vector<std::unique_ptr<Box>> boxes;
	std::array<std::vector<EndPoint*>, 2> endPoints;

	std::function<void(Box*, Box*)> onAdd;
	std::function<void(Box*, Box*)> onRemove;

	SweepAndPrune() {
		boxes.clear();
		endPoints = {};
		onAdd	  = [](Box*, Box*) {
		};
		onRemove = [](Box*, Box*) {
		};
	};

	Box* addObject(const V2_float& v0, const V2_float& v1, AABB* userData) {
		auto minX = new EndPoint(v0.x, true);
		auto maxX = new EndPoint(v1.x, false);
		auto minY = new EndPoint(v0.y, true);
		auto maxY = new EndPoint(v1.y, false);

		auto box	  = std::make_unique<Box>(minX, minY, maxX, maxY);
		box->userData = userData;
		minX->box = maxX->box = minY->box = maxY->box = box.get();

		Box* boxPtr = box.get();
		boxes.push_back(std::move(box));

		insertSorted(endPoints[0], minX, true);
		insertSorted(endPoints[0], maxX, false);
		endPoints[1].push_back(minY);
		endPoints[1].push_back(maxY);

		for (int axis = 0; axis < 2; ++axis) {
			sortFull(endPoints[axis]);
		}

		return boxPtr;
	}

	void updateObject(Box* box, V2_float v0, V2_float v1) {
		float newPos[2][2] = { { v0.x, v1.x }, { v0.y, v1.y } };
		for (int axis = 0; axis < 2; ++axis) {
			auto& axisVec = endPoints[axis];

			box->minEndPoints[axis]->value = newPos[axis][0];
			sortMinDown(axisVec, indexOf(axisVec, box->minEndPoints[axis]));

			box->maxEndPoints[axis]->value = newPos[axis][1];
			sortMaxUp(axisVec, indexOf(axisVec, box->maxEndPoints[axis]));

			sortMinUp(axisVec, indexOf(axisVec, box->minEndPoints[axis]));
			sortMaxDown(axisVec, indexOf(axisVec, box->maxEndPoints[axis]));
		}
	}

	void removeObject(Box* box) {
		box->minEndPoints[1]->value = std::numeric_limits<float>::max() - 1;
		box->maxEndPoints[1]->value = std::numeric_limits<float>::max();
		sortFull(endPoints[1]);

		boxes.erase(
			std::remove_if(
				boxes.begin(), boxes.end(),
				[box](const std::unique_ptr<Box>& b) { return b.get() == box; }
			),
			boxes.end()
		);

		for (int axis = 0; axis < 2; ++axis) {
			auto& axisVec = endPoints[axis];
			remove(axisVec, box->minEndPoints[axis]);
			remove(axisVec, box->maxEndPoints[axis]);
		}
	}

private:
	void insertSorted(std::vector<EndPoint*>& vec, EndPoint* ep, bool isMin) {
		auto it = vec.begin();
		while (it != vec.end() && (*it)->value < ep->value) {
			++it;
		}
		vec.insert(it, ep);
	}

	int indexOf(const std::vector<EndPoint*>& vec, EndPoint* value) {
		auto it = std::find(vec.begin(), vec.end(), value);
		return (it != vec.end()) ? (int)std::distance(vec.begin(), it) : -1;
	}

	void remove(std::vector<EndPoint*>& vec, EndPoint* value) {
		vec.erase(std::remove(vec.begin(), vec.end(), value), vec.end());
	}

	void sortFull(std::vector<EndPoint*>& axis) {
		for (auto j = 1; j < axis.size(); ++j) {
			EndPoint* keyElement = axis[j];
			float key			 = keyElement->value;
			int i				 = j - 1;
			while (i >= 0 && axis[i]->value > key) {
				EndPoint* swapper = axis[i];
				if (keyElement->isMin && !swapper->isMin &&
					swapper->box->overlaps(keyElement->box)) {
					onAdd(swapper->box, keyElement->box);
				} else if (!keyElement->isMin && swapper->isMin) {
					onRemove(swapper->box, keyElement->box);
				}
				axis[i + 1] = swapper;
				--i;
			}
			axis[i + 1] = keyElement;
		}
	}

	void sortMinDown(std::vector<EndPoint*>& axis, int j) {
		auto keyElement = axis[j];
		float key		= keyElement->value;
		int i			= j - 1;
		while (i >= 0 && axis[i]->value > key) {
			auto swapper = axis[i];
			if (keyElement->isMin && !swapper->isMin && swapper->box->overlaps(keyElement->box)) {
				onAdd(swapper->box, keyElement->box);
			}
			axis[i + 1] = swapper;
			--i;
		}
		axis[i + 1] = keyElement;
	}

	void sortMinUp(std::vector<EndPoint*>& axis, int j) {
		auto keyElement = axis[j];
		float key		= keyElement->value;
		int i			= j + 1;
		while (i < static_cast<int>(axis.size()) && axis[i]->value < key) {
			auto swapper = axis[i];
			if (keyElement->isMin && !swapper->isMin) {
				onRemove(swapper->box, keyElement->box);
			}
			axis[i - 1] = swapper;
			++i;
		}
		axis[i - 1] = keyElement;
	}

	void sortMaxDown(std::vector<EndPoint*>& axis, int j) {
		auto keyElement = axis[j];
		float key		= keyElement->value;
		int i			= j - 1;
		while (i >= 0 && axis[i]->value > key) {
			auto swapper = axis[i];
			if (!keyElement->isMin && swapper->isMin) {
				onRemove(swapper->box, keyElement->box);
			}
			axis[i + 1] = swapper;
			--i;
		}
		axis[i + 1] = keyElement;
	}

	void sortMaxUp(std::vector<EndPoint*>& axis, int j) {
		auto keyElement = axis[j];
		float key		= keyElement->value;
		int i			= j + 1;
		while (i < static_cast<int>(axis.size()) && axis[i]->value < key) {
			auto swapper = axis[i];
			if (!keyElement->isMin && swapper->isMin && swapper->box->overlaps(keyElement->box)) {
				onAdd(swapper->box, keyElement->box);
			}
			axis[i - 1] = swapper;
			++i;
		}
		axis[i - 1] = keyElement;
	}
};

void moveAABBs(
	std::vector<std::unique_ptr<AABB>>& aabbs, float deltaTimeSeconds, float canvasWidth,
	float movingPercent
) {
	auto movingCount = static_cast<int>(aabbs.size() * movingPercent / 100.0f);
	float boundary	 = canvasWidth;

	for (auto i = 0; i < movingCount; ++i) {
		auto aabb	   = aabbs[i].get();
		V2_float delta = aabb->velocity * deltaTimeSeconds;

		aabb->min = aabb->min + delta;
		aabb->max = aabb->max + delta;

		for (int dim = 0; dim < 2; ++dim) {
			float minVal = (dim == 0 ? aabb->min.x : aabb->min.y);
			float maxVal = (dim == 0 ? aabb->max.x : aabb->max.y);

			if (minVal < 0.0f || maxVal > boundary) {
				if (dim == 0) {
					aabb->velocity.x *= -1.0f;
				} else {
					aabb->velocity.y *= -1.0f;
				}
			}
		}
	}
}

void updateSAP(std::vector<std::unique_ptr<AABB>>& aabbs, SweepAndPrune& sap, float movingPercent) {
	auto movingCount = static_cast<int>(aabbs.size() * movingPercent / 100.0f);

	for (auto i = 0; i < movingCount; ++i) {
		auto aabb = aabbs[i].get();
		sap.updateObject(aabb->sapBox, aabb->min, aabb->max);
	}
}

void addAABB(
	std::vector<std::unique_ptr<AABB>>& aabbs, SweepAndPrune& sap, float size, float canvasWidth
) {
	auto getRandomPosition = [size, canvasWidth]() -> float {
		static std::random_device rd;
		static std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dist(size, canvasWidth - size);
		return dist(gen);
	};

	float x0 = getRandomPosition() - size;
	float y0 = getRandomPosition() - size;
	float x1 = x0 + size;
	float y1 = y0 + size;

	auto aabb	= std::make_unique<AABB>(V2_float{ x0, y0 }, V2_float{ x1, y1 });
	aabb->index = static_cast<int>(aabbs.size());

	// Register with SAP
	aabb->sapBox = sap.addObject(aabb->min, aabb->max, aabb.get());

	aabbs.push_back(std::move(aabb));
}

void removeAABB(std::vector<std::unique_ptr<AABB>>& aabbs, SweepAndPrune& sap) {
	auto& aabb = aabbs[aabbs.size() - 1];
	sap.removeObject(aabb->sapBox);
	aabbs.pop_back();
}

class SweepAndPruneScene : public Scene {
public:
	float movingPercent = 50.0f;

	float size = 20.0f;

	std::unordered_set<std::int64_t> pairs;

	std::vector<std::unique_ptr<AABB>> aabbs;

	SweepAndPrune sap;

	std::size_t entity_count{ 1000 };

	void Enter() {
		sap.onAdd = [&](Box* boxA, Box* boxB) {
			auto i = boxA->userData->index;
			auto j = boxB->userData->index;
			PTGN_ASSERT(i != j);
			if (i > j) {
				auto tmp = j;
				j		 = i;
				i		 = tmp;
			}
			pairs.insert((i << 16) | j);
		};
		sap.onRemove = [&](Box* boxA, Box* boxB) {
			auto i = boxA->userData->index;
			auto j = boxB->userData->index;
			if (i > j) {
				auto tmp = j;
				j		 = i;
				i		 = tmp;
			}
			pairs.erase((i << 16) | j);
		};
		for (auto i = 0; i < entity_count; i++) {
			addAABB(aabbs, sap, size, (float)window_size.x);
		}
	}

	void Update() {
		moveAABBs(aabbs, game.dt(), (float)window_size.x, movingPercent);
		updateSAP(aabbs, sap, movingPercent);

		for (auto& aabb : aabbs) {
			DrawDebugRect(aabb->min, aabb->max - aabb->min, color::Green, Origin::TopLeft, 1.0f);
		}
		for (auto& key : pairs) {
			auto i		= (key >> 16) & 0xffff;
			auto j		= key & 0xffff;
			auto& aabbi = aabbs[i];
			auto& aabbj = aabbs[j];
			DrawDebugLine(
				(aabbi->min + aabbi->max) / 2.0f, (aabbj->min + aabbj->max) / 2.0f, color::DarkRed,
				1.0f
			);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("SweepAndPruneScene", window_size);
	game.scene.Enter<SweepAndPruneScene>("");
	return 0;
}*/