// #pragma once
//
// #include <vector>
//
// #include "ecs/components/generic.h"
// #include "ecs/entity.h"
// #include "serialization/json/serializable.h"
//
// namespace ptgn {
//
//// Note: Will call hide on post_fx entity.
//// @return entity.
// Entity& AddPostFX(Entity& entity, Entity post_fx);
//
//// Note: Will call hide on pre_fx entity.
//// @return entity.
// Entity& AddPreFX(Entity& entity, Entity pre_fx);
//
// namespace impl {
//
// struct UsePreviousTexture : public BoolComponent {
//	using BoolComponent::BoolComponent;
//
//	UsePreviousTexture() : BoolComponent{ true } {}
// };
//
// } // namespace impl
//
// struct PostFX {
//	PostFX() = default;
//
//	std::vector<Entity> post_fx_;
//
//	friend bool operator==(const PostFX&, const PostFX&) = default;
//
//	PTGN_SERIALIZER_REGISTER_NAMED(PostFX, KeyValue("post_fx", post_fx_))
// };
//
// struct PreFX {
//	PreFX() = default;
//
//	std::vector<Entity> pre_fx_;
//
//	friend bool operator==(const PreFX&, const PreFX&) = default;
//
//	PTGN_SERIALIZER_REGISTER_NAMED(PreFX, KeyValue("pre_fx", pre_fx_))
// };
//
// } // namespace ptgn