#pragma once

#include "manager/ResourceManager.h"
#include "animation/AnimationState.h"

namespace ptgn {

namespace animation {

class AnimationMap : public manager::ResourceManager<AnimationState> {
public:
	void Update() {
		ForEach([&](AnimationState& animation_state) {
			animation_state.Update();
		});
	}
	std::size_t Size() const {
		return ResourceManager::Size();
	}
};

} // namespace animation

} // namespace ptgn