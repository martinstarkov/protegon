#include <algorithm>
#include <vector>

#include "components/sprite.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/window.h"
#include "input/input_handler.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/vfx/light.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class Shadow {
public:
	V2_float origin;

	static void Draw(impl::RenderData& ctx, const Entity& entity) {
		PTGN_ASSERT(entity.Has<Polygon>());

		impl::ShapeDrawInfo info{ entity };

		const auto& polygon{ entity.Get<Polygon>() };

		info.state.blend_mode = BlendMode::None;
		info.tint			  = color::Black;

		// We need at least 3 points to form a triangle
		if (polygon.vertices.size() < 3) {
			return;
		}

		// Use the first point as the fan origin
		const V2_float& origin = entity.Get<Shadow>().origin;

		for (size_t i = 0; i < polygon.vertices.size(); ++i) {
			const V2_float& a = polygon.vertices[i];
			const V2_float& b = polygon.vertices[(i + 1) % polygon.vertices.size()];
			ctx.AddTriangle(
				info.transform, Triangle{ origin, a, b }, info.tint, info.depth, info.line_width,
				info.state
			);
		}

		// ctx.AddPolygon(info.transform, polygon, info.tint, info.depth, info.line_width,
		// info.state);
	}
};

PTGN_DRAWABLE_REGISTER(Shadow);

class LightMap {
public:
	static void Filter(RenderTarget& render_target) {
		auto& display_list{ render_target.GetDisplayList() };
		SortShadows(display_list);
	}

private:
	static void SortShadows(std::vector<Entity>& entities) {
		std::stable_sort(entities.begin(), entities.end(), [](const Entity& a, const Entity& b) {
			const bool a_is_shadow = a.Has<Shadow>();
			const bool b_is_shadow = b.Has<Shadow>();
			if (a_is_shadow != b_is_shadow) {
				return a_is_shadow; // false (0) comes before true (1)
			}

			const bool a_is_light = a.Has<impl::LightProperties>();
			const bool b_is_light = b.Has<impl::LightProperties>();
			if (a_is_light != b_is_light) {
				return !a_is_light; // true (1) comes before false (0)
			}

			// Otherwise, preserve order
			return false;
		});
	}
};

PTGN_DRAW_FILTER_REGISTER(LightMap);

namespace geometry {
// Simple 2d vector type
template <typename T>
struct vector2 {
	template <typename VectorType, typename ReturnType>
	using if_vector = typename std::enable_if<!std::is_scalar<VectorType>::value, ReturnType>::type;

	template <typename ScalarType, typename ReturnType>
	using if_scalar = typename std::enable_if<std::is_scalar<ScalarType>::value, ReturnType>::type;

	struct {
		T x, y;
	};

	vector2() {}

	vector2(T x, T y) : x(x), y(y) {}

	explicit vector2(const T& scalar) : x(scalar), y(scalar) {}

	// allow copy
	vector2(const vector2<T>&)			  = default;
	vector2& operator=(const vector2<T>&) = default;

	// Mutable operations
	template <typename VectorType>
	vector2& operator+=(VectorType other) {
		x += other.x;
		y += other.y;
		return *this;
	}

	template <typename VectorType>
	vector2& operator-=(VectorType other) {
		x -= other.x;
		y -= other.y;
		return *this;
	}

	template <typename VectorType>
	if_vector<VectorType, vector2&> operator*=(VectorType other) {
		x *= other.x;
		y *= other.y;
		return *this;
	}

	template <typename VectorType>
	if_vector<VectorType, vector2&> operator/=(VectorType other) {
		x /= other.x;
		y /= other.y;
		return *this;
	}

	template <typename ScalarType>
	if_scalar<ScalarType, vector2&> operator*=(ScalarType scalar) {
		x *= scalar;
		y *= scalar;
		return *this;
	}

	template <typename ScalarType>
	if_scalar<ScalarType, vector2&> operator/=(ScalarType scalar) {
		x /= scalar;
		y /= scalar;
		return *this;
	}

	// immutable operations
	template <typename VectorType>
	vector2 operator+(VectorType other) const {
		return { x + other.x, y + other.y };
	}

	template <typename VectorType>
	vector2 operator-(VectorType other) const {
		return { x - other.x, y - other.y };
	}

	template <typename VectorType>
	if_vector<VectorType, vector2> operator*(VectorType other) const {
		return { x * other.x, y * other.y };
	}

	template <typename VectorType>
	if_vector<VectorType, vector2> operator/(VectorType other) const {
		return { x / other.x, y / other.y };
	}

	template <typename ScalarType>
	if_scalar<ScalarType, vector2> operator*(ScalarType scalar) const {
		return { x * scalar, y * scalar };
	}

	template <typename ScalarType>
	friend if_scalar<ScalarType, vector2> operator*(ScalarType scalar, vector2 vector) {
		return { vector.x * scalar, vector.y * scalar };
	}

	template <typename ScalarType>
	if_scalar<ScalarType, vector2> operator/(ScalarType scalar) const {
		return { x / scalar, y / scalar };
	}

	friend vector2<T> operator-(vector2<T> vec) {
		return { -vec.x, -vec.y };
	}

	// comparison operators
	template <typename VectorType>
	bool operator==(VectorType other) const {
		return x == other.x && y == other.y;
	}

	template <typename VectorType>
	bool operator!=(VectorType other) const {
		return x != other.x || y != other.y;
	}
};

/** Calculate standard dot product.
 * @param a vector
 * @param b vector
 * @return dot product of the 2 vectors
 */
template <typename Vector>
auto dot(Vector a, Vector b) {
	return a.x * b.x + a.y * b.y;
}

/** Calculate squared length of a vector.
 * @param vector
 * @return squared length of the vector
 */
template <typename Vector>
auto length_squared(Vector vector) {
	return dot(vector, vector);
}

/** Squared distance of 2 points.
 * @param a point
 * @param b point
 * @return squared distance of the 2 points
 */
template <typename Vector>
auto distance_squared(Vector a, Vector b) {
	return length_squared(a - b);
}

/** Return orthogonal 2D vector.
 * @param vector
 * @return vector orthogonal to the argument
 */
template <typename Vector>
Vector normal(Vector vector) {
	return { -vector.y, vector.x };
}

/** Calculate det([a_x, b.x; a_y, b_y]).
 * @param a vector
 * @param b vector
 * @return det([a_x, b.x; a_y, b_y])
 */
template <typename Vector>
auto cross(Vector a, Vector b) {
	return a.x * b.y - a.y * b.x;
}

/** Normalize a floating point vector to have an unit length.
 * @param vector to normalize
 * @return normalized vector.
 *         If the vector is 0, it will return a 0 vector.
 *         If the vector is non-zero, it will return a vector with the same
 *         direction and unit length.
 */
template <typename Vector>
Vector normalize(Vector vector) {
	using value_type = typename std::decay<decltype(vector.x)>::type;

	value_type length = std::sqrt(dot(vector, vector));
	if (std::abs(length) < std::numeric_limits<value_type>::epsilon()) {
		return vector;
	}
	vector /= length;
	return vector;
}

/** Print a vector to the output stream.
 * @param output stream
 * @param vector to print
 * @return reference to given output stream
 */
template <typename T>
std::ostream& operator<<(std::ostream& output, vector2<T> vector) {
	output << "[" << vector.x << ", " << vector.y << "]";
	return output;
}

using vec2 = vector2<float>;
} // namespace geometry

namespace geometry {
inline bool approx_equal(float a, float b, float epsilon = std::numeric_limits<float>::epsilon()) {
	return std::abs(a - b) <= std::max(std::abs(a), std::abs(b)) * epsilon;
}

inline bool strictly_less(float a, float b, float epsilon = std::numeric_limits<float>::epsilon()) {
	return (b - a) > std::max(std::abs(a), std::abs(b)) * epsilon;
}

template <typename T>
bool approx_equal(vector2<T> a, vector2<T> b, T epsilon = std::numeric_limits<T>::epsilon()) {
	return approx_equal(a.x, b.x, epsilon) && approx_equal(a.y, b.y, epsilon);
}

template <typename T>
bool strictly_less(vector2<T>& a, vector2<T> b, T epsilon = std::numeric_limits<T>::epsilon()) {
	return strictly_less(a.x, b.x, epsilon) && strictly_less(a.y, b.y, epsilon);
}
} // namespace geometry

namespace geometry {
enum class orientation {
	left_turn  = 1,
	right_turn = -1,
	collinear  = 0
};

/** Compute orientation of 3 points in a plane.
 * @param a first point
 * @param b second point
 * @param c third point
 * @return orientation of the points in the plane (left turn, right turn
 *         or collinear)
 */
template <typename Vector>
orientation compute_orientation(Vector a, Vector b, Vector c) {
	auto det = cross(b - a, c - a);
	return static_cast<orientation>(
		static_cast<int>(strictly_less(0.f, det)) - static_cast<int>(strictly_less(det, 0.f))
	);
}

template <typename Vector>
struct line_segment {
	Vector a, b;

	line_segment() {}

	line_segment(Vector a, Vector b) : a(a), b(b) {}

	line_segment(const line_segment&)					 = default;
	line_segment& operator=(const line_segment& segment) = default;
};

template <typename Vector>
struct ray {
	Vector origin;
	Vector direction;

	ray() {}

	ray(Vector origin, Vector direction) : origin(origin), direction(direction) {}

	/** Find the nearest intersection point of ray and line segment.
	 * @param segment
	 * @param out_point reference to a variable where the nearest
	 *        intersection point will be stored (can be changed even
	 *        when there is no intersection)
	 * @return true iff the ray intersects the line segment
	 */
	bool intersects(const line_segment<Vector>& segment, Vector& out_point) const {
		auto ao	 = origin - segment.a;
		auto ab	 = segment.b - segment.a;
		auto det = cross(ab, direction);
		if (approx_equal(det, 0.f)) {
			auto abo = compute_orientation(segment.a, segment.b, origin);
			if (abo != orientation::collinear) {
				return false;
			}
			auto dist_a = dot(ao, direction);
			auto dist_b = dot(origin - segment.b, direction);

			if (dist_a > 0 && dist_b > 0) {
				return false;
			} else if ((dist_a > 0) != (dist_b > 0)) {
				out_point = origin;
			} else if (dist_a > dist_b) { // at this point, both distances are negative
				out_point = segment.a;	  // hence the nearest point is A
			} else {
				out_point = segment.b;
			}
			return true;
		}

		auto u = cross(ao, direction) / det;
		if (strictly_less(u, 0.f) || strictly_less(1.f, u)) {
			return false;
		}

		auto t	  = -cross(ab, ao) / det;
		out_point = origin + t * direction;
		return approx_equal(t, 0.f) || t > 0;
	}
};
} // namespace geometry

namespace geometry {
/* Compare 2 line segments based on their distance from given point
 * Assumes: (1) the line segments are intersected by some ray from the origin
 *          (2) the line segments do not intersect except at their endpoints
 *          (3) no line segment is collinear with the origin
 */
template <typename Vector>
struct line_segment_dist_comparer {
	using segment_type = line_segment<Vector>;

	Vector origin;

	explicit line_segment_dist_comparer(Vector origin) : origin(origin) {}

	/** Check whether the line segment x is closer to the origin than the
	 * line segment y.
	 * @param x line segment: left hand side of the comparison operator
	 * @param y line segment: right hand side of the comparison operator
	 * @return true iff x < y (x is closer than y)
	 */
	bool operator()(const segment_type& x, const segment_type& y) const {
		auto a = x.a, b = x.b;
		auto c = y.a, d = y.b;

		assert(
			compute_orientation(origin, a, b) != orientation::collinear &&
			"AB must not be collinear with the origin."
		);
		assert(
			compute_orientation(origin, c, d) != orientation::collinear &&
			"CD must not be collinear with the origin."
		);

		// sort the endpoints so that if there are common endpoints,
		// it will be a and c
		if (approx_equal(b, c) || approx_equal(b, d)) {
			std::swap(a, b);
		}
		if (approx_equal(a, d)) {
			std::swap(c, d);
		}

		// cases with common endpoints
		if (approx_equal(a, c)) {
			auto oad = compute_orientation(origin, a, d);
			auto oab = compute_orientation(origin, a, b);
			if (approx_equal(b, d) || oad != oab) {
				return false;
			}
			return compute_orientation(a, b, d) != compute_orientation(a, b, origin);
		}

		// cases without common endpoints
		auto cda = compute_orientation(c, d, a);
		auto cdb = compute_orientation(c, d, b);
		if (cdb == orientation::collinear && cda == orientation::collinear) {
			return distance_squared(origin, a) < distance_squared(origin, c);
		} else if (cda == cdb || cda == orientation::collinear || cdb == orientation::collinear) {
			auto cdo = compute_orientation(c, d, origin);
			return cdo == cda || cdo == cdb;
		} else {
			auto abo = compute_orientation(a, b, origin);
			return abo != compute_orientation(a, b, c);
		}
	}
};

// compare angles clockwise starting at the positive y axis
template <typename Vector>
struct angle_comparer {
	Vector vertex;

	explicit angle_comparer(Vector origin) : vertex(origin) {}

	bool operator()(const Vector& a, const Vector& b) const {
		auto is_a_left = strictly_less(a.x, vertex.x);
		auto is_b_left = strictly_less(b.x, vertex.x);
		if (is_a_left != is_b_left) {
			return is_b_left;
		}

		if (approx_equal(a.x, vertex.x) && approx_equal(b.x, vertex.x)) {
			if (!strictly_less(a.y, vertex.y) || !strictly_less(b.y, vertex.y)) {
				return strictly_less(b.y, a.y);
			}
			return strictly_less(a.y, b.y);
		}

		auto oa	 = a - vertex;
		auto ob	 = b - vertex;
		auto det = cross(oa, ob);
		if (approx_equal(det, 0.f)) {
			return length_squared(oa) < length_squared(ob);
		}
		return det < 0;
	}
};

template <typename Vector>
struct visibility_event {
	// events used in the visibility polygon algorithm
	enum event_type {
		start_vertex,
		end_vertex
	};

	event_type type;
	line_segment<Vector> segment;

	visibility_event() {}

	visibility_event(event_type type, const line_segment<Vector>& segment) :
		type(type), segment(segment) {}

	const auto& point() const {
		return segment.a;
	}
};

/** Calculate visibility polygon vertices in clockwise order.
 * Endpoints of the line segments (obstacles) can be ordered arbitrarily.
 * Line segments collinear with the point are ignored.
 * @param point - position of the observer
 * @param begin iterator of the list of line segments (obstacles)
 * @param end iterator of the list of line segments (obstacles)
 * @return vector of vertices of the visibility polygon
 */
template <typename Vector, typename InputIterator>
std::vector<Vector> visibility_polygon(Vector point, InputIterator begin, InputIterator end) {
	using segment_type			= line_segment<Vector>;
	using event_type			= visibility_event<Vector>;
	using segment_comparer_type = line_segment_dist_comparer<Vector>;

	segment_comparer_type cmp_dist{ point };
	std::set<segment_type, segment_comparer_type> state{ cmp_dist };
	std::vector<event_type> events;

	for (; begin != end; ++begin) {
		auto segment = *begin;

		// Sort line segment endpoints and add them as events
		// Skip line segments collinear with the point
		auto pab = compute_orientation(point, segment.a, segment.b);
		if (pab == orientation::collinear) {
			continue;
		} else if (pab == orientation::right_turn) {
			events.emplace_back(event_type::start_vertex, segment);
			events.emplace_back(event_type::end_vertex, segment_type{ segment.b, segment.a });
		} else {
			events.emplace_back(event_type::start_vertex, segment_type{ segment.b, segment.a });
			events.emplace_back(event_type::end_vertex, segment);
		}

		// Initialize state by adding line segments that are intersected
		// by vertical ray from the point
		auto a = segment.a, b = segment.b;
		if (a.x > b.x) {
			std::swap(a, b);
		}

		auto abp = compute_orientation(a, b, point);
		if (abp == orientation::right_turn &&
			(approx_equal(b.x, point.x) || (a.x < point.x && point.x < b.x))) {
			state.insert(segment);
		}
	}

	// sort events by angle
	angle_comparer<Vector> cmp_angle{ point };
	std::sort(events.begin(), events.end(), [&cmp_angle](auto&& a, auto&& b) {
		// if the points are equal, sort end vertices first
		if (approx_equal(a.point(), b.point())) {
			return a.type == event_type::end_vertex && b.type == event_type::start_vertex;
		}
		return cmp_angle(a.point(), b.point());
	});

	// find the visibility polygon
	std::vector<Vector> vertices;
	for (auto&& event : events) {
		if (event.type == event_type::end_vertex) {
			state.erase(event.segment);
		}

		if (state.empty()) {
			vertices.push_back(event.point());
		} else if (cmp_dist(event.segment, *state.begin())) {
			// Nearest line segment has changed
			// Compute the intersection point with this segment
			vec2 intersection;
			ray<Vector> ray{ point, event.point() - point };
			auto nearest_segment = *state.begin();
			auto intersects		 = ray.intersects(nearest_segment, intersection);
			assert(intersects && "Ray intersects line segment L iff L is in the state");

			if (event.type == event_type::start_vertex) {
				vertices.push_back(intersection);
				vertices.push_back(event.point());
			} else {
				vertices.push_back(event.point());
				vertices.push_back(intersection);
			}
		}

		if (event.type == event_type::start_vertex) {
			state.insert(event.segment);
		}
	}

	// remove collinear points
	auto top = vertices.begin();
	for (auto it = vertices.begin(); it != vertices.end(); ++it) {
		auto prev = top == vertices.begin() ? vertices.end() - 1 : top - 1;
		auto next = it + 1 == vertices.end() ? vertices.begin() : it + 1;
		if (compute_orientation(*prev, *it, *next) != orientation::collinear) {
			*top++ = *it;
		}
	}
	vertices.erase(top, vertices.end());
	return vertices;
}
} // namespace geometry

class ShadowScene : public Scene {
public:
	PointLight mouse_light;
	Entity polygon;

	std::vector<geometry::line_segment<geometry::vec2>> shadow_segments;

	void Enter() override {
		// game.renderer.SetBackgroundColor(color::White);
		SetBackgroundColor(color::LightBlue.WithAlpha(1));

		game.window.SetSetting(WindowSetting::Resizable);
		LoadResource("test", "resources/test1.jpg");

		auto sprite = CreateSprite(*this, "test", { -200, -200 });
		SetDrawOrigin(sprite, Origin::TopLeft);

		float intensity{ 0.5f };
		float radius{ 30.0f };
		float falloff{ 2.0f };

		float step{ 80 };

		// TODO: Shadows work when light renders to transparent target, but it breaks the scene
		// background color.
		auto rt = CreateRenderTarget(*this, ResizeMode::DisplaySize, color::Transparent);
		rt.SetDrawFilter<LightMap>();
		// TODO: Fix having to do this.
		SetBlendMode(rt, BlendMode::AddPremultipliedWithAlpha);

		polygon = CreatePolygon(
			*this, { 0, 0 }, { V2_float{ 0, -100 }, V2_float{ 100, 100 }, V2_float{ -100, 100 } },
			color::Blue, -1.0f
		);
		SetDraw<Shadow>(polygon);
		polygon.Add<Shadow>();

		V2_float s{ game.renderer.GetGameSize() };

		geometry::vec2 size{ s.x, s.y };

		shadow_segments.emplace_back(geometry::vec2{ 0, -100 }, geometry::vec2{ 100, 100 });
		shadow_segments.emplace_back(geometry::vec2{ 100, 100 }, geometry::vec2{ -100, 100 });
		shadow_segments.emplace_back(geometry::vec2{ -100, 100 }, geometry::vec2{ 0, -100 });
		shadow_segments.emplace_back(-size * 0.5f, geometry::vec2{ size.x * 0.5f, -size.y * 0.5f });
		shadow_segments.emplace_back(geometry::vec2{ size.x * 0.5f, -size.y * 0.5f }, size * 0.5f);
		shadow_segments.emplace_back(size * 0.5f, geometry::vec2{ -size.x * 0.5f, size.y * 0.5f });
		shadow_segments.emplace_back(geometry::vec2{ -size.x * 0.5f, size.y * 0.5f }, -size * 0.5f);

		rt.AddToDisplayList(polygon);

		const auto create_light = [&](const Color& color) {
			static int i = 1;
			auto light	 = CreatePointLight(
				  *this, V2_float{ -rt.GetCamera().GetViewportSize() * 0.5f } + V2_float{ i * step },
				  radius, color, intensity, falloff
			  );
			i++;
			return light;
		};

		rt.AddToDisplayList(create_light(color::Cyan));
		rt.AddToDisplayList(create_light(color::Green));
		rt.AddToDisplayList(create_light(color::Blue));
		rt.AddToDisplayList(create_light(color::Magenta));
		rt.AddToDisplayList(create_light(color::Yellow));
		rt.AddToDisplayList(create_light(color::Cyan));
		rt.AddToDisplayList(create_light(color::White));

		mouse_light = CreatePointLight(*this, {}, 50.0f, color::White, 0.8f, 1.0f);
		rt.AddToDisplayList(mouse_light);

		auto sprite2 = CreateSprite(*this, "test", { -200, 150 });
		SetDrawOrigin(sprite2, Origin::TopLeft);

		CreateRect(*this, { 200, 200 }, { 100, 100 }, color::Red, -1.0f, Origin::TopLeft);
	}

	void Update() override {
		auto pos{ input.GetMousePosition() };
		SetPosition(mouse_light, pos);
		geometry::vec2 posv{ pos.x, pos.y };
		auto verts{
			geometry::visibility_polygon(posv, shadow_segments.begin(), shadow_segments.end())
		};
		std::vector<V2_float> verts_2;
		for (const auto& v : verts) {
			verts_2.emplace_back(v.x, v.y);
		}
		if (verts_2.size() >= 3) {
			polygon.Get<Polygon>().vertices = verts_2;
			polygon.Get<Shadow>().origin	= pos;
		}
	}

	void Exit() override {
		json j = *this;
		// SaveJson(j, "resources/light_scene.json");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ShadowScene", { 800, 800 });
	game.scene.Enter<ShadowScene>("");
	return 0;
}