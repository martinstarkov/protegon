#pragma once

#include <string_view>

#include "runtime/ecs/component.h"

namespace ptgn::impl {

constexpr std::string_view kDefaultTag{ "Unnamed Entity" };

class Tag : public StringComponent {
public:
	using StringComponent::StringComponent;
};

} // namespace ptgn::impl