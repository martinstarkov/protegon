#include "Entity.h"

#include "Components.h"

// TODO: Refactor serialize and deserialize functions to cycle through EntityData components lists

Entity::Entity() : _id(UINT_MAX), _manager(nullptr) {}
Entity::Entity(Manager* manager) : _id(UINT_MAX), _manager(manager) {}
Entity::Entity(EntityID id, Manager* manager) : _id(id), _manager(manager) {}

bool Entity::hasComponent(ComponentID cId) {
	assert(_manager);
	return _manager->hasComponent(_id, cId);
}

void Entity::destroy() {
	assert(_manager);
	_manager->destroyEntity(_id);
}

EntityID Entity::getID() {
	return _id;
}

const EntityID Entity::getID() const {
	return _id;
}

void Entity::setID(EntityID id) {
	_id = id;
}

Manager* Entity::getManager() {
	return _manager;
}

Manager* Entity::getManager() const {
	return _manager;
}

// json serialization

namespace Serializer {

	template <typename C>
	void deserializeComponent(Entity& o, const nlohmann::json& j) {
		auto key = typeid(C).name();
		if (j.find(key) != j.end()) {
			json jsonComponent = j.at(key);
			C component = jsonComponent.get<C>();
			o.addComponent(component);
		} else {
			//LOG("Could not find " << key << " in components json field for EntityID " << o.getID());
		}
	}

	template <typename ...Cs>
	void deserialize(Entity& o, const nlohmann::json& j) {
		if (j.find("components") != j.end()) {
			Util::swallow((deserializeComponent<Cs>(o, j["components"]), 0)...);
		} else {
			// Assert not necessary but good to have while developing
			assert(false && "Attempting to deserialize Entity without ""components"" json field");
		}
	}

	void deserialize(const nlohmann::json& j, Entity& o) {
		Entity handle = Entity(o.getID(), o.getManager());
		deserialize<
			AnimationComponent,
			CollisionComponent,
			DirectionComponent,
			InputComponent,
			LifetimeComponent,
			PlayerController,
			RenderComponent,
			RigidBodyComponent,
			SpriteComponent,
			SpriteSheetComponent,
			TransformComponent
			// StateMachineComponent // add when map is fixed
		>(handle, j);
		o = handle;
	}
}