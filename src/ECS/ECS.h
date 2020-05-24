#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
#include <bitset>
#include <array>

class Component;
class Entity;
class Manager;

inline std::size_t getComponentTypeID() {
	static std::size_t lastID = 0;
	return lastID++;
}

template <typename T> inline std::size_t getComponentTypeID() noexcept {
	static std::size_t typeID = getComponentTypeID();
	return typeID;
}

constexpr std::size_t maxComponents = 32;

using ComponentBitSet = std::bitset<maxComponents>;
using ComponentArray = std::array<Component*, maxComponents>;

class Component {
public:
	Entity* entity = nullptr;
	virtual void init() {}
	virtual void update() {}
	virtual void draw() {}
	virtual ~Component() {}
	std::size_t id = 0;
};

class Entity {
public:
	void update() {
		for (auto& c : components) c->update();
	}
	void draw() {
		for (auto& c : components) c->draw();
	}
	bool isActive() const { return active; }
	void destroy() { active = false; }
	template <typename T> bool has() const {
		return componentBitSet[getComponentTypeID< T >()];
	}
	template <typename T, typename... TArgs> T& add(TArgs&&... mArgs) {
		T* c(new T(std::forward<TArgs>(mArgs)...));
		c->entity = this;
		c->id = getComponentTypeID<T>();
		std::unique_ptr<Component> uPtr{ c };
		components.emplace_back(std::move(uPtr));
		componentArray[c->id] = c;
		componentBitSet[c->id] = true;
		c->init();
		return *c;
	}

	template <typename T> T& get(bool createIfNotFound = false) {
		Component* c = componentArray[getComponentTypeID<T>()];
		if (createIfNotFound) {
			if (c) {
				return *static_cast<T*>(c);
			} else {
				return add<T>();
			}
		} else {
			return *static_cast<T*>(c);
		}
	}
	void printComponents() {
		//std::cout << "Component Type IDs: ";
		//int i = 0;
		//for (auto& c : components) {
		//	std::cout << componentArray[i]->id << ",";
		//	i++;
		//}
		//std::cout << std::endl;
	}
private:
	bool active = true;
	std::vector<std::unique_ptr<Component>> components;
	ComponentArray componentArray = {};
	ComponentBitSet componentBitSet;
};

class Manager {
public:
	void update() {
		for (auto& e : entities) e->update();
	}
	void draw() {
		for (auto& e : entities) e->draw();
	}
	void refresh() {
		entities.erase(std::remove_if(std::begin(entities), std::end(entities), [](const std::unique_ptr<Entity>& mEntity) { return !mEntity->isActive(); }), std::end(entities));
	}
	Entity& addEntity() {
		Entity* e = new Entity();
		std::unique_ptr<Entity> uPtr{ e };
		entities.emplace_back(std::move(uPtr));
		return *e;
	}
private:
	std::vector<std::unique_ptr<Entity>> entities;
};