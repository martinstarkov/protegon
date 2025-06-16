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
#include "math/rng.h"
#include "math/vector2.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 1000, 1000 }; //{ 1280, 720 };

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
	AABB bounds;
	std::vector<Entity> objects;
	QuadtreeNode* children[4] = { nullptr, nullptr, nullptr, nullptr };
	int maxObjects			  = 4;
	int maxLevels			  = 5;
	int level;

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

void SpawnEnemies(
	Quadtree& tree, size_t count,
	RNG<float>& positionRNGX, // e.g., RNG<float> positionRNG{ 0.0f, 800.0f };
	RNG<float>& positionRNGY, // e.g., RNG<float> positionRNG{ 0.0f, 800.0f };
	RNG<float>& sizeRNG,	  // e.g., RNG<float> sizeRNG{ 10.0f, 40.0f };
	Scene& scene			  // Interface to create entities
) {
	for (size_t i = 0; i < count; ++i) {
		float width	 = sizeRNG();
		float height = sizeRNG();

		float x = positionRNGX();
		float y = positionRNGY();

		V2_float min = { x, y };
		V2_float max = { x + width, y + height };

		Entity enemy = scene.CreateEntity(); // Your ECS entity creation
		AABB box{ min, max };
		enemy.Add<AABB>(box);

		tree.insert(enemy);
	}
}

bool Overlaps(const AABB& a, const AABB& b) {
	return (a.min.x <= b.max.x && a.max.x >= b.min.x) && (a.min.y <= b.max.y && a.max.y >= b.min.y);
}

#define QUADTREE 1

struct BroadphaseScene : public Scene {
	Quadtree tree{ AABB{ V2_float{ 0, 0 }, V2_float{ 1000, 1000 } } };

	Entity player;
	V2_float playerSize{ 20, 20 };

	RNG<float> positionRNGX{ 0.0f, (float)window_size.x };
	RNG<float> positionRNGY{ 0.0f, (float)window_size.y };
	RNG<float> sizeRNG{ 5.0f, 30.0f };

	AABB computePlayerAABBFromPosition(Entity p) const {
		auto pos = p.GetPosition();
		return AABB{ pos - playerSize / 2.0f, pos + playerSize / 2.0f };
	}

	void Enter() override {
		player = CreateEntity();
		player.Add<Transform>();
		player.Add<AABB>(computePlayerAABBFromPosition(player));

		tree.insert(player);

		SpawnEnemies(tree, 100000, positionRNGX, positionRNGY, sizeRNG, *this);
	}

	void Update() override {
		MoveWASD(player.Get<Transform>().position, V2_float{ 100.0f } * game.dt(), false);
		// Update player's AABB component before updating the tree
		player.Get<AABB>() = computePlayerAABBFromPosition(player);

#ifdef QUADTREE
		// Update player's position in the quadtree (will re-insert if needed)
		tree.update(player);

		auto candidates = tree.retrieve(player.Get<AABB>());
#endif

		for (auto [e, aabb] : EntitiesWith<AABB>()) {
#ifdef QUADTREE
			if (e != player &&
				std::find(candidates.begin(), candidates.end(), e) != candidates.end() &&
				Overlaps(player.Get<AABB>(), aabb)) {
				// DrawDebugRect((aabb.min + aabb.max) / 2.0f, aabb.max - aabb.min, color::Red);
			} else {
				// DrawDebugRect((aabb.min + aabb.max) / 2.0f, aabb.max - aabb.min, color::Green);
			}
#else
			if (e == player) {
				continue;
			} else if (Overlaps(player.Get<AABB>(), aabb)) {
				// DrawDebugRect((aabb.min + aabb.max) / 2.0f, aabb.max - aabb.min, color::Red);
			} else {
				// DrawDebugRect((aabb.min + aabb.max) / 2.0f, aabb.max - aabb.min, color::Green);
			}
#endif
			// DrawDebugRect((aabb.min + aabb.max) / 2.0f, aabb.max - aabb.min, color::Green);
		}

		// DrawDebugRect(player.GetPosition(), playerSize, color::Purple);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("BroadphaseScene");
	game.scene.Enter<BroadphaseScene>("");
	return 0;
}