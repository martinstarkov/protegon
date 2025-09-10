#pragma once

#include <variant>

#include "components/transform.h"
#include "math/geometry/arc.h"
#include "math/geometry/capsule.h"
#include "math/geometry/circle.h"
#include "math/geometry/ellipse.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/rounded_rect.h"
#include "math/geometry/triangle.h"
#include "math/vector2.h"
#include "serialization/fwd.h"

namespace ptgn {

class Entity;

class Shape;

namespace impl {

template <typename... Ts>
struct NamedVariant : public std::variant<Ts...> {
	using std::variant<Ts...>::variant;
	using variant_type = std::variant<Ts...>;
};

using ShapeType = NamedVariant<
	V2_float, Rect, Circle, Ellipse, Polygon, RoundedRect, Arc, Line, Triangle, Capsule>;

using ColliderType = NamedVariant<
	V2_float, Rect, Circle, Ellipse, Polygon, RoundedRect, Arc, Line, Triangle, Capsule>;

using InteractiveType = NamedVariant<Rect, Circle>;

} // namespace impl

template <typename T>
concept Visitable = requires(const T& v) {
	std::visit([](const auto&) {}, static_cast<const typename T::variant_type&>(v));
};

class InteractiveShape : public impl::InteractiveType {
public:
	using impl::InteractiveType::InteractiveType;

	template <Visitable T>
	InteractiveShape(const T& shape) {
		std::visit([&](const auto& value) { *this = InteractiveShape{ value }; }, shape);
	}

	// friend void to_json(json& j, const InteractiveShape& shape);
	// friend void from_json(const json& j, InteractiveShape& shape);
};

class ColliderShape : public impl::ColliderType {
public:
	using impl::ColliderType::ColliderType;

	template <Visitable T>
	ColliderShape(const T& shape) {
		std::visit([&](const auto& value) { *this = ColliderShape{ value }; }, shape);
	}

	// friend void to_json(json& j, const ColliderShape& shape);
	// friend void from_json(const json& j, ColliderShape& shape);
};

class Shape : public impl::ShapeType {
public:
	using impl::ShapeType::ShapeType;

	Shape() = default;

	template <Visitable T>
	Shape(const T& shape) {
		std::visit([&](const auto& value) { *this = Shape{ value }; }, shape);
	}

	// friend void to_json(json& j, const Shape& shape);
	// friend void from_json(const json& j, Shape& shape);
};

[[nodiscard]] Transform OffsetByOrigin(
	const Shape& shape, const Transform& transform, const Entity& entity
);

} // namespace ptgn