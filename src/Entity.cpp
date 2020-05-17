#include "Entity.h"
#include "Game.h"
#include <algorithm>
#include <limits>
#include <vector>
#include <map>
#include <tuple>

#define COLLISION_DELTA 0.1f
#define EPSILON 0.001f

void Entity::update() {
	updateMotion();
	collisionCheck();
}

void Entity::updateMotion() {
	velocity *= (1.0f - DRAG); // drag
	velocity += acceleration; // gravity and movement acceleration
	gravity = false;
	if (gravity) {
		g = GRAVITY;
		velocity.y += g;
	}
	terminalMotion();
}
template <typename T> static int sgn(T val) {
	return (T(0) < val) - (val < T(0));
}
void Entity::terminalMotion() {
	if (abs(velocity.x) >= terminalVelocity.x) {
		velocity.x = terminalVelocity.x * sgn(velocity.x);
	}
	if (abs(velocity.y) >= terminalVelocity.y) {
		velocity.y = terminalVelocity.y * sgn(velocity.y);
	}
	const float threshold = 0.1f;
	if (abs(velocity.x) < threshold) {
		velocity.x = 0;
	}
	if (abs(velocity.y) < threshold) {
		velocity.y = 0;
	}
}
void Entity::boundaryCheck(AABB& hb) {
	if (hb.pos.x < 0) {
		hb.pos.x = 0;
	}
	if (hb.pos.x + hb.size.x > WINDOW_WIDTH) {
		hb.pos.x = WINDOW_WIDTH - hb.size.x;
	}
	if (hb.pos.y < 0) {
		hb.pos.y = 0;
		//velocity.y *= -1 / 2;
		//acceleration.y *= -1 / 10;
	}
	if (hb.pos.y + hb.size.y > WINDOW_HEIGHT) {
		hb.pos.y = WINDOW_HEIGHT - hb.size.y;
		//hitGround();
	}
}
AABB Entity::broadphaseBox(AABB a, Vec2D vel) {
	Vec2D bPos;
	bPos.x = vel.x > 0 ? a.pos.x : a.pos.x + vel.x;
	bPos.y = vel.y > 0 ? a.pos.y : a.pos.y + vel.y;
	Vec2D bSize = a.size + abs(vel);
	return AABB(bPos, bSize);
}
AABB Entity::maximumBroadphaseBox(AABB a, Vec2D terminalVelocity) {
	Vec2D extremeMinimum = a.min() - abs(terminalVelocity);
	Vec2D extremeMaximum = a.max() + abs(terminalVelocity);
	Vec2D bPos = extremeMinimum;
	Vec2D bSize = abs(extremeMaximum - extremeMinimum);
	return AABB(bPos, bSize);
}
bool Entity::equalOverlapAABBvsAABB(AABB a, AABB b) {
	// Exit with no intersection if separated along an axis
	if (a.max()[0] < b.min()[0] || a.min()[0] > b.max()[0]) return false;
	if (a.max()[1] < b.min()[1] || a.min()[1] > b.max()[1]) return false;
	// Overlapping on all axes means AABBs are intersecting
	return true;
}
bool Entity::overlapAABBvsAABB(AABB a, AABB b) {
	// Exit with no intersection if separated along an axis
	if (a.max()[0] <= b.min()[0] || a.min()[0] >= b.max()[0]) return false;
	if (a.max()[1] <= b.min()[1] || a.min()[1] >= b.max()[1]) return false;
	// Overlapping on all axes means AABBs are intersecting
	return true;
}
bool Entity::axisOverlapAABB(AABB a, AABB b, Axis axis) {
	if (a.max()[int(axis)] <= b.min()[int(axis)] || a.min()[int(axis)] >= b.max()[int(axis)]) return false;
	return true;
}

// Intersect AABBs ‘a’ and ‘b’ moving with constant velocities va and vb.
// On intersection, return time of first and last contact in tfirst and tlast
bool Entity::sweepAABBvsAABB(AABB a, AABB b, Vec2D va, Vec2D vb, float& tfirst, float& tlast, float& xfirst, float& yfirst) {
	// Exit early if ‘a’ and ‘b’ initially overlapping
	//if (overlapAABBvsAABB(a, b)) {
	//	tfirst = tlast = 0.0f;
	//	return true;
	//}
	// Use relative velocity; effectively treating ’b’ as stationary
	Vec2D v = vb - va;
	// Initialize times of first and last contact
	tfirst = xfirst = yfirst = 0.0f;
	tlast = 1.0f;
	// For each axis, determine times of first and last contact, if any
	for (int i = 0; i < 2; i++) {
		if (v[i] < 0.0f) {
			if (b.max()[i] < a.min()[i]) {
				return false; // Nonintersecting and moving apart
			}
			if (a.max()[i] < b.min()[i]) { 
				tfirst = std::max((a.max()[i] - b.min()[i]) / v[i], tfirst);
				if (i == 0) {
					xfirst = std::max((a.max()[i] - b.min()[i]) / v[i], xfirst);
				} else {
					yfirst = std::max((a.max()[i] - b.min()[i]) / v[i], yfirst);
				}
			}
			if (b.max()[i] > a.min()[i]) {
				tlast = std::min((a.min()[i] - b.max()[i]) / v[i], tlast);
			}
		}
		if (v[i] > 0.0f) {
			if (b.min()[i] > a.max()[i]) {
				return false; // Nonintersecting and moving apart
			}
			if (b.max()[i] < a.min()[i]) {
				tfirst = std::max((a.min()[i] - b.max()[i]) / v[i], tfirst);
				if (i == 0) {
					xfirst = std::max((a.min()[i] - b.max()[i]) / v[i], xfirst);
				} else {
					yfirst = std::max((a.min()[i] - b.max()[i]) / v[i], yfirst);
				}
			}
			if (a.max()[i] > b.min()[i]) {
				tlast = std::min((a.max()[i] - b.min()[i]) / v[i], tlast);
			}
		}
		// No overlap possible if time of first contact occurs after time of last contact
		if (tfirst > tlast) {
			return false;
		}
	}
	return true;
}
float Entity::sweepAABB(AABB b1, AABB b2, Vec2D v1, Vec2D v2, float& xEntryTime, float& yEntryTime, float& xExitTime, float& yExitTime, float& exit) {
	float xInvEntry, yInvEntry;
	float xInvExit, yInvExit;
	Vec2D rv = v1 - v2;
	// find the distance between the objects on the near and far sides for both x and y 
	if (rv.x > 0.0f) {
		xInvEntry = b2.min().x - b1.max().x;
		xInvExit = b2.max().x - b1.min().x;
	} else {
		xInvEntry = b2.max().x - b1.min().x;
		xInvExit = b2.min().x - b1.max().x;
	}

	if (rv.y > 0.0f) {
		yInvEntry = b2.min().y - b1.max().y;
		yInvExit = b2.max().y - b1.min().y;
	} else {
		yInvEntry = b2.max().y - b1.min().y;
		yInvExit = b2.min().y - b1.max().y;
	}

	// find time of collision and time of leaving for each axis (if statement is to prevent divide by zero) 
	float xEntry, yEntry;
	float xExit, yExit;

	if (rv.x == 0.0f) {
		xEntry = -std::numeric_limits<float>::infinity();
		xExit = std::numeric_limits<float>::infinity();
	} else {
		xEntry = xInvEntry / rv.x;
		xExit = xInvExit / rv.x;
	}

	if (rv.y == 0.0f) {
		yEntry = -std::numeric_limits<float>::infinity();
		yExit = std::numeric_limits<float>::infinity();
	} else {
		yEntry = yInvEntry / rv.y;
		yExit = yInvExit / rv.y;
	}


	if (xEntry > 1.0f) {
		xEntry = -std::numeric_limits<float>::infinity();
	}
	if (yEntry > 1.0f) {
		yEntry = -std::numeric_limits<float>::infinity();
	}
	if (yEntry > 1.0f) yEntry = -FLT_MAX;
	if (xEntry > 1.0f) xEntry = -FLT_MAX;

	xEntryTime = xEntry;
	yEntryTime = yEntry;
	xExitTime = xExit;
	yExitTime = yExit;
	// find the earliest/latest times of collision
	float entryTime = std::max(xEntry, yEntry);
	float exitTime = std::min(xExit, yExit);
	 //there was no collision
	if (entryTime > exitTime) return 1.0f; // This check was correct.
	if (xEntry < 0.0f && yEntry < 0.0f) return 1.0f;
	if (xEntry < 0.0f) {
		// Check that the bounding box started overlapped or not.
		if (b2.max().x < b1.min().x || b2.min().x > b1.max().x) return 1.0f;
	}
	if (yEntry < 0.0f) {
		// Check that the bounding box started overlapped or not.
		if (b2.max().y < b1.min().y || b2.min().y > b1.max().y) return 1.0f;
	}

	// if there was no collision
	if (entryTime > exitTime || xEntry < 0.0f && yEntry < 0.0f || xEntry > 1.0f || yEntry > 1.0f) {
		return 1.0f;
	}
	exit = exitTime;
	return entryTime;
}

void Entity::collisionCheck() {
	bool grounded = true;
	AABB newHitbox = hitbox;
	Vec2D newVelocity = velocity;
	color = originalColor;
	if (id == 0) {

		std::vector<Entity*> potentialColliders;
		for (Entity* e : Game::entities) {
			if (e != this) {
				AABB b = maximumBroadphaseBox(newHitbox, terminalVelocity);
				//Game::broadphase.push_back(b);
				if (equalOverlapAABBvsAABB(b, e->getHitbox())) {
					potentialColliders.push_back(e);
				}
			}
		}
		/* Static collision check here */
		//for (Entity* e : potentialColliders) {
		//	if (e != this) {
		//		if (equalOverlapAABBvsAABB(broadphaseBox(newHitbox, velocity), e->getHitbox())) {
		//			AABB md = newHitbox.minkowskiDifference(e->getHitbox());
		//			if (md.pos.x < 0 &&
		//				md.max().x > 0 &&
		//				md.pos.y < 0 &&
		//				md.max().y > 0) {
		//				Vec2D pv = md.closestPointOnBoundsToPoint();
		//				if (pv.x) {
		//					newHitbox.pos.x -= pv.x;// + sgn(pv.x) / abs(pv.x) * COLLISION_DELTA;
		//					//std::cout << "Collided with " << e->getId() << "by X: " << pv.x << std::endl;
		//				}
		//			}
		//		}
		//	}
		//}
		//for (Entity* e : potentialColliders) {
		//	if (e != this) {
		//		if (equalOverlapAABBvsAABB(broadphaseBox(newHitbox, velocity), e->getHitbox())) {
		//			AABB md = newHitbox.minkowskiDifference(e->getHitbox());
		//			if (md.pos.x < 0 &&
		//				md.max().x > 0 &&
		//				md.pos.y < 0 &&
		//				md.max().y > 0) {
		//				Vec2D pv = md.closestPointOnBoundsToPoint();
		//				if (pv.y) {
		//					newHitbox.pos.y -= pv.y;// + sgn(pv.y) / abs(pv.y) * COLLISION_DELTA;
		//					//std::cout << "Collided with " << e->getId() << "by Y: " << pv.y << std::endl;
		//				}
		//			}
		//		}
		//	}
		//}
		//std::cout << "potentialColliders:" << potentialColliders.size() << ",";
		std::vector<std::tuple<float, Vec2D, Entity*>> firstSweepTimes;
		std::vector<Vec2D> normals;
		Vec2D collisionNormal = Vec2D();
		//std::cout << std::endl;
		bool individualCorner = false;
		float firstCollisionTime = 1.0f;
		if (velocity) {
			for (Entity* e : potentialColliders) {
				if (e != this) {
					AABB b = broadphaseBox(newHitbox, velocity);
					//Game::broadphase.push_back(b);
					if (overlapAABBvsAABB(b, e->getHitbox())) {
						e->setColor({ 255, 0, 0, 255 });
						float firstSweepTime, xSweepTime, ySweepTime, lastSweepTime;
						sweepAABBvsAABB(newHitbox, e->getHitbox(), velocity, e->getVelocity(), firstSweepTime, lastSweepTime, xSweepTime, ySweepTime);
						if (firstSweepTime < 1.0f) {
							firstSweepTimes.push_back({ firstSweepTime, Vec2D(xSweepTime, ySweepTime), e });
							normals.push_back(newHitbox.minkowskiDifference(e->getHitbox()).penetrationNormal());
						}
					}
				}
			}
		}
		//std::cout << "potentialFirstSweeps:" << firstSweepTimes.size() << ",";
		//// normal detection
		//Vec2D collisionNormal = Vec2D();
		//int empty = 0;
		//for (auto n : normals) {
		//	if (n) {
		//		//std::cout << n << ",";
		//		collisionNormal += n;
		//	} else {
		//		empty++;
		//	}
		//}
		//collisionNormal /= float(int(normals.size()) - empty);
		////std::cout << " --- avg: " << collisionNormal;
		//// corner detection
		//bool isIntegerX = abs(collisionNormal.x) == 0.0f || abs(collisionNormal.x) == 1.0f;
		//bool isIntegerY = abs(collisionNormal.y) == 0.0f || abs(collisionNormal.y) == 1.0f;
		//if (!isIntegerX && isIntegerY) {
		//	collisionNormal = Vec2D(0.0f, collisionNormal.y);
		//}
		//if (isIntegerX && !isIntegerY) {
		//	collisionNormal = Vec2D(collisionNormal.x, 0.0f);
		//}
		//collisionNormal = collisionNormal.identityVector();
		Axis firstAxis = Axis::NEITHER, secondAxis = Axis::NEITHER;
		if (firstSweepTimes.size() > 0) {
			sort(firstSweepTimes.begin(), firstSweepTimes.end());
			auto t = *firstSweepTimes.begin();
			firstCollisionTime = std::get<0>(t);
			Vec2D sweepAxisTime = std::get<1>(t);
			Entity* collider = std::get<2>(t);
			if (sweepAxisTime == firstCollisionTime) {
				firstAxis = Axis::BOTH;
				secondAxis = Axis::BOTH;
			} else if (sweepAxisTime.x == firstCollisionTime) {
				firstAxis = Axis::HORIZONTAL;
				secondAxis = Axis::VERTICAL;
				//std::cout << "Sweeping X-Axis first" << std::endl;
			} else if (sweepAxisTime.y == firstCollisionTime) {
				firstAxis = Axis::VERTICAL;
				secondAxis = Axis::HORIZONTAL;
				//std::cout << "Sweeping Y-Axis first" << std::endl;
			}

			if (firstCollisionTime) { // first collision, firstCollisionTime != 0
				if (firstAxis != Axis::BOTH && secondAxis != Axis::BOTH) {
					AABB interpolatedHitbox = newHitbox;
					interpolatedHitbox.pos[int(firstAxis)] += newVelocity[int(firstAxis)] * firstCollisionTime;
					int fm = interpolatedHitbox.matchingCoordinates(collider->getHitbox(), firstAxis);
					int sm = interpolatedHitbox.matchingCoordinates(collider->getHitbox(), secondAxis);
					std::cout << "normals: "<< normals.size() <<", axis:  " << int(firstAxis) << ":" << fm << ", axis: " << int(secondAxis) << ":" << sm << std::endl;
					if (!sm && normals.size() < 3) {
						std::cout << "Corner" << std::endl;
					}
				}
				//if (normals.size() == 1) { // deal with fake corners
				//	//std::cout << "Individual corner sweep at " << normals[0] << std::endl;
				//	// remember to sweep again here, but for now just move regualar
				//	newHitbox.pos += newVelocity;

				//} else if (normals.size() == 2) { // regular sweep hit
				//	//std::cout << "Regular first sweep, normals: " << normals.size() << std::endl;
				//	//std::cout << "Regular first sweep, normals: " << normals[0] << normals[1] << std::endl;
				//	//newHitbox.pos += velocity * firstCollisionTime;
				//	//boundaryCheck(newHitbox);
				//}
			} else { // continuous collision, firstCollisionTime = 0
				//std::cout << "Normals on continuous sweep: " << normals.size() << std::endl;

			}
			//std::cout << "std::get<1>(firstColliderTuple): " << std::get<1>(firstColliderTuple) << std::endl;
			//if (firstCollisionTime > 0.0f) { // not currently colliding with a wall
			//	if (std::get<1>(firstColliderTuple).x == firstCollisionTime) {// x-axis collides first
			//		if (velocity.x > 0) {
			//			collisionNormal.x = -1;
			//		} else if (velocity.x < 0) {
			//			collisionNormal.x = 1;
			//		} else {
			//			collisionNormal.x = 0;
			//		}
			//	} else if (std::get<1>(firstColliderTuple).y == firstCollisionTime) { // y-axis collides first
			//		if (velocity.y > 0) {
			//			collisionNormal.y = -1;
			//		} else if (velocity.y < 0) {
			//			collisionNormal.y = 1;
			//		} else {
			//			collisionNormal.y = 0;
			//		}
			//	}
			//	//std::cout << "First collision, times: " << std::get<1>(firstColliderTuple) << ", normal: " << cornerNormal << ",";
			//}
			if (normals.size() > 0) {
				int empty = 0;
				for (auto n : normals) {
					if (n) {
						collisionNormal += n;
					} else {
						empty++;
					}
					//std::cout << n << ",";
				}
				collisionNormal /= (float)(int(normals.size()) - empty);
				//std::cout << " --- avg: " << collisionNormal;
				// corner detection
				bool isIntegerX = abs(collisionNormal.x) == 0.0f || abs(collisionNormal.x) == 1.0f;
				bool isIntegerY = abs(collisionNormal.y) == 0.0f || abs(collisionNormal.y) == 1.0f;
				if (!isIntegerX && isIntegerY) {
					collisionNormal = Vec2D(0.0f, collisionNormal.y);
				}
				if (isIntegerX && !isIntegerY) {
					collisionNormal = Vec2D(collisionNormal.x, 0.0f);
				}
				collisionNormal = collisionNormal.identityVector();
				//std::cout << " --- final: " << collisionNormal << std::endl;
				//std::cout << ", collisionNormal: " << collisionNormal << ",";
			}
			if (normals.size() == 1 && collisionNormal.nonZero()) {
				individualCorner = true;
			}

			Axis freeAxis, currentAxis;
			if (!abs(collisionNormal.x)) {
				currentAxis = Axis::VERTICAL;
				freeAxis = Axis::HORIZONTAL;
			} else if (!abs(collisionNormal.y)) {
				currentAxis = Axis::HORIZONTAL;
				freeAxis = Axis::VERTICAL;
			} else {
				freeAxis = Axis::NEITHER;
				currentAxis = Axis::BOTH;
			}
			//std::cout << "velocity: " << velocity << ",";
			//std::cout << "pos before first sweep:" << newHitbox.pos << ",";
			if (!individualCorner) {
				newHitbox.pos += velocity * firstCollisionTime;
				boundaryCheck(newHitbox);
			}
			//std::cout << "pos after first sweep:" << newHitbox.pos << ",";
			//std::cout << "axis of second sweep:" << int(freeAxis) << ",";
			Vec2D tangent = collisionNormal.tangent();
			float dotProduct = velocity.dotProduct(tangent);
			//std::cout << "collisionNormal:" << collisionNormal << "," << "velocity:" << velocity;
			if (dotProduct && !collisionNormal.nonZero()) {
				dotProduct /= abs(dotProduct); // normalize
			} else { // for corners, keep newVelocity as zero vector
				dotProduct = 0;
			}

			//std::cout << "newvel:" << newVelocity << ",";


			if (freeAxis != Axis::NEITHER || individualCorner) { // there exists a free axis, i.e. not a corner collision, sweep again
				if (individualCorner) {
					//std::cout << "collisionNormal: " << collisionNormal << ",";
					freeAxis = currentAxis;
					if (velocity.x) {
						freeAxis = Axis::HORIZONTAL;
					}
					if (velocity.y) {
						freeAxis = Axis::VERTICAL;
					}
					//std::cout << "free axis will be: " << int(freeAxis) << ",";

				}
				newVelocity = tangent * velocity.magnitude() * dotProduct;// * (1.0f - firstCollisionTime);

				//std::cout << "newVelocity:" << newVelocity << ",";

				std::vector<std::tuple<float, Vec2D, Entity*>> secondSweepTimes;
				for (Entity* e : potentialColliders) {
					if (e != this) {
						AABB b = broadphaseBox(newHitbox, newVelocity);
						//Game::broadphase.push_back(b);
						if (equalOverlapAABBvsAABB(b, e->getHitbox())) {
							//e->setColor({ 255, 0, 0, 255 });
							float secondSweepTime, xSweepTime, ySweepTime, lastSweepTime;
							sweepAABBvsAABB(newHitbox, e->getHitbox(), newVelocity, e->getVelocity(), secondSweepTime, lastSweepTime, xSweepTime, ySweepTime);
							if (secondSweepTime < 1.0f && secondSweepTime > 0.0f) {
								secondSweepTimes.push_back({ secondSweepTime, Vec2D(xSweepTime, ySweepTime), e });
							}
						}
					}
				}
				//std::cout << "potentialSecondSweeps:" << secondSweepTimes.size() << ",";
				if (secondSweepTimes.size() > 0) {
					float secondCollisionTime = 1.0f;
					for (auto t : secondSweepTimes) {
						float time = std::get<1>(t)[int(freeAxis)];
						if (std::get<0>(t) == time) {
							if (time < secondCollisionTime) {
								secondCollisionTime = time;
							}
						}
						//std::cout << time << ",";
					}
					if (individualCorner) {
						//newHitbox.pos[int(freeAxis)] += velocity[int(freeAxis)];
						//std::cout << "MOVING FULL 1 LENGTH: " << velocity << std::endl;
					} else {
						//std::cout << std::endl;
						newHitbox.pos[int(freeAxis)] += newVelocity[int(freeAxis)] * secondCollisionTime;
						//std::cout << "axis:" << int(freeAxis) << "," << newHitbox.pos[int(freeAxis)] << "+" << newVelocity[int(freeAxis)] << "*" << secondCollisionTime << std::endl;
					}
					newVelocity[int(freeAxis)] -= newVelocity[int(freeAxis)] * secondCollisionTime;
				} else {
					if (individualCorner) {
						//newHitbox.pos[int(freeAxis)] += velocity[int(freeAxis)];
						//std::cout << "MOVING FULL 2 LENGTH: " << velocity << std::endl;
					} else {
						newHitbox.pos[int(freeAxis)] += newVelocity[int(freeAxis)];
					}
					newVelocity = Vec2D();
					//std::cout << newHitbox.pos[int(freeAxis)] << "+" << newVelocity[int(freeAxis)] << std::endl;
				}


				boundaryCheck(newHitbox);
				//newVelocity[int(freeAxis)] = velocity[int(freeAxis)];

			} else {
				Vec2D movementDirection = velocity.identityVector();
				if (sgn(collisionNormal.x) != sgn(movementDirection.x)) {
					newVelocity.x = 0;
				}
				if (sgn(collisionNormal.y) != sgn(movementDirection.y)) {
					newVelocity.y = 0;
				}
				newHitbox.pos += newVelocity;
				//std::cout << "movementDirection: " << movementDirection << "," << "collisionNormal: " << collisionNormal;
				//std::cout << std::endl;
			}
		} else {
		newHitbox.pos += velocity;
		}
		//std::cout << "d:" << dotProduct << ",";
		//std::cout << "cn:" << collisionNormal << ",";
		//std::cout << std::endl;
		boundaryCheck(newHitbox);
		if (!individualCorner) {
			velocity = newVelocity;
		}
		terminalMotion();
		//std::cout << "v:" << velocity << std::endl;
		//float firstSweepTime = 1.0f;
		//std::map<float, Vec2D> collisionTimes;
		//std::map<float, Vec2D> exits;
		//std::vector<Vec2D> normals;
		//for (Entity* e : potentialCollisions) {
		//	e->setColor({ 255, 0, 0, 255 });
		//	float xEntryTime = 1.0f;
		//	float yEntryTime = 1.0f;
		//	float xExitTime = 0.0f;
		//	float yExitTime = 0.0f;
		//	float exit = 0.0f;
		//	float collisionTime = sweepAABB(newHitbox, e->getHitbox(), velocity, e->getVelocity(), xEntryTime, yEntryTime, xExitTime, yExitTime, exit);
		//	if (collisionTime < 1.0f) {
		//		collisionTimes.insert({ abs(collisionTime), Vec2D(xEntryTime, yEntryTime) });
		//		normals.push_back(newHitbox.minkowskiDifference(e->getHitbox()).penetrationNormal());
		//	}
		//}
		//if (collisionTimes.size() > 0 && normals.size() > 0) {

		//	// normal detection
		//	Vec2D collisionNormal = Vec2D();
		//	int empty = 0;
		//	for (auto n : normals) {
		//		if (n) {
		//			//std::cout << n << ",";
		//			collisionNormal += n;
		//		} else {
		//			empty++;
		//		}
		//	}
		//	collisionNormal /= float(int(normals.size()) - empty);
		//	//std::cout << " --- avg: " << collisionNormal;
		//	bool isIntegerX = abs(collisionNormal.x) == 0.0f || abs(collisionNormal.x) == 1.0f;
		//	bool isIntegerY = abs(collisionNormal.y) == 0.0f || abs(collisionNormal.y) == 1.0f;
		//	if (!isIntegerX && isIntegerY) {
		//		collisionNormal = Vec2D(0.0f, collisionNormal.y);
		//	}
		//	if (isIntegerX && !isIntegerY) {
		//		collisionNormal = Vec2D(collisionNormal.x, 0.0f);
		//	}
		//	collisionNormal = collisionNormal.identityVector();

		//	float collisionTime = (*collisionTimes.begin()).first;
		//	float dotproduct = 0.0f;
		//	if (!collisionNormal.x) {
		//		if (velocity.x > 0.0f) {
		//			dotproduct = -1.0f;
		//		} else if (velocity.x < 0.0f) {
		//			dotproduct = 1.0f;
		//		}
		//	}
		//	if (!collisionNormal.y) {
		//		if (velocity.y > 0.0f) {
		//			dotproduct = -1.0f;
		//		} else if (velocity.y < 0.0f) {
		//			dotproduct = 1.0f;
		//		}
		//	}
		//	Vec2D tangent = collisionNormal.tangent() * dotproduct;
		//	Vec2D newVelocity = tangent * velocity.magnitude() * (1.0f - collisionTime);
		//	float xCollisionTime = 1.0f;
		//	float yCollisionTime = 1.0f;
		//	if (!collisionTime) {
		//		std::cout << "[" << velocity << "]";
		//		std::vector<float> xRepresentatives, yRepresentatives;
		//		for (auto e : collisionTimes) {
		//			std::cout << "{" << e.first << ":" << e.second << "}";
		//			if (e.first == e.second.x) {
		//				if (e.second.x) {
		//					xRepresentatives.push_back(e.second.x);
		//				} else {
		//					xCollisionTime = 0.0f;
		//				}
		//			}
		//			if (e.first == e.second.y) {
		//				if (e.second.y) {
		//					yRepresentatives.push_back(e.second.y);
		//				} else {
		//					yCollisionTime = 0.0f;
		//				}
		//			}
		//		}
		//		std::cout << std::endl;
		//		if (xRepresentatives.size() > 0) {
		//			sort(xRepresentatives.begin(), xRepresentatives.end());
		//			xCollisionTime = *xRepresentatives.begin();
		//		}
		//		if (yRepresentatives.size() > 0) {
		//			sort(yRepresentatives.begin(), yRepresentatives.end());
		//			yCollisionTime = *yRepresentatives.begin();
		//		}
		//		if (!newVelocity.y && newVelocity.x) {
		//			newHitbox.pos.x += velocity.x * xCollisionTime;
		//		}
		//		if (!newVelocity.x && newVelocity.y) {
		//			newHitbox.pos.y += velocity.y * yCollisionTime;
		//		}
		//		if (!newVelocity.x && !newVelocity.y) {
		//		}
		//	} else {
		//		newHitbox.pos += velocity * collisionTime;
		//	}
		//	velocity = newVelocity;
		//} else {
		//	newHitbox.pos += velocity;
		//}
	}
	hitbox = newHitbox;
}

void Entity::clearColliders() {
}

void Entity::hitGround() {
	grounded = true;
}

Entity* Entity::collided(Side direction) {
	//switch (direction) {
	//	case Side::BOTTOM:
	//	case Side::TOP:
	//		if (yCollisions.size() > 0) {
	//			for (auto c : yCollisions) {
	//				if (c.second.y > 0 && direction == Side::BOTTOM) {
	//					return c.first;
	//				}
	//				if (c.second.y < 0 && direction == Side::TOP) {
	//					return c.first;
	//				}
	//			}
	//		}
	//		break;
	//	case Side::LEFT:
	//	case Side::RIGHT:
	//		if (xCollisions.size() > 0) {
	//			for (auto c : xCollisions) {
	//				if (c.second.x > 0 && direction == Side::RIGHT) {
	//					return c.first;
	//				}
	//				if (c.second.x < 0 && direction == Side::LEFT) {
	//					return c.first;
	//				}
	//			}
	//		}
	//		break;
	//	case Side::ANY:
	//		if (yCollisions.size() > 0) {
	//			return yCollisions[0].first;
	//		}
	//		if (xCollisions.size() > 0) {
	//			return xCollisions[0].first;
	//		}
	//	default:
	//		break;
	//}
	//return nullptr;
	return 0;
}

void Entity::reset() {
	acceleration = velocity = {};
	hitbox.pos = originalPos;
	gravity = falling;
	g = GRAVITY;
	color = originalColor;
}

void Entity::accelerate(Axis direction, float movementAccel) {
	switch (direction) {
		case Axis::VERTICAL:
			acceleration.y = movementAccel;
			break;
		case Axis::HORIZONTAL:
			acceleration.x = movementAccel;
			break;
		case Axis::BOTH:
			acceleration = Vec2D(movementAccel, movementAccel);
		default:
			break;
	}
}

void Entity::stop(Axis direction) {
	accelerate(direction, 0.0f);
	//switch (direction) {
	//	case Axis::VERTICAL:
	//		velocity.y = 0.0f;
	//		break;
	//	case Axis::HORIZONTAL:
	//		velocity.x = 0.0f;
	//		break;
	//	case Axis::BOTH:
	//		velocity = Vec2D();
	//	default:
	//		break;
	//}
}