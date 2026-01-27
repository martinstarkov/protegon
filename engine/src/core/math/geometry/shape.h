#pragma once

#include <optional>
#include <variant>
#include <vector>

#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/json/fwd.h"

namespace ptgn {

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

	bool operator==(const InteractiveShape&) const = default;

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

	bool operator==(const ColliderShape&) const = default;

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

	bool operator==(const Shape&) const = default;

	// friend void to_json(json& j, const Shape& shape);
	// friend void from_json(const json& j, Shape& shape);
};

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