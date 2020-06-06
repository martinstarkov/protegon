
#include "Entity.h"
#include "Manager.h"

void Entity::refreshManager() {
	_manager->refreshSystems(_id);
}
