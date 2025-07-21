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
#include "rendering/batching/render_data.h"
#include "rendering/graphics/rect.h"
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

void SpawnEnemy(Quadtree& tree, Scene& scene, const V2_float& top_left, const V2_float& size) {
	Entity enemy = CreateRect(scene, top_left + size * 0.5f, size, color::Green);
	AABB box{ top_left, top_left + size };
	enemy.Add<AABB>(box);

	// auto vertices{ impl::GetQuadVertices(
	// 	impl::GetVertices(enemy.GetTransform(), size, Origin::Center), color::Green,
	// 	enemy.GetDepth(), 0.0f, impl::default_texture_coordinates
	// ) };
	// enemy.Add<QuadVertices>(vertices);
	tree.insert(enemy);
}

void SpawnEnemies(
	Quadtree& tree, size_t count,
	RNG<float>& positionRNGX, // e.g., RNG<float> positionRNG{ 0.0f, 800.0f };
	RNG<float>& positionRNGY, // e.g., RNG<float> positionRNG{ 0.0f, 800.0f };
	RNG<float>& sizeRNG,	  // e.g., RNG<float> sizeRNG{ 10.0f, 40.0f };
	Scene& scene			  // Interface to create entities
) {
	for (std::size_t i{ 0 }; i < count; ++i) {
		SpawnEnemy(tree, scene, { positionRNGX(), positionRNGY() }, { sizeRNG(), sizeRNG() });
	}
}

bool Overlaps(const AABB& a, const AABB& b) {
	return (a.min.x <= b.max.x && a.max.x >= b.min.x) && (a.min.y <= b.max.y && a.max.y >= b.min.y);
}

#define QUADTREE 1

struct BroadphaseScene : public Scene {
	Quadtree tree{ AABB{ V2_float{ 0, 0 }, V2_float{ window_size } } };

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
		player = CreateRect(*this, {}, playerSize, color::Purple);
		player.Add<AABB>(computePlayerAABBFromPosition(player));
		player.SetDepth(1);
		// auto vertices{ impl::GetQuadVertices(
		// 	impl::GetVertices(player.GetTransform(), playerSize, Origin::Center), color::Purple,
		// 	player.GetDepth(), 0.0f, impl::default_texture_coordinates
		// ) };
		// player.Add<QuadVertices>(vertices);

		tree.insert(player);

		SpawnEnemy(tree, *this, { 0, 0 }, { 200, 200 });
		SpawnEnemies(tree, 10000, positionRNGX, positionRNGY, sizeRNG, *this);
	}

	void Update() override {
		PTGN_PROFILE_FUNCTION();

		MoveWASD(player.GetPosition(), V2_float{ 100.0f } * game.dt(), false);
		// Update player's AABB component before updating the tree
		player.Get<AABB>() = computePlayerAABBFromPosition(player);
		// auto vertices{ impl::GetQuadVertices(
		// 	impl::GetVertices(player.GetTransform(), playerSize, Origin::Center), color::Purple,
		// 	player.GetDepth(), 0.0f, impl::default_texture_coordinates
		// ) };
		// player.Get<QuadVertices>() = vertices;

		for (auto [e, tint] : EntitiesWith<Tint>()) {
			tint = color::Green;
		}

		player.SetTint(color::Purple);

#ifdef QUADTREE
		// Update player's position in the quadtree (will re-insert if needed)
		tree.update(player);

		auto candidates = tree.retrieve(player.Get<AABB>());
		// PTGN_LOG("Candidates: ", candidates.size());

		for (auto candidate : candidates) {
			if (candidate == player) {
				continue;
			}
			if (Overlaps(player.Get<AABB>(), candidate.Get<AABB>())) {
				candidate.SetTint(color::Red);
			}
		}
#else
		for (auto [e, aabb] : EntitiesWith<AABB>()) {
			if (e == player) {
				continue;
			}
			if (Overlaps(player.Get<AABB>(), aabb)) {
				e.SetTint(color::Red);
			} else {
				e.SetTint(color::Green);
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

*/

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
}