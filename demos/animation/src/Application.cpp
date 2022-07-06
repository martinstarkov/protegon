#include <iostream>

#include "animation/SpriteMap.h"
#include "animation/Offset.h"

using namespace ptgn;

int main(int c, char** v) {

	animation::Animation test{ { 0, 0 }, { 16, 16 }, 3 };

	V2_int hitbox_size{ 8, 8 };

	animation::Offset offset{ test, hitbox_size, animation::Alignment::LEFT, animation::Alignment::TOP };
	std::cout << offset.value << std::endl;
	offset = { test, hitbox_size, animation::Alignment::MIDDLE, animation::Alignment::TOP };
	std::cout << offset.value << std::endl;
	offset = { test, hitbox_size, animation::Alignment::RIGHT, animation::Alignment::TOP };
	std::cout << offset.value << std::endl;
	offset = { test, hitbox_size, animation::Alignment::LEFT, animation::Alignment::MIDDLE };
	std::cout << offset.value << std::endl;
	offset = { test, hitbox_size, animation::Alignment::MIDDLE, animation::Alignment::MIDDLE };
	std::cout << offset.value << std::endl;
	offset = { test, hitbox_size, animation::Alignment::RIGHT, animation::Alignment::MIDDLE };
	std::cout << offset.value << std::endl;
	offset = { test, hitbox_size, animation::Alignment::LEFT, animation::Alignment::BOTTOM };
	std::cout << offset.value << std::endl;
	offset = { test, hitbox_size, animation::Alignment::MIDDLE, animation::Alignment::BOTTOM };
	std::cout << offset.value << std::endl;
	offset = { test, hitbox_size, animation::Alignment::RIGHT, animation::Alignment::BOTTOM };
	std::cout << offset.value << std::endl;
	
	return 0;
}