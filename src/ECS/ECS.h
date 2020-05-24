#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
#include <bitset>
#include <array>
#include <map>

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

using ComponentVector = std::map<std::size_t, std::vector<Component*>>;

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
		for (auto kv : components) {
			for (auto c : kv.second) {
				c->update();
			}
		}
	}
	void draw() {
		for (auto kv : components) {
			for (auto c : kv.second) {
				c->draw();
			}
		}
	}
	bool isActive() const { return active; }
	void destroy() { active = false; }
	template <typename T> int count() {
		return (int)components[getComponentTypeID<T>()].size();
	}
	template <typename T> bool has(int amount = 1) {
		if (count<T>() >= amount) {
			return true;
		}
		return false;
	}
	template <typename T, typename... TArgs> T* add(TArgs&&... mArgs) {
		T* c(new T(std::forward<TArgs>(mArgs)...));
		c->entity = this;
		c->id = getComponentTypeID<T>();
		if (has<T>()) {
			components[c->id].emplace_back(std::move(c));
		} else {
			components[c->id] = std::vector<Component*>{ c };
		}
		c->init();
		return c;
	}

	template <typename T> T* get(int index = 0) {
		return static_cast<T*>(components[getComponentTypeID<T>()][index]);
	}
	void printComponents() {
		std::cout << "Player Type IDs: " << std::endl;
		for (auto kv : components) {
			std::cout << typeid(*kv.second[0]).name() << ", Id: ";
			for (auto c : kv.second) {
				std::cout << c->id << ",";
			}
			std::cout << std::endl;
		}
	}
private:
	bool active = true;
	ComponentVector components;
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