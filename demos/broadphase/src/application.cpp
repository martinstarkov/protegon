#include <algorithm>
#include <list>
#include <new>
#include <unordered_map>
#include <vector>

#include "components/movement.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "math/geometry.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "physics/physics.h"
#include "rendering/graphics/rect.h"
#include "rendering/render_data.h"
#include "rendering/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 }; //{ 1280, 720 };

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
	AABB aabb;
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

	std::vector<Entity> Raycast(V2_float origin, V2_float dir) const {
		std::vector<Entity> hits;
		Raycast(root.get(), origin, dir, hits);
		return hits;
	}

	Entity RaycastFirst(V2_float origin, V2_float dir) const {
		Entity closest_hit{};
		float closest_t = 1.0f;
		RaycastFirst(root.get(), origin, dir, closest_t, closest_hit);
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

	void Raycast(const KDNode* node, V2_float origin, V2_float dir, std::vector<Entity>& result)
		const {
		if (!node) {
			return;
		}
		for (const auto& obj : node->objects) {
			if (obj.aabb.IntersectsRay(origin, dir)) {
				result.push_back(obj.entity);
			}
		}
		if (node->left) {
			Raycast(node->left.get(), origin, dir, result);
		}
		if (node->right) {
			Raycast(node->right.get(), origin, dir, result);
		}
	}

	void RaycastFirst(
		const KDNode* node, V2_float origin, V2_float dir, float& closest_t, Entity& closest_entity
	) const {
		if (!node) {
			return;
		}
		for (const auto& obj : node->objects) {
			if (obj.aabb.IntersectsRay(origin, dir, 0.0f, closest_t)) {
				closest_entity = obj.entity;
				closest_t	   = 0.0f; // not precise, but placeholder
			}
		}
		if (node->left) {
			RaycastFirst(node->left.get(), origin, dir, closest_t, closest_entity);
		}
		if (node->right) {
			RaycastFirst(node->right.get(), origin, dir, closest_t, closest_entity);
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
	auto position{ entity.GetPosition() };
	// TODO: Use collider size.
	auto half{ entity.Get<Rect>().size * 0.5f };
	auto center{ position + impl::GetOriginOffsetHalf(entity.GetOrigin(), half) };
	return { center - half, center + half };
}

Entity AddEntity(
	KDTree& tree, Scene& scene, const V2_float& center, const V2_float& size, const Color& color,
	bool induce_random_velocity = true
) {
	Entity entity = CreateRect(scene, center, size, color);
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
	AABB bounds{ GetBoundingVolume(entity) };
	tree.Insert(entity, bounds);
	return entity;
}

#define KDTREE 0

struct BroadphaseScene : public Scene {
	KDTree tree{ 200 };

	std::size_t entity_count{ 10000 };

	Entity player;
	V2_float player_size{ 20, 20 };

	RNG<float> rngx{ 0.0f, (float)window_size.x };
	RNG<float> rngy{ 0.0f, (float)window_size.y };
	RNG<float> rngsize{ 5.0f, 30.0f };

	void Enter() override {
		physics.SetBounds({}, window_size, BoundaryBehavior::ReflectVelocity);

		player = AddEntity(tree, *this, window_size * 0.5f, player_size, color::Purple, false);
		player.SetDepth(1);

		for (std::size_t i{ 0 }; i < entity_count; ++i) {
			AddEntity(
				tree, *this, { rngx(), rngy() }, { rngsize(), rngsize() }, color::Green, FlipCoin()
			);
		}
	}

	void Update() override {
		PTGN_PROFILE_FUNCTION();

		MoveWASD(player.GetPosition(), V2_float{ 100.0f } * game.dt(), false);

		for (auto [e, tint] : EntitiesWith<Tint>()) {
			tint = color::Green;
		}

		player.SetTint(color::Purple);

		auto player_volume{ GetBoundingVolume(player) };

#ifdef KDTREE

		if (KDTREE) {
			// Check only collisions with relevant k-d tree nodes.

			// TODO: Only update if player moved.
			tree.Update(player, GetBoundingVolume(player));

			for (auto [e, rect, rb] : EntitiesWith<Rect, RigidBody>()) {
				// TODO: Only update if entity moved.
				tree.Update(e, GetBoundingVolume(e));
			}
		} else {
			std::vector<Object> objects;
			objects.reserve(Size());

			for (auto [e, rect, rb] : EntitiesWith<Rect, RigidBody>()) {
				// TODO: Only update if entity moved.
				objects.emplace_back(e, GetBoundingVolume(e));
			}

			tree.Build(objects);
		}

		for (auto [e1, rect1] : EntitiesWith<Rect>()) {
			auto b1{ GetBoundingVolume(e1) };
			auto candidates = tree.Query(b1);
			for (auto& e2 : candidates) {
				if (e1 == e2) {
					continue;
				}
				if (b1.Intersects(GetBoundingVolume(e2))) {
					e1.SetTint(color::Red);
					e2.SetTint(color::Red);
				}
			}
		}
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