#include "protegon/collision.h"

#include <limits> // std::numeric_limits
#include <array>  // std::array
#include <cmath>  // std::sqrtf

#include "protegon/math.h"

namespace ptgn {

namespace impl {

float SquareDistancePointRectangle(const Point<float>& a,
                                   const Rectangle<float>& b) {
    float dist2{ 0.0f };
    const V2_float max{ b.Max() };
    for (std::size_t i{ 0 }; i < 2; ++i) {
        const float v{ a[i] };
        if (v < b.position[i])
            dist2 += (b.position[i] - v) * (b.position[i] - v);
        if (v > max[i])
            dist2 += (v - max[i]) * (v - max[i]);
    }
    return dist2;
}

float ParallelogramArea(const Point<float>& a,
                        const Point<float>& b,
                        const Point<float>& c) {
    return (a - c).Cross(b - c);
}

void ClosestPointSegment(const Point<float>& a,
                         const Segment<float>& b,
                         float& out_t,
                         Point<float>& out_d) {
    const V2_float ab{ b.Direction() };
    // Project A onto ab, but deferring divide by Dot(ab, ab)
    out_t = (a - b.a).Dot(ab);
    if (out_t <= 0.0f) {
        // A projects outside the [B.a,B.b] interval, on the B.a side; clamp to B.a
        out_t = 0.0f;
        out_d = b.a;
    } else {
        const float denom{ ab.Dot(ab) }; // Always nonnegative since denom = ||ab||^2
        if (out_t >= denom) {
            // A projects outside the [B.a,B.b] interval, on the B.b side; clamp to B.b
            out_t = 1.0f;
            out_d = b.b;
        } else {
            // A projects inside the [B.a,B.b] interval; must do deferred divide now
            out_t = out_t / denom;
            out_d = b.a + out_t * ab;
        }
    }
}

} // namespace impl

namespace overlap {

bool RectangleRectangle(const Rectangle<float>& a,
                        const Rectangle<float>& b) {
    if (a.position.x + a.size.x < b.position.x || a.position.x > b.position.x + b.size.x)
        return false;
    if (a.position.y + a.size.y < b.position.y || a.position.y > b.position.y + b.size.y)
        return false;
    return true;
}

bool CircleCircle(const Circle<float>& a,
                  const Circle<float>& b) {
    const V2_float dist{ a.center - b.center };
    const float dist2{ dist.Dot(dist) };
    const float rad_sum{ a.radius + b.radius };
    const float rad_sum2{ rad_sum * rad_sum };
    return dist2 < rad_sum2 || NearlyEqual(dist2, rad_sum2);
}

bool CircleRectangle(const Circle<float>& a,
                     const Rectangle<float>& b) {
    const float dist2{ impl::SquareDistancePointRectangle(a.center, b) };
    const float rad2{ a.radius * a.radius };
    return dist2 < rad2 || NearlyEqual(dist2, rad2);
}

bool PointRectangle(const Point<float>& a,
                    const Rectangle<float>& b) {
    return RectangleRectangle({ a, {} }, b);
}

bool PointCircle(const Point<float>& a,
                 const Circle<float>& b) {
    return CircleCircle({ a, 0.0f }, b);
}

bool PointSegment(const Point<float>& a,
                  const Segment<float>& b) {
    const V2_float ab{ b.Direction() };
    const V2_float ac{ a - b.a };
    const V2_float bc{ a - b.b };

    const float e{ ac.Dot(ab) };
    // Handle cases where c projects outside ab
    if (e < 0 || NearlyEqual(e, 0.0f))
        return NearlyEqual(ac.x, 0.0f) && NearlyEqual(ac.y, 0.0f);

    const float f{ ab.Dot(ab) };
    if (e > f || NearlyEqual(e, f))
        return NearlyEqual(bc.x, 0.0f) && NearlyEqual(bc.y, 0.0f);

    // Handle cases where c projects onto ab
    return NearlyEqual(ac.Dot(ac) * f, e * e);
}

bool SegmentRectangle(const Segment<float>& a,
                      const Rectangle<float>& b) {
    const Vector2<float> e{ b.size };
    const Vector2<float> d{ a.Direction() };
    const Vector2<float> m{ a.a + a.b - 2 * b.position - b.size };

    // Try world coordinate axes as separating axes
    float adx{ FastAbs(d.x) };
    if (FastAbs(m.x) > e.x + adx)
        return false;

    float ady{ FastAbs(d.y) };
    if (FastAbs(m.y) > e.y + ady)
        return false;

    // Add in an epsilon term to counteract arithmetic errors when segment is
    // (near) parallel to a coordinate axis (see text for detail)
    adx += epsilon<float>;
    ady += epsilon<float>;

    // Try cross products of segment direction vector with coordinate axes
    if (FastAbs(m.x * d.y - m.y * d.x) > e.x * ady + e.y * adx) 
        return false;

    // No separating axis found; segment must be overlapping AABB
    return true;

    // Alternative method:
    // Source: https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
}

bool SegmentCircle(const Segment<float>& a,
                   const Circle<float>& b) {
    // If the line is inside the circle entirely, exit early.
    if (PointCircle(a.a, b) && 
        PointCircle(a.b, b))
        return true;

    float min_dist2{ std::numeric_limits<float>::infinity() };
    const float rad2{ b.radius * b.radius };

    // O is the circle center, P is the line origin, Q is the line destination.
    const V2_float OP{ a.a - b.center };
    const V2_float OQ{ a.b - b.center };
    const V2_float PQ{ a.Direction() };

    const float OP_dist2{ OP.Dot(OP) };
    const float OQ_dist2{ OQ.Dot(OQ) };
    const float max_dist2{ std::max(OP_dist2, OQ_dist2) };

    if (OP.Dot(-PQ) > 0.0f && OQ.Dot(PQ) > 0.0f) {
        const float triangle_area{ FastAbs(impl::ParallelogramArea(b.center, a.a, a.b)) / 2.0f };
        min_dist2 = 4.0f * triangle_area * triangle_area / PQ.Dot(PQ);
    } else {
        min_dist2 = std::min(OP_dist2, OQ_dist2);
    }
    return (min_dist2 < rad2 || NearlyEqual(min_dist2, rad2)) &&
           (max_dist2 > rad2 || NearlyEqual(max_dist2, rad2));
}

bool SegmentSegment(const Segment<float>& a,
                    const Segment<float>& b) {
    // Sign of areas correspond to which side of ab points c and d are
    const float a1{ impl::ParallelogramArea(a.a, a.b, b.b) }; // Compute winding of abd (+ or -)
    const float a2{ impl::ParallelogramArea(a.a, a.b, b.a) }; // To intersect, must have sign opposite of a1
    // If c and d are on different sides of ab, areas have different signs
    bool polarity_diff{ false };
    bool collinear{ false };
    // Same as above but for floating points.
    polarity_diff = a1 * a2 < 0.0f;
    collinear = NearlyEqual(a1, 0.0f) || NearlyEqual(a2, 0.0f);
    // For integral implementation use this instead of the above two lines:
    //if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
    //	// Second part for difference in polarity.
    //	polarity_diff = (a1 ^ a2) < 0;
    //	collinear = a1 == 0 || a2 == 0;
    //}
    if (!collinear && polarity_diff) {
        // Compute signs for a and b with respect to segment cd
        const float a3{ impl::ParallelogramArea(b.a, b.b, a.a) }; // Compute winding of cda (+ or -)
        // Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
        // const T a4 = math::ParallelogramArea(c, d, b); // Must have opposite sign of a3
        const float a4{ a3 + a2 - a1 };
        // Points a and b on different sides of cd if areas have different signs
        // Segments intersect if true.
        bool intersect{ false };
        // If either is 0, the line is intersecting with the straight edge of the other line.
        // (i.e. corners with angles).
        // Check if a3 and a4 signs are different.
        intersect = a3 * a4 < 0.0f;
        collinear = NearlyEqual(a3, 0.0f) || NearlyEqual(a4, 0.0f);
        // For integral implementation use this instead of the above two lines:
        //if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
        //	intersect = (a3 ^ a4) < 0;
        //	collinear = a3 == 0 || a4 == 0;
        //}
        if (intersect) return true;
    }
    return collinear &&
        (PointSegment(b.b, a) ||
         PointSegment(b.a, a) ||
         PointSegment(a.a, b) ||
         PointSegment(a.b, b));
}

} // namespace overlap

namespace intersect {

bool CircleCircle(const Circle<float>& a,
                  const Circle<float>& b,
                  Collision& c) {
    c = {};

    const V2_float d{ b.center - a.center };
    const float dist2{ d.Dot(d) };
    const float r{ a.radius + b.radius };

    if (dist2 > r * r)
        return false;

    // Edge case where circle centers are in the same location.
    c.normal = { 0.0f, -1.0f }; // upward
    c.depth = r;

    if (dist2 > epsilon2<float>) {
        const float dist{ std::sqrtf(dist2) };
        c.normal = -d / dist;
        c.depth = r - dist;
    }
    return true;
}

bool RectangleRectangle(const Rectangle<float>& a,
                        const Rectangle<float>& b,
                        Collision& c) {
    c = {};

    const V2_float a_h{ a.Half() };
    const V2_float b_h{ b.Half() };
    const V2_float d{ b.position + b_h - (a.position + a_h) };
    const V2_float pen{ a_h + b_h - d.FastAbs() };

    if (pen.x < 0 || pen.y < 0 || NearlyEqual(pen.x, 0.0f) || NearlyEqual(pen.y, 0.0f))
        return false;

    if (NearlyEqual(d.x, 0.0f) && NearlyEqual(d.y, 0.0f)) {
        // Edge case where aabb centers are in the same location.
        c.normal = { 0.0f, -1.0f }; // upward
        c.depth = a_h.y + b_h.y;
    } else if (pen.y < pen.x) {
        c.normal = { 0.0f, -Sign(d.y) };
        c.depth = FastAbs(pen.y);
    } else {
        c.normal = { -Sign(d.x), 0.0f };
        c.depth = FastAbs(pen.x);
    }
    return true;
}

// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
bool CircleRectangle(const Circle<float>& a,
                     const Rectangle<float>& b,
                     Collision& c) {
    c = {};
    using Edge = std::pair<V2_float, V2_float>;

    V2_float top_right{ b.position.x + b.size.x, b.position.y };
    V2_float bottom_right{ b.position + b.size };
    V2_float bottom_left{ b.position.x, b.position.y + b.size.y };

    std::array<Edge, 4> edges;
    edges.at(0) = { b.position,   top_right };    // top
    edges.at(1) = { top_right,    bottom_right }; // right
    edges.at(2) = { bottom_right, bottom_left };  // bottom
    edges.at(3) = { bottom_left,  b.position };   // left

    float min_dist2{ std::numeric_limits<float>::infinity() };
    V2_float min_point;
    std::size_t side{ 0 };

    for (std::size_t i{ 0 }; i < edges.size(); ++i) {
        auto& [a_, b_] = edges[i];
        float t{};
        V2_float c1;
        impl::ClosestPointSegment(a.center, { a_, b_ }, t, c1);
        const V2_float d{ a.center - c1 };
        float dist2{ d.Dot(d) };
        if (dist2 < min_dist2) {
            side = i;
            min_dist2 = dist2;
            // Point on the AABB that was the closest.
            min_point = c1;
        }
    }

    bool inside{ overlap::PointRectangle(a.center, b) };

    if (!inside && min_dist2 > a.radius * a.radius)
        return false;

    if (NearlyEqual(min_dist2, 0.0f)) {
        // Circle is on one of the AABB edges.
        switch (side) {
            case 0:
                c.normal = { 0.0f, -1.0f }; // top
                break;
            case 1:
                c.normal = { 1.0f, 0.0f };  // right
                break;
            case 2:
                c.normal = { 0.0f, 1.0f };  // bottom
                break;
            case 3:
                c.normal = { -1.0f, 0.0f }; // left
                break;
        }
        c.depth = a.radius;
    } else {
        const V2_float d{ a.center - min_point };
        const float mag2{ d.Dot(d) };

        if (NearlyEqual(mag2, 0.0f)) {
            // Edge case where circle and aabb centers are in the same location.
            c.normal = { 0.0f, -1.0f }; // upward
            c.depth = a.radius;
        } else {
            const float mag{ std::sqrtf(mag2) };
            c.normal = d / mag;
            if (inside) {
                c.normal *= -1.0f;
                c.depth = a.radius + mag;
            } else {
                c.depth = a.radius - mag;
            }
        }

    }
    return true;
}

} // namespace intersect

namespace dynamic {



} // namespace dynamic

} // namespace ptgn