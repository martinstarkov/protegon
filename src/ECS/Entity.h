#pragma once
#include "Types.h"
#include <map>
#include <iostream>
#include "Components.h"

class Component;
class Manager;

class Entity {
private:
	using ComponentMap = std::map<ComponentID, Component*>;
	EntityID _id;
	ComponentMap _components;
	bool _alive = true;
public:
	Entity(EntityID id) : _id(id) {}

	Entity(const Entity&) = delete;
	~Entity() = default;

	Entity* tree(float x, float y) {
		addComponent<TransformComponent>(Vec2D(x, y));
		addComponent<SizeComponent>(Vec2D(32, 32));
		addComponent<SpriteComponent>("./resources/textures/enemy.png", AABB(0, 0, 16, 16));
		addComponent<RenderComponent>();
		return this;
	}
	Entity* box(float x, float y) {
		addComponent<TransformComponent>(Vec2D(x, y));
		addComponent<SizeComponent>(Vec2D(16, 16));
		addComponent<MotionComponent>(Vec2D(0.1f, 0.1f));
		addComponent<GravityComponent>();
		addComponent<SpriteComponent>("./resources/textures/player.png", AABB(0, 0, 16, 16));
		addComponent<RenderComponent>();
		return this;
	}
	Entity* ghost(float x, float y, float lifetime = 10.0f) {
		addComponent<TransformComponent>(Vec2D(x, y));
		addComponent<SizeComponent>(Vec2D(16, 16));
		addComponent<MotionComponent>(Vec2D(0.01f, 0.0f));
		addComponent<GravityComponent>();
		addComponent<LifetimeComponent>(lifetime);
		addComponent<RenderComponent>();
		return this;
	}

	void destroy() { _alive = false; }
	bool isAlive() { return _alive; }

	template <typename TComponent, typename... TArgs> void addComponent(TArgs&&... mArgs) {
		if (_components.find(TComponent::ID) == _components.end()) {
			TComponent* p(new TComponent(std::forward<TArgs>(mArgs)...));
			p->setEntityID(_id);
			_components.emplace(TComponent::ID, std::move(p));
		}
	}

	EntityID getID() { return _id; }

	const ComponentMap getComponents() const { return _components;  }

	template <typename TComponent> TComponent* getComponent() {
		auto it = _components.find(TComponent::ID);
		if (it != _components.end()) {
			return static_cast<TComponent*>(it->second);
		}
		return nullptr;
	}
};

