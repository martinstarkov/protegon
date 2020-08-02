#include "SpriteSheetComponent.h"

SpriteInformation SpriteSheetComponent::getSpriteInformation(AnimationName name, Direction direction) {
	auto aIt = animations.find(name); // animation iterator
	assert(aIt != animations.end() && "AnimationName not found in AnimationComponent");
	if (direction == Direction::LEFT) { // ignore SDL flipped direction
		direction = Direction::RIGHT;
	}
	assert(aIt->second.size() > 0 && "Cannot fetch spriteInformation for size 0 spriteSheetMap");
	auto dIt = aIt->second.find(direction); // direction iterator
	if (dIt != aIt->second.end()) {
		return dIt->second; // SpriteInformation
	} else {
		// return the first DirectionMap entry if specified direction is not found
		// CONSIDER: Somehow improve this in the future?
		return aIt->second.begin()->second;
	}
}