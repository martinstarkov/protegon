#include"ECS.h"

int ComponentManager::_lastComponentTypeId = 0;
std::map<int, const char*> ComponentManager::_components;

void Entity::addGroup(Group group) {
	_manager.setGroup(this, group);
}