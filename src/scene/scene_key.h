#pragma once

#include "components/generic.h"

namespace ptgn {

namespace impl {

struct SceneKey : public ArithmeticComponent<std::size_t> {
	using ArithmeticComponent::ArithmeticComponent;
};

} // namespace impl

} // namespace ptgn