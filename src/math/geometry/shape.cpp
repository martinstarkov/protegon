#include "math/geometry/shape.h"

#include <variant>

#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "math/geometry/rect.h"
#include "math/vector2.h"
#include "serialization/json.h"

namespace ptgn {

// TODO: Fix shape type serialization.

/*
void to_json(json& j, const InteractiveShape& shape) {
	// nlohmann::adl_serializer<impl::InteractiveType>::to_json(j, shape);
}

void from_json(const json& j, InteractiveShape& shape) {
	// nlohmann::adl_serializer<impl::InteractiveType>::from_json(j, shape);
}

void to_json(json& j, const ColliderShape& shape) {
	// nlohmann::adl_serializer<impl::ColliderType>::to_json(j, shape);
}

void from_json(const json& j, ColliderShape& shape) {
	// nlohmann::adl_serializer<impl::ColliderType>::from_json(j, shape);
}

void to_json(json& j, const Shape& shape) {
	// nlohmann::adl_serializer<impl::ShapeType>::to_json(j, shape);
}

void from_json(const json& j, Shape& shape) {
	// nlohmann::adl_serializer<impl::ShapeType>::from_json(j, shape);
}
*/

Transform OffsetByOrigin(const Shape& shape, const Transform& transform, const Entity& entity) {
	if (!std::holds_alternative<Rect>(shape)) {
		return transform;
	}
	const Rect& rect{ std::get<Rect>(shape) };
	auto draw_origin{ GetDrawOrigin(entity) };
	return rect.Offset(transform, draw_origin);
}

} // namespace ptgn