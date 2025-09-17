#pragma once

#include <optional>
#include <variant>
#include <vector>

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

	template <typename T>
	static constexpr bool contains = (std::is_same_v<T, Ts> || ...);
};

using ShapeVariant = NamedVariant<
	V2_float, Rect, Circle, Ellipse, Polygon, RoundedRect, Arc, Line, Triangle, Capsule>;

using ColliderVariant = NamedVariant<
	V2_float, Rect, Circle, Ellipse, Polygon, RoundedRect, Arc, Line, Triangle, Capsule>;

using InteractiveVariant = NamedVariant<Rect, Circle>;

} // namespace impl

template <typename T>
concept Visitable = requires(const T& v) {
	std::visit([](const auto&) {}, static_cast<const typename T::variant_type&>(v));
};

template <typename T>
concept ShapeType = impl::ShapeVariant::template contains<T>;

template <typename T>
concept InteractiveType = impl::InteractiveVariant::template contains<T>;

template <typename T>
concept ColliderType = impl::ColliderVariant::template contains<T>;

class InteractiveShape : public impl::InteractiveVariant {
public:
	using impl::InteractiveVariant::InteractiveVariant;

	template <Visitable T>
	InteractiveShape(const T& shape) {
		std::visit([&](const auto& value) { *this = InteractiveShape{ value }; }, shape);
	}

	// friend void to_json(json& j, const InteractiveShape& shape);
	// friend void from_json(const json& j, InteractiveShape& shape);
};

class ColliderShape : public impl::ColliderVariant {
public:
	using impl::ColliderVariant::ColliderVariant;

	template <Visitable T>
	ColliderShape(const T& shape) {
		std::visit([&](const auto& value) { *this = ColliderShape{ value }; }, shape);
	}

	// friend void to_json(json& j, const ColliderShape& shape);
	// friend void from_json(const json& j, ColliderShape& shape);
};

class Shape : public impl::ShapeVariant {
public:
	using impl::ShapeVariant::ShapeVariant;

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

// @return The shape of the entity, if it has one.
std::optional<Shape> GetShape(const Entity& entity);

// @return The display size of the entity sprite (if it has a TextureHandle), or its shape, if it
// has one.
std::optional<Shape> GetSpriteOrShape(const Entity& entity);

// @return The vertices that fully contain the shape.
// For a line, this is the start and end points.
// For polygons, this is equivalent to their vertices.
// For shapes with curved edges, this is the quad that contains them.
[[nodiscard]] std::vector<V2_float> GetWorldVertices(
	const Shape& shape, const Transform& transform
);

struct EdgeInfo {
	// If a shape has arced edges, this is set to true and edges is populated with the quad edges
	// that outline the shape.
	bool quad_approximation{ false };

	std::vector<Line> edges;
};

[[nodiscard]] EdgeInfo GetEdges(const Shape& shape, const Transform& transform);

} // namespace ptgn