#pragma once

#include "common/concepts.h"

namespace ptgn {

struct Transform;
struct Depth;
struct Visible;
struct Interactive;
struct Children;
struct Parent;
struct Tint;
struct PreFX;
struct PostFX;
class UUID;

namespace impl {

class IDrawable;
class IDrawFilter;
struct ChildKey;
struct IgnoreParentTransform;

// Components which	cannot be modified or retrieved by the user through the entity class.
template <typename T>
concept RetrievableComponent = !IsAnyOf<
	T, Transform, Depth, Visible, Interactive, impl::IDrawable, impl::IDrawFilter, Tint, Children,
	Parent, impl::ChildKey, impl::IgnoreParentTransform, PreFX, PostFX, UUID>;

template <typename... Ts>
concept AllRetrievableComponents = (RetrievableComponent<Ts> && ...);

template <typename T>
concept ModifiableComponent = RetrievableComponent<T>;

} // namespace impl

} // namespace ptgn