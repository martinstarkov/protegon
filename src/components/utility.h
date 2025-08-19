#pragma once

#include "common/type_traits.h"

namespace ptgn {

struct Transform;
struct Depth;
struct Visible;
struct Interactive;
class IDrawable;
struct Children;
struct Parent;
struct Tint;
struct PreFX;
struct PostFX;
class UUID;

namespace impl {

struct ChildKey;
struct IgnoreParentTransform;

// Components which	cannot be modified or retrieved by the user through the entity class.
template <typename T>
inline constexpr bool access_disabled_v{ tt::is_any_of_v<
	T, Transform, Depth, Visible, Interactive, IDrawable, Tint, Children, Parent, impl::ChildKey,
	impl::IgnoreParentTransform, PreFX, PostFX, UUID> };

template <typename T>
concept RetrievableComponent = !access_disabled_v<T>;

template <typename... Ts>
concept AllRetrievableComponents = (RetrievableComponent<Ts> && ...);

template <typename T>
concept ModifiableComponent = !access_disabled_v<T>;

} // namespace impl

} // namespace ptgn