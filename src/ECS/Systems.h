#pragma once
#include "ECS.h"
#include "../TextureManager.h"
#include "SDL.h"
#include <string>

class System {
public:
	System(Manager& manager, Signature mask) : _manager(manager), _mask(mask) {}
	virtual void update() = 0;
protected:
	Signature _mask;
	Manager& _manager;
};

class MovementSystem : public System {
public:
	MovementSystem(Manager& manager, Signature mask) : System{ manager, mask } {}
	void update() override {
		Displacement* d;
		Velocity* v;
		std::cout << "[" << std::bitset<32>(_mask) << "] system has matches: [";
		for (SignatureEntityPair match : _manager.matchingSignatures(_mask)) {
			std::cout << std::bitset<32>(match.first) << ": (";
			for (Entity entity : *(match.second)) {
				std::cout << entity << ",";
				d = &_manager.displacements[entity];
				v = &_manager.velocities[entity];

				//v->y -= 0.98f;

				d->x += v->x;
				d->y += v->y;

			}
			std::cout << "),";
		}
		std::cout << "]" << std::endl;
	}
};

class RenderSystem : public System {
public:
	RenderSystem(Manager& manager, Signature mask) : System{ manager, mask } {}
	void update() override {
		Displacement* d;
		Appearance* a;
		for (SignatureEntityPair match : _manager.matchingSignatures(_mask)) {
			for (Entity entity : *(match.second)) {
				d = &_manager.displacements[entity];
				a = &_manager.appearances[entity];

				std::string path = "./resources/textures/" + std::string(a->name) + ".png";
				TextureManager::draw(a->texture, SDL_Rect{ 0, 0, 16, 16 }, SDL_Rect{ (int)round(d->x), (int)round(d->y), 32, 32 });
				TextureManager::draw({ (int)d->x, (int)d->y, 32, 32 });

				//printf("%s at (%f, %f)\n", a->name, d->x, d->y);
			}
		}
	}
};