#include "FallingPlatform.h"
#include "Player.h"

void FallingPlatform::reset() {
	Entity::reset();
	lifetime = originalLifetime;
	fallen = false;
}

void FallingPlatform::update() {
	Entity::update();
	//std::cout << "Falling: " << gravity << std::endl;
	//std::cout << "velocity: " << velocity << std::endl;
	if (!fallen) {
		if (lifetime == 0) {
			fallen = true;
			gravity = true;
			Player* player = Player::getInstance();
			player->setVelocity(Vec2D(player->getVelocity().x, velocity.y));
			color = { 0, 0, 0, 255 };
		} else {
			float fraction = float(lifetime) / float(originalLifetime);
			Uint8 b = Uint8(255.0f * fraction);
			Uint8 r = Uint8(125.0f * (1.0f - fraction));
			color = { r, 0, b, 255 };
		}
	}
}
