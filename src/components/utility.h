#pragma once

#include "common/type_traits.h"

namespace ptgn {

struct Transform;
struct Depth;
struct Visible;
struct Interactive;
class IDrawable;

namespace impl {

// Components which	cannot be retrieved by the user through Get<>
template <typename T>
inline constexpr bool is_retrievable_component_v{
	!tt::is_any_of_v<T, Transform, Depth, Visible, Interactive, IDrawable>
};

} // namespace impl

} // namespace ptgn