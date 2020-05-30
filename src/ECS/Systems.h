#pragma once
#include "ECS.h"
#include "../TextureManager.h"
#include "SDL.h"
#include <string>

class System {
public:
	System(Manager& manager, int mask) : _manager(manager), _mask(mask) {}
	virtual void update() = 0;
protected:
	int _mask;
	Manager& _manager;
};

class MovementSystem : public System {
public:
	MovementSystem(Manager& manager, int mask) : System{ manager, mask } {}
	void update() override {
		Entity entity;
		Displacement* d;
		Velocity* v;
		for (entity = 0; entity < ENTITY_COUNT; ++entity) {
			if ((_manager.masks[entity] & _mask) == _mask) {
				d = &_manager.displacements[entity];
				v = &_manager.velocities[entity];

				//v->y -= 0.98f;

				d->x += v->x;
				d->y += v->y;
			}
		}
	}
};

class RenderSystem : public System {
public:
	RenderSystem(Manager& manager, int mask) : System{ manager, mask } {}
	void update() override {
		Entity entity;
		Displacement* d;
		Appearance* a;
		for (entity = 0; entity < ENTITY_COUNT; ++entity) {
			if ((_manager.masks[entity] & _mask) == _mask) {
				d = &_manager.displacements[entity];
				a = &_manager.appearances[entity];

				std::string path = "./resources/textures/" + std::string(a->name) + ".png";
				TextureManager::draw(a->texture, SDL_Rect{ 0, 0, 16, 16 }, SDL_Rect{ (int)round(d->x), (int)round(d->y), 32, 32 }); 
				TextureManager::draw({ (int)d->x, (int)d->y, 32, 32 });

				printf("%s at (%f, %f)\n", a->name, d->x, d->y);
			}
		}
	}
};