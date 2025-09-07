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
	static void Draw(impl::RenderData& ctx, const Entity& entity) {
		PTGN_ASSERT(entity.Has<Polygon>());

		impl::ShapeDrawInfo info{ entity };

		const auto& polygon{ entity.Get<Polygon>() };

		info.state.blend_mode = BlendMode::None;
		info.tint			  = color::Black;

		ctx.AddPolygon(info.transform, polygon, info.tint, info.depth, info.line_width, info.state);
	}
};

PTGN_DRAWABLE_REGISTER(Shadow);

class LightMap {
public:
	static void Filter(RenderTarget& render_target) {
		auto& display_list{ render_target.GetDisplayList() };
		SortShadowsToEnd(display_list);
	}

private:
	static void SortShadowsToEnd(std::vector<Entity>& entities) {
		std::stable_sort(entities.begin(), entities.end(), [](const Entity& a, const Entity& b) {
			const bool a_is_light = a.Has<impl::LightProperties>();
			const bool b_is_light = b.Has<impl::LightProperties>();
			if (a_is_light != b_is_light) {
				return a_is_light; // true (1) comes before false (0)
			}

			const bool a_is_shadow = a.Has<Shadow>();
			const bool b_is_shadow = b.Has<Shadow>();
			if (a_is_shadow != b_is_shadow) {
				return !a_is_shadow; // false (0) comes before true (1)
			}

			// Otherwise, preserve order
			return false;
		});
	}
};

PTGN_DRAW_FILTER_REGISTER(LightMap);

enum class VisibilityEventType {
	Start,
	End
};
enum class orientation {
	left_turn  = 1,
	right_turn = -1,
	collinear  = 0
};

inline bool NearlyEqual(float a, float b, float epsilon = std::numeric_limits<float>::epsilon()) {
	return std::abs(a - b) <= std::max(std::abs(a), std::abs(b)) * epsilon;
}

inline bool strictly_less(float a, float b, float epsilon = std::numeric_limits<float>::epsilon()) {
	return (b - a) > std::max(std::abs(a), std::abs(b)) * epsilon;
}

bool strictly_less(V2_float& a, V2_float b, float epsilon = std::numeric_limits<float>::epsilon()) {
	return strictly_less(a.x, b.x, epsilon) && strictly_less(a.y, b.y, epsilon);
}

/** Compute orientation of 3 points in a plane.
 * @param a first point
 * @param b second point
 * @param c third point
 * @return orientation of the points in the plane (left turn, right turn
 *         or collinear)
 */
orientation compute_orientation(V2_float a, V2_float b, V2_float c) {
	auto det = (b - a).Cross(c - a);
	return static_cast<orientation>(
		static_cast<int>(strictly_less(0.f, det)) - static_cast<int>(strictly_less(det, 0.f))
	);
}

bool NearlyEqual(V2_float a, V2_float b, float epsilon = std::numeric_limits<float>::epsilon()) {
	return a == b;
}

struct SegmentDistanceComparer {
	V2_float origin;

	explicit SegmentDistanceComparer(V2_float origin) : origin(origin) {}

	bool operator()(Line lhs, Line rhs) const {
		auto [a, b] = std::tie(lhs.start, lhs.end);
		auto [c, d] = std::tie(rhs.start, rhs.end);

		PTGN_ASSERT(
			(compute_orientation(origin, a, b) != orientation::collinear),
			"AB must not be collinear with origin."
		);
		PTGN_ASSERT(
			(compute_orientation(origin, c, d) != orientation::collinear),
			"CD must not be collinear with origin."
		);

		if (NearlyEqual(b, c) || NearlyEqual(b, d)) {
			std::swap(a, b);
		}
		if (NearlyEqual(a, d)) {
			std::swap(c, d);
		}

		if (NearlyEqual(a, c)) {
			auto oad = compute_orientation(origin, a, d);
			auto oab = compute_orientation(origin, a, b);
			if (NearlyEqual(b, d) || oad != oab) {
				return false;
			}
			return compute_orientation(a, b, d) != compute_orientation(a, b, origin);
		}

		auto cda = compute_orientation(c, d, a);
		auto cdb = compute_orientation(c, d, b);

		if (cda == orientation::collinear && cdb == orientation::collinear) {
			return (origin - a).MagnitudeSquared() < (origin - c).MagnitudeSquared();
		}

		if (cda == cdb || cda == orientation::collinear || cdb == orientation::collinear) {
			auto cdo = compute_orientation(c, d, origin);
			return cdo == cda || cdo == cdb;
		}

		auto abo = compute_orientation(a, b, origin);
		return abo != compute_orientation(a, b, c);
	}
};

// Clockwise angle comparison starting at positive Y axis
struct AngleComparer {
	V2_float origin;

	explicit AngleComparer(V2_float origin) : origin(origin) {}

	bool operator()(const V2_float& a, const V2_float& b) const {
		const bool is_a_left = a.x < origin.x;
		const bool is_b_left = b.x < origin.x;

		if (is_a_left != is_b_left) {
			return is_b_left;
		}

		if (NearlyEqual(a.x, origin.x) && NearlyEqual(b.x, origin.x)) {
			if (!strictly_less(a.y, origin.y) || !strictly_less(b.y, origin.y)) {
				return b.y < a.y;
			}
			return a.y < b.y;
		}

		const auto oa  = a - origin;
		const auto ob  = b - origin;
		const auto det = oa.Cross(ob);

		if (NearlyEqual(det, 0.f)) {
			return oa.MagnitudeSquared() < ob.MagnitudeSquared();
		}

		return det < 0;
	}
};

struct VisibilityEvent {
	VisibilityEventType type;
	Line segment;

	VisibilityEvent() = default;

	VisibilityEvent(VisibilityEventType type, const Line& segment) : type(type), segment(segment) {}

	const V2_float& point() const {
		return segment.start;
	}
};

struct ray {
	V2_float origin;
	V2_float direction;

	ray() {}

	ray(V2_float origin, V2_float direction) : origin(origin), direction(direction) {}

	/** Find the nearest intersection point of ray and line segment.
	 * @param segment
	 * @param out_point reference to a variable where the nearest
	 *        intersection point will be stored (can be changed even
	 *        when there is no intersection)
	 * @return true iff the ray intersects the line segment
	 */
	bool intersects(const Line& segment, V2_float& out_point) const {
		auto ao	 = origin - segment.start;
		auto ab	 = segment.end - segment.start;
		auto det = ab.Cross(direction);
		if (NearlyEqual(det, 0.f)) {
			auto abo = compute_orientation(segment.start, segment.end, origin);
			if (abo != orientation::collinear) {
				return false;
			}
			auto dist_a = ao.Dot(direction);
			auto dist_b = (origin - segment.end).Dot(direction);

			if (dist_a > 0 && dist_b > 0) {
				return false;
			} else if ((dist_a > 0) != (dist_b > 0)) {
				out_point = origin;
			} else if (dist_a > dist_b) {  // at this point, both distances are negative
				out_point = segment.start; // hence the nearest point is A
			} else {
				out_point = segment.end;
			}
			return true;
		}

		auto u = ao.Cross(direction) / det;
		if (strictly_less(u, 0.f) || strictly_less(1.f, u)) {
			return false;
		}

		auto t	  = -ab.Cross(ao) / det;
		out_point = origin + t * direction;
		return NearlyEqual(t, 0.f) || t > 0;
	}
};

class ShadowScene : public Scene {
public:
	PointLight mouse_light;
	Entity polygon;

	inline std::vector<V2_float> visibility_polygon(V2_float point, std::vector<Line> segments) {
		SegmentDistanceComparer cmp_dist{ point };
		std::set<Line, SegmentDistanceComparer> active_segments{ cmp_dist };
		std::vector<VisibilityEvent> events;

		// Step 1: Process all segments into events
		for (auto& seg : segments) {
			auto [a, b] = std::tie(seg.start, seg.end);
			auto orient = compute_orientation(point, a, b);

			if (orient == orientation::collinear) {
				continue;
			}

			if (orient == orientation::right_turn) {
				events.emplace_back(VisibilityEventType::Start, seg);
				events.emplace_back(VisibilityEventType::End, Line{ b, a });
			} else {
				events.emplace_back(VisibilityEventType::Start, Line{ b, a });
				events.emplace_back(VisibilityEventType::End, seg);
			}

			if (a.x > b.x) {
				std::swap(a, b);
			}

			if (compute_orientation(a, b, point) == orientation::right_turn &&
				(NearlyEqual(b.x, point.x) || (a.x < point.x && point.x < b.x))) {
				active_segments.insert(seg);
			}
		}

		// Step 2: Sort events by angle around the origin
		AngleComparer cmp_angle{ point };
		std::sort(events.begin(), events.end(), [&](const auto& a, const auto& b) {
			if (NearlyEqual(a.point(), b.point())) {
				return a.type == VisibilityEventType::End && b.type == VisibilityEventType::Start;
			}
			return cmp_angle(a.point(), b.point());
		});

		// Step 3: Sweep events and construct polygon
		std::vector<V2_float> output;

		for (auto event : events) {
			if (event.type == VisibilityEventType::End) {
				active_segments.erase(event.segment);
			}

			if (active_segments.empty()) {
				output.emplace_back(event.point());
			} else if (auto segment_begin{ *active_segments.begin() };
					   cmp_dist(event.segment, segment_begin)) {
				// Nearest segment has changed
				auto& nearest = *active_segments.begin();
				ray r{ point, event.point() - point };
				V2_float intersection;
				const bool hit = r.intersects(nearest, intersection);
				PTGN_ASSERT(hit, "Ray must intersect with closest segment");

				if (event.type == VisibilityEventType::Start) {
					output.emplace_back(intersection);
					output.emplace_back(event.point());
				} else {
					output.emplace_back(event.point());
					output.emplace_back(intersection);
				}
			}

			if (event.type == VisibilityEventType::Start) {
				active_segments.insert(event.segment);
			}
		}

		// Step 4: Remove collinear points
		std::vector<V2_float> cleaned;
		cleaned.reserve(output.size());

		for (size_t i = 0; i < output.size(); ++i) {
			const V2_float& prev = output[(i + output.size() - 1) % output.size()];
			const V2_float& curr = output[i];
			const V2_float& next = output[(i + 1) % output.size()];

			if (compute_orientation(prev, curr, next) != orientation::collinear) {
				cleaned.emplace_back(curr);
			}
		}

		return cleaned;
	}

	std::vector<Line> shadow_segments;

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

		auto rt = CreateRenderTarget(*this, ResizeMode::DisplaySize, color::Transparent);
		rt.SetDrawFilter<LightMap>();
		SetBlendMode(rt, BlendMode::AddPremultipliedWithAlpha);

		polygon = CreatePolygon(
			*this, { 0, 0 }, { V2_float{ 0, -100 }, V2_float{ 100, 100 }, V2_float{ -100, 100 } },
			color::Blue, -1.0f
		);
		SetDraw<Shadow>(polygon);
		polygon.Add<Shadow>();

		auto size{ game.renderer.GetGameSize() };

		shadow_segments.emplace_back(Line{ V2_float{ 0, -100 }, V2_float{ 100, 100 } });
		shadow_segments.emplace_back(Line{ V2_float{ 100, 100 }, V2_float{ -100, 100 } });
		shadow_segments.emplace_back(Line{ V2_float{ -100, 100 }, V2_float{ 0, -100 } });
		shadow_segments.emplace_back(Line{ -size * 0.5f, V2_float{ size.x * 0.5f, -size.y * 0.5f } }
		);
		shadow_segments.emplace_back(Line{ V2_float{ size.x * 0.5f, -size.y * 0.5f }, size * 0.5f }
		);
		shadow_segments.emplace_back(Line{ size * 0.5f, V2_float{ -size.x * 0.5f, size.y * 0.5f } }
		);
		shadow_segments.emplace_back(Line{ V2_float{ -size.x * 0.5f, size.y * 0.5f }, -size * 0.5f }
		);

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
		polygon.Get<Polygon>().vertices = visibility_polygon(pos, shadow_segments);
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