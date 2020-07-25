#pragma once

#include "System.h"

#include "SDL.h"

// TODO: Add key specificity to InputComponent

class InputSystem : public System<InputComponent> {
public:
	virtual void update() override final {
		s = SDL_GetKeyboardState(NULL);
		for (auto& id : entities) {
			Entity e = Entity(id, manager);
			/*if (s[SDL_SCANCODE_E]) {
				e.addComponents(Serialization::deserialize<SpriteComponent>("resources/sprite1.json"));
			}
			if (s[SDL_SCANCODE_Q]) {
				e.addComponents(Serialization::deserialize<SpriteComponent>("resources/sprite2.json"));
			}*/
			if (e.getID() == 2) {
				if (s[SDL_SCANCODE_P]) {
					Vec2D pos = e.getComponent<TransformComponent>()->position + Vec2D(50.0, 0.0);
					manager->createBox(pos);
				}
				//if (s[SDL_SCANCODE_R]) {
				//	std::remove("resources/player.json");
				//	std::ofstream out("resources/player.json");
				//	json j;
				//	Serializer::serialize(j[typeid(Entity).name()], e);
				//	out << std::setw(4) << j; // overloaded setw for pretty printing, number is amount of spaces to indent
				//	out.close();
				//}
				//if (s[SDL_SCANCODE_F]) {
				//	Entity handle = Entity(manager);
				//	std::ifstream in("resources/player.json");
				//	json j;
				//	in >> j;
				//	Serializer::deserialize(j[typeid(Entity).name()], handle);
				//	in.close();
				//}

				if (s[SDL_SCANCODE_C]) {
					for (auto& rId : manager->getEntities({ 2 })) {
						Entity handle(rId, manager);
						handle.destroy();
					}
				}
			}
			PlayerController* player = e.getComponent<PlayerController>();
			if (player) {
				MotionComponent* motion = e.getComponent<MotionComponent>();
				if (motion) {
					// player stops
					if ((s[SDL_SCANCODE_A] && s[SDL_SCANCODE_D]) || (!s[SDL_SCANCODE_A] && !s[SDL_SCANCODE_D])) {
						motion->acceleration.x = 0.0;
					}
					if ((s[SDL_SCANCODE_W] && s[SDL_SCANCODE_S]) || (!s[SDL_SCANCODE_W] && !s[SDL_SCANCODE_S])) {
						motion->acceleration.y = 0.0;
					}
					// key press occurs
					if (s[SDL_SCANCODE_W] || s[SDL_SCANCODE_A] || s[SDL_SCANCODE_S] || s[SDL_SCANCODE_D]) {
						if (s[SDL_SCANCODE_W] && !s[SDL_SCANCODE_S]) { // jump
							motion->acceleration.y = -player->speed.y;
						}
						if (s[SDL_SCANCODE_S] && !s[SDL_SCANCODE_W]) { // press down
							motion->acceleration.y = player->speed.y;
						}
						if (s[SDL_SCANCODE_A] && !s[SDL_SCANCODE_D]) { // walk left
							motion->acceleration.x = -player->speed.x;
						}
						if (s[SDL_SCANCODE_D] && !s[SDL_SCANCODE_A]) { // walk right
							motion->acceleration.x = player->speed.x;
						}
					} else { // player stops
					}
				}
			}
		}
	}
private:
	const Uint8* s = nullptr;
};