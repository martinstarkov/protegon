#pragma once

#include "math/Vector2.h"
#include "utility/TypeTraits.h"
#include "physics/Types.h"

// TODO: Use segments instead of lines where appropriate.

namespace ptgn {

namespace math {

// TODO: This is essentially the same as SignedTriangleArea
// Get the area of the triangle formed by points a, b, c.
template <typename T, typename S = double,
    tt::floating_point<S> = true>
inline S TriangleArea(const math::Vector2<T>& a,
                          const math::Vector2<T>& b,
                          const math::Vector2<T>& c) {
    const math::Vector2<S> ab{ b - a };
    const math::Vector2<S> ac{ c - a };
    return math::FastAbs(ab.Cross(ac)) / 2;
}

// Returns 2 times the signed triangle area. The result is positive if
// abc is ccw, negative if abc is cw, zero if abc is degenerate.
template <typename T>
inline T SignedTriangleArea(const Vector2<T>& a,
                            const Vector2<T>& b,
                            const Vector2<T>& c) {
    return (a.x - c.x) * (b.y - c.y) - (a.y - c.y) * (b.x - c.x);
}


namespace cs {

typedef int OutCode;

constexpr const int INSIDE = 0; // 0000
constexpr const int LEFT = 1;   // 0001
constexpr const int RIGHT = 2;  // 0010
constexpr const int BOTTOM = 4; // 0100
constexpr const int TOP = 8;    // 1000

// Compute the bit code for a point p (x, y) using the clip rectangle
// bounded diagonally by (xmin, ymin), and (xmax, ymax)

// ASSUME THAT xmax, xmin, ymax and ymin are global constants.

template <typename T = double,
    tt::floating_point<T> = true>
OutCode ComputeOutCode(const math::Vector2<T>& a,
                           const math::Vector2<T>& min,
                           const math::Vector2<T>& max) {
    OutCode code = INSIDE;  // initialised as being inside of clip window

    if (a.x < min.x)           // to the left of clip window
        code |= LEFT;
    else if (a.x > max.x)      // to the right of clip window
        code |= RIGHT;

    if (a.y < min.y)           // below the clip window
        code |= BOTTOM;
    else if (a.y > max.y)      // above the clip window
        code |= TOP;

    return code;
}

} // namespace cs

// Cohen–Sutherland clipping algorithm clips a line from
// p0 = (x0, y0) to p1 = (x1, y1) against a rectangle with 
// diagonal from (xmin, ymin) to (xmax, ymax).
template <typename T = double,
    tt::floating_point<T> = true>
bool CohenSutherlandLineClip(math::Vector2<T> p0,
                                 math::Vector2<T> p1,
                                 const math::Vector2<T>& min,
                                 const math::Vector2<T>& max) {
    // compute outcodes for P0, P1, and whatever point lies outside the clip rectangle
    cs::OutCode outcode0{ cs::ComputeOutCode<T>(p0, min, max) };
    cs::OutCode outcode1{ cs::ComputeOutCode<T>(p1, min, max) };
    bool accept{ false };

    while (true) {
        if (!(outcode0 | outcode1)) {
            // bitwise OR is 0: both points inside window; trivially accept and exit loop
            accept = true;
            break;
        } else if (outcode0 & outcode1) {
            // bitwise AND is not 0: both points share an outside zone (LEFT, RIGHT, TOP,
            // or BOTTOM), so both must be outside window; exit loop (accept is false)
            break;
        } else {
            math::Vector2<T> p;
            // At least one endpoint is outside the clip rectangle; pick it.
            const cs::OutCode outcodeOut{ outcode1 > outcode0 ? outcode1 : outcode0 };

            // Now find the intersection point;
            // use formulas:
            //   slope = (y1 - y0) / (x1 - x0)
            //   x = x0 + (1 / slope) * (ym - y0), where ym is ymin or ymax
            //   y = y0 + slope * (xm - x0), where xm is xmin or xmax
            // No need to worry about divide-by-zero because, in each case, the
            // outcode bit being tested guarantees the denominator is non-zero
            if (outcodeOut & cs::TOP) {           // point is above the clip window
                p.x = p0.x + (p1.x - p0.x) * (max.y - p0.y) / (p1.y - p0.y);
                p.y = max.y;
            } else if (outcodeOut & cs::BOTTOM) { // point is below the clip window
                p.x = p0.x + (p1.x - p0.x) * (min.y - p0.y) / (p1.y - p0.y);
                p.y = min.y;
            } else if (outcodeOut & cs::RIGHT) {  // point is to the right of clip window
                p.y = p0.y + (p1.y - p0.y) * (max.x - p0.x) / (p1.x - p0.x);
                p.x = max.x;
            } else if (outcodeOut & cs::LEFT) {   // point is to the left of clip window
                p.y = p0.y + (p1.y - p0.y) * (min.x - p0.x) / (p1.x - p0.x);
                p.x = min.x;
            }
            // Now we move outside point to intersection point to clip
            // and get ready for next pass.
            if (outcodeOut == outcode0) {
                p0.x = p.x;
                p0.y = p.y;
                outcode0 = cs::ComputeOutCode<T>(p0, min, max);
            } else {
                p1.x = p.x;
                p1.y = p.y;
                outcode1 = cs::ComputeOutCode<T>(p1, min, max);
            }
        }
    }
    return accept;
}

// Source: file:///C:/Users/Martin/Desktop/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 130.
// Returns the squared distance between point and segment line_origin -> line_destination.
template <typename T, typename S = double,
    tt::floating_point<S> = true>
static S PointToLineSquareDistance(const Point<T>& a,
                                   const Line<T>& b) {
    const math::Vector2<S> ab{ b.Direction() };
    const math::Vector2<S> ac{ a - b.origin };
    const math::Vector2<S> bc{ a - b.destination };
    const S e{ ac.Dot(ab) };
    // Handle cases where c projects outside ab
    if (e < 0 || math::Compare(e, 0)) return ac.Dot(ac);
    const S f{ ab.Dot(ab) };
    if (e > f || math::Compare(e, f)) return bc.Dot(bc);
    // Handle cases where c projects onto ab
    return ac.Dot(ac) - e * e / f;
}

// Source: file:///C:/Users/Martin/Desktop/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 129.
// Given segment ab and point c, computes closest point out_d on ab.
// Also returns out_t for the position of out_d, out_d(out_t)= a + out_t * (b - a)
template <typename T, typename S = double,
    tt::floating_point<S> = true>
static void ClosestPointLine(const Point<T>& a,
                             const Line<T>& b,
                             S& out_t,
                             math::Vector2<S>& out_d) {
    const math::Vector2<S> ab{ b.Direction() };
    // Project c onto ab, but deferring divide by Dot(ab, ab)
    out_t = (a - b.origin).Dot(ab);
    if (out_t < 0 || math::Compare(out_t, 0)) {
        // c projects outside the [a,b] interval, on the a side; clamp to a
        out_t = 0;
        out_d = b.origin;
    } else {
        S denom = ab.Dot(ab); // Always nonnegative since denom = ||ab||^2
        if (out_t > denom || math::Compare(out_t, denom)) {
            // c projects outside the [a,b] interval, on the b side; clamp to b
            out_t = 1;
            out_d = b.destination;
        } else {
            // c projects inside the [a,b] interval; must do deferred divide now
            out_t = out_t / denom;
            out_d = b.origin + out_t * ab;
        }
    }
}

// Given an infinite line line_origin->line_destination and point, computes closest point out_d on ab.
// Also returns out_t for the parametric position of out_d, out_d(t)= a + out_t * (b - a)
template <typename S = double, typename T,
    std::enable_if_t<std::is_floating_point_v<S>, bool> = true>
inline void ClosestPointInfiniteLine(const math::Vector2<T>& point, const math::Vector2<T>& line_origin, const math::Vector2<T>& line_destination, S& out_t, math::Vector2<S>& out_d) {
    math::Vector2<S> ab{ line_destination - line_origin };
    // Project c onto ab, but deferring divide by Dot(ab, ab)
    out_t = (point - line_origin).Dot(ab) / ab.Dot(ab);
    out_d = line_origin + out_t * ab;
}

template <typename T>
inline T SquareDistancePointAABB(const Point<T>& a, const AABB<T>& b) {
    T dist2{ 0 };
    const Vector2<T> max{ b.Max() };
    for (std::size_t i{ 0 }; i < 2; ++i) {
        const T v{ a[i] };
        if (v < b.position[i]) dist2 += (b.position[i] - v) * (b.position[i] - v);
        if (v > max[i]) dist2 += (v - max[i]) * (v - max[i]);
    }
    return dist2;
}

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Pages 149-150.
// Computes closest points C1 and C2 of S1(s)=P1+s*(Q1-P1) and
// S2(t)=P2+t*(Q2-P2), returning s and t. 
// Function return is squared distance between between S1(s) and S2(t)
template <typename S = double, typename T,
    tt::floating_point<S> = true>
static S ClosestPointLineLine(const Line<T>& a,
                              const Line<T>& b,
                              S& out_s,
                              S& out_t,
                              math::Vector2<S>& out_c1,
                              math::Vector2<S>& out_c2) {
    static_assert(!tt::is_narrowing_v<T, S>);
    const math::Vector2<S> d1{ a.destination - a.origin }; // Direction vector of segment S1
    const math::Vector2<S> d2{ b.destination - b.origin }; // Direction vector of segment S2
    const math::Vector2<S> r{ a.origin - b.origin };
    const S z{ d1.Dot(d1) }; // Squared length of segment S1, always nonnegative
    const S e{ d2.Dot(d2) }; // Squared length of segment S2, always nonnegative
    const S f{ d2.Dot(r) };
    // Check if either or both segments degenerate into points
    if (z <= math::epsilon<S> && e <= math::epsilon<S>) {
        // Both segments degenerate into points
        out_s = out_t = 0;
        out_c1 = a.origin;
        out_c2 = b.origin;
        const math::Vector2<S> subtraction{ out_c1 - out_c2 };
        return subtraction.Dot(subtraction);
    }
    const S lower{ 0 };
    const S upper{ 1 };
    if (z <= math::epsilon<S>) {
        // First segment degenerates into a point
        out_s = 0;
        out_t = f / e; // out_s = 0 => out_t = (b*out_s + f) / e = f / e
        out_t = std::clamp(out_t, lower, upper);
    } else {
        const S c{ d1.Dot(r) };
        if (e <= math::epsilon<S>) {
            // Second segment degenerates into a point
            out_t = 0;
            out_s = std::clamp(-c / z, lower, upper); // out_t = 0 => out_s = (b*out_t - c) / z = -c / z
        } else {
            // The general nondegenerate case starts here
            const S b{ d1.Dot(d2) };
            const S denom{ z * e - b * b }; // Always nonnegative
            // If segments not parallel, compute closest point on L1 to L2 and
            // clamp to segment S1. Else pick arbitrary out_s (here 0)
            if (!math::Compare(denom, 0))
                out_s = std::clamp((b * f - c * e) / denom, lower, upper);
            else
                out_s = 0;

            const S tnom{ b * out_s + f };
            if (tnom < 0) {
                out_t = 0;
                out_s = std::clamp(-c / z, lower, upper);
            } else if (tnom > e) {
                out_t = 1;
                out_s = std::clamp((b - c) / z, lower, upper);
            } else {
                out_t = tnom / e;
            }
            /*
            // Compute point on L2 closest to S1(out_s) using
            // out_t = Dot((P1 + D1*out_s) - P2,D2) / Dot(D2,D2) = (b*out_s + f) / e
            out_t = (b * out_s + f) / e;
            // If out_t in [lower, upper] done. Else clamp out_t, recompute out_s for the new value
            // of out_t using out_s = Dot((P2 + D2*out_t) - P1,D1) / Dot(D1,D1)= (out_t*b - c) / z
            // and clamp out_s to [lower, upper]
            if (out_t < 0) {
                out_t = 0;
                out_s = Clamp(-c / z, lower, upper);
            } else if (out_t > 1) {
                out_t = 1;
                out_s = Clamp((b - c) / z, lower, upper);
            }
            */
        }
    }
    out_c1 = a.origin + d1 * out_s;
    out_c2 = b.origin + d2 * out_t;
    const math::Vector2<S> subtraction{ out_c1 - out_c2 };
    return subtraction.Dot(subtraction);
}

} // namespace math

} // namespace ptgn