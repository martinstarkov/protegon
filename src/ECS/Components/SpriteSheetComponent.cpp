#include "SpriteSheetComponent.h"

SpriteInformation SpriteSheetComponent::getSpriteInformation(AnimationName name, Direction direction) {
	auto aIt = animations.find(name);
	assert(aIt != animations.end() && "AnimationName not found in AnimationComponent");
	if (direction == Direction::LEFT) { // ignore SDL flipped direction
		direction = Direction::RIGHT;
	}
	assert(aIt->second.size() > 0 && "Cannot fetch spriteInformation for size 0 spriteSheetMap");
	auto dIt = aIt->second.find(direction);
	if (dIt != aIt->second.end()) {
		return dIt->second;
	} else { // return first spriteSheetMap entry if direction is not found
		return aIt->second.begin()->second;
	}
}