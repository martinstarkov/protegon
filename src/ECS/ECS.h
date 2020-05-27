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

// add component bitsets for each entity and systems so they can check

using ComponentMap = std::map<int, std::vector<Component*>>;
using Group = std::size_t;

enum Groups : std::size_t {
	drawables,
	hitboxes,
	dynamics,
	colliders,
	projectiles,
	shooters,
	COUNT
};

class ComponentManager {
public:
	static int componentAddition(const char* className) {
		if (!hasComponent(className)) {
			_lastComponentTypeId++;
			_components.insert({ _lastComponentTypeId, className });
			return _lastComponentTypeId;
		} else {
			return findIndex(className);
		}
	}
	static int findIndex(const char* className) {
		int id = 0;
		for (auto it = _components.begin(); it != _components.end(); it++) {
			if ((*it).second == className) {
				id = (*it).first;
				break;
			}
		}
		return id;
	}
	static const char* findName(int index) {
		if (_components.find(index) != _components.end()) {
			return _components[index];
		} else {
			return "";
		}
	}
	static int getLastIndex() {
		return _lastComponentTypeId;
	}
	static bool hasComponent(const char* className) {
		if (findIndex(className)) {
			return true;
		}
		return false;
	}
private:
	static int _lastComponentTypeId;
	static std::map<int, const char*> _components;
};

class Component {
public:
	Entity* entity = nullptr;
	virtual void init() {}
	virtual void update() {}
	virtual void draw() {}
	virtual ~Component() {}
	void add(Component* c) {
		c->setChild(true);
		int positionIndex = ComponentManager::findIndex(typeid(*c).name());
		if (has(positionIndex)) {
			_children[positionIndex].push_back(c);
		} else {
			_children.insert({ positionIndex, std::vector<Component*>{ c } });
		}
	}
	template <typename T> T* get(int index = 0) {
		int positionIndex = ComponentManager::findIndex(typeid(T).name());
		if (has(positionIndex)) {
			return static_cast<T*>(_children[positionIndex][index]);
		}
		return nullptr;
	}
	template <typename T> bool has() {
		int positionIndex = ComponentManager::findIndex(typeid(T).name());
		return has(positionIndex);
	}
	template <typename T> int count() {
		int positionIndex = ComponentManager::findIndex(typeid(T).name());
		if (has(positionIndex)) {
			return (int)_children[positionIndex].size();
		}
		return 0;
	}
	int childrenCount() {
		return static_cast<int>(_children.size());
	}
	void printChildComponents() {
		std::cout << "#############" << std::endl;
		std::cout << ComponentManager::findName(_id) << " has following child components: " << std::endl;
		for (auto component : _children) {
			std::cout << ComponentManager::findName(component.first) << ": ";
			for (auto c : component.second) {
				std::cout << c << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "#############" << std::endl;
	}
	void setId(int id) { _id = id; }
	int getId() { return _id; }
	void setChild(bool child) { _child = child; }
	bool isChild() { return _child; }
private:
	bool has(int index) {
		if (_children.find(index) != _children.end()) {
			return true;
		}
		return false;
	}
private:
	int _id = 0;
	bool _child = false;
	ComponentMap _children;
};

class Entity {
public:
	Manager& _manager;
public:
	Entity(Manager& manager) : _manager(manager) {}
	void update() {
		for (auto kv : _components) {
			for (auto c : kv.second) {
				c->update();
			}
		}
	}
	void draw() {
		for (auto kv : _components) {
			for (auto c : kv.second) {
				c->draw();
			}
		}
	}
	bool isActive() const { return _active; }
	void destroy() { _active = false; }
	template <typename T> bool has() {
		int positionIndex = ComponentManager::findIndex(typeid(T).name());
		return has(positionIndex);
	}
	template <typename T, typename... TArgs> T* add(TArgs&&... mArgs) {
		T* c(new T(std::forward<TArgs>(mArgs)...));
		const char* name = typeid(T).name();
		c->entity = this;
		c->setId(ComponentManager::componentAddition(name));
		if (has<T>()) {
			_components[c->getId()].push_back(c);
		} else { // first type of component
			_components.insert({ c->getId(), std::vector<Component*>{ c } });
		}
		c->init();
		return c;
	}
	void addGroup(Group group);
	template <typename T> T* get(int index = 0) {
		int positionIndex = ComponentManager::findIndex(typeid(T).name());
		if (has(positionIndex)) {
			return static_cast<T*>(_components[positionIndex][index]);
		}
		return nullptr;
	}
	template <typename T> std::vector<T*> getComponents() {
		int positionIndex = ComponentManager::findIndex(typeid(T).name());
		std::vector<T*> vector;
		if (has(positionIndex)) {
			for (auto c : _components[positionIndex]) {
				vector.push_back(static_cast<T*>(c));
			}
		}
		return vector;
	}
	template <typename T> int count() {
		int positionIndex = ComponentManager::findIndex(typeid(T).name());
		if (has(positionIndex)) {
			return (int)_components[positionIndex].size();
		}
		return 0;
	}
	void printComponents() {
		std::cout << "----------------" << std::endl;
		for (auto kv : _components) {
			std::cout << ComponentManager::findName(kv.first) << ": ";
			if (kv.second.size() > 0) {
				for (auto c : kv.second) {
					std::cout << c << ",";
					//if (c->childrenCount()) {
					//	std::cout << std::endl;
					//	c->printChildComponents();
					//}
				}
			}
			std::cout << std::endl;
		}
	}
private:
	bool has(int index) {
		if (_components.find(index) != _components.end()) {
			return true;
		}
		return false;
	}
private:
	bool _active = true;
	ComponentMap _components;
};

class Manager {
public:
	void update() {
		for (auto& e : _entities) {
			e->update();
		}
	}
	void draw() {
		for (auto& e : _groups[Groups::drawables]) {
			e->draw();
		}
	}
	void refresh() {
		_entities.erase(std::remove_if(std::begin(_entities), std::end(_entities), [](const std::unique_ptr<Entity>& mEntity) { return !mEntity->isActive(); }), std::end(_entities));
	}
	Entity& addEntity() {
		Entity* e = new Entity(*this);
		std::unique_ptr<Entity> uPtr{ e };
		_entities.emplace_back(std::move(uPtr));
		return *e;
	}
	void setGroup(Entity* entity, Group group) {
		_groups[group].emplace_back(entity);
	}
	std::vector<Entity*> getGroup(Group group) {
		return _groups[group];
	}
	bool hasGroup(Group group) {
		return _groups[group].size() > 0;
	}
	void printGroup(Group group) {
		std::cout << "Group " << group << " members: ";
		for (auto m : _groups[group]) {
			std::cout << m << ",";
		}
		std::cout << std::endl;
	}
private:
	std::vector<std::unique_ptr<Entity>> _entities;
	std::array<std::vector<Entity*>, Groups::COUNT> _groups;
};