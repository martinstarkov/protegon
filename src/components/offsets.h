#pragma once

#include "components/transform.h"
#include "serialization/serializable.h"

namespace ptgn {

class Entity;

namespace impl {

/**
 * @brief Holds temporary transform offsets that do not permanently change an entity's transform.
 *
 * This struct is useful for representing temporary visual or motion effects such as camera shake or
 * bounce. These effects are meant to be transient and should not modify the underlying transform of
 * an entity.
 */
struct Offsets {
	/**
	 * @brief Computes the combined transform of all temporary offsets.
	 *
	 * @return Transform The total combined offset transform (e.g., shake + bounce).
	 */
	[[nodiscard]] Transform GetTotal() const;

	// Temporary transform applied for camera or entity shake effect.
	Transform shake;

	// Temporary transform applied for bounce effect.
	Transform bounce;

	// User applied offset.
	Transform custom;

	PTGN_SERIALIZER_REGISTER(Offsets, shake, bounce, custom)
};

} // namespace impl

/**
 * @brief Computes the relative offset transform for a given entity.
 *
 * This represents how much the entity is offset relative to its base transform due to temporary
 * effects.
 *
 * @param entity The entity to compute the relative offset for.
 * @return Transform The computed relative offset.
 */
[[nodiscard]] Transform GetRelativeOffset(const Entity& entity);

/**
 * @brief Retrieves the total (including parent offsets) temporary transform offset for a given
 * entity.
 *
 * This includes effects like shake or bounce and is meant to be applied on top of the entity's
 * regular transform.
 *
 * @param entity The entity to retrieve the offset for.
 * @return Transform The total offset applied to the entity.
 */
[[nodiscard]] Transform GetOffset(const Entity& entity);

} // namespace ptgn