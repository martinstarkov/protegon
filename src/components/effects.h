#pragma once

#include <vector>

#include "components/generic.h"
#include "core/entity.h"
#include "serialization/serializable.h"

namespace ptgn {

// Note: Will call hide on post_fx entity.
// @return entity.
Entity& AddPostFX(Entity& entity, Entity post_fx);

// Note: Will call hide on pre_fx entity.
// @return entity.
Entity& AddPreFX(Entity& entity, Entity pre_fx);

namespace impl {

struct UsePreviousTexture : public BoolComponent {
	using BoolComponent::BoolComponent;

	UsePreviousTexture() : BoolComponent{ true } {}
};

struct PostFX {
	PostFX() = default;

	std::vector<Entity> post_fx_;

	friend bool operator==(const PostFX&, const PostFX&) = default;

	PTGN_SERIALIZER_REGISTER_NAMED(PostFX, KeyValue("post_fx", post_fx_))
};

struct PreFX {
	PreFX() = default;

	std::vector<Entity> pre_fx_;

	friend bool operator==(const PreFX&, const PreFX&) = default;

	PTGN_SERIALIZER_REGISTER_NAMED(PreFX, KeyValue("pre_fx", pre_fx_))
};

} // namespace impl

} // namespace ptgn