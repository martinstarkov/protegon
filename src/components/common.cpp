#include "components/common.h"

#include "core/entity.h"

namespace ptgn {

Depth Depth::RelativeTo(Depth parent) const {
	parent.value_ += *this;
	return parent;
}

bool EntityDepthCompare::operator()(const Entity& a, const Entity& b) const {
	auto depth_a{ a.GetDepth() };
	auto depth_b{ b.GetDepth() };
	if (depth_a == depth_b) {
		return a.WasCreatedBefore(b);
	}
	return depth_a < depth_b;
}

} // namespace ptgn