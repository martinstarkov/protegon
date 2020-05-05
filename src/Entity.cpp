#include "Entity.h"
#include <algorithm>
#include <limits>

void Entity::update() {
	//p.clear();
	//p.push_back(pos);
	//p.push_back(Vec2D(pos.x + size.x, pos.y));
	//p.push_back(Vec2D(pos.x, pos.y + size.y));
	//p.push_back(Vec2D(pos.x + size.x, pos.y + size.y));
}

bool Entity::checkCollision(Entity* entity) { // AABB - AABB collision
    // Collision x-axis?
    bool collisionX = pos.x + size.x >= entity->getPosition().x &&
        entity->getPosition().x + entity->getSize().x >= pos.x;
    // Collision y-axis?
    bool collisionY = pos.y + size.y >= entity->getPosition().y &&
        entity->getPosition().y + entity->getSize().y >= pos.y;
    // Collision only if on both axes
    return collisionX && collisionY;
}

float Entity::sweptAABB(Entity* entity, Vec2D& normal) {
    float xInvEntry, yInvEntry;
    float xInvExit, yInvExit;

    // find the distance between the objects on the near and far sides for both x and y 
    if (vel.x > 0.0f) {
        xInvEntry = entity->getPosition().x - (pos.x + size.x);
        xInvExit = (entity->getPosition().x + entity->getSize().x) - pos.x;
    } else {
        xInvEntry = (entity->getPosition().x + entity->getSize().x) - pos.x;
        xInvExit = entity->getPosition().x - (pos.x + size.x);
    }

    if (vel.y > 0.0f) {
        yInvEntry = entity->getPosition().y - (pos.y + size.y);
        yInvExit = (entity->getPosition().y + entity->getSize().y) - pos.y;
    } else {
        yInvEntry = (entity->getPosition().y + entity->getSize().y) - pos.y;
        yInvExit = entity->getPosition().y - (pos.y + size.y);
    }

    // find time of collision and time of leaving for each axis (if statement is to prevent divide by zero) 
    float xEntry, yEntry;
    float xExit, yExit;

    if (vel.x == 0.0f) {
        xEntry = -std::numeric_limits<float>::infinity();
        xExit = std::numeric_limits<float>::infinity();
    } else {
        xEntry = xInvEntry / vel.x;
        xExit = xInvExit / vel.x;
    }

    if (vel.y == 0.0f) {
        yEntry = -std::numeric_limits<float>::infinity();
        yExit = std::numeric_limits<float>::infinity();
    } else {
        yEntry = yInvEntry / vel.y;
        yExit = yInvExit / vel.y;
    }

    // find the earliest/latest times of collisionfloat 
    float entryTime = std::max(xEntry, yEntry);
    float exitTime = std::min(xExit, yExit);

    // if there was no collision
    if (entryTime > exitTime || xEntry < 0.0f && yEntry < 0.0f || xEntry > 1.0f || yEntry > 1.0f) {
        normal = Vec2D();
        return 1.0f;
    } else { // if there was a collision 
        // calculate normal of collided surface
        if (xEntry > yEntry) {
            if (xInvEntry < 0.0f) {
                normal = Vec2D(1.0f, 0.0f);
            } else {
                normal = Vec2D(-1.0f, 0.0f);
            }
        } else {
            if (yInvEntry < 0.0f) {
                normal = Vec2D(0.0f, 1.0f);
            } else {
                normal = Vec2D(0.0f, -1.0f);
            }
        } 
    }
    // return the time of collisionreturn entryTime
    return entryTime;
}