#include "Collision.h"

#include "Circle.h"
#include "Polygon.h"
#include "Manifold.h"

CollisionCallback Dispatch[static_cast<int>(ShapeType::COUNT)][static_cast<int>(ShapeType::COUNT)] = { { CircleVsCircle, CircleVsPolygon }, { PolygonVsCircle, PolygonVsPolygon } };

void CircleVsCircle(Manifold* m, Body* a, Body* b) {
    Circle* A = reinterpret_cast<Circle*>(a->shape);
    Circle* B = reinterpret_cast<Circle*>(b->shape);

    // Calculate translational vector, which is normal
    auto normal = b->position - a->position;

    auto dist_sqr = normal.MagnitudeSquared();
    auto radius = A->GetRadius() + B->GetRadius();

    // Not in contact
    if (dist_sqr >= radius * radius) {
        m->contact_count = 0;
        return;
    }

    auto distance = std::sqrt(dist_sqr);

    m->contact_count = 1;

    if (distance == 0.0) {
        m->penetration = A->GetRadius();
        m->normal = { 1, 0 };
        m->contacts[0] = a->position;
    } else {
        m->penetration = radius - distance;
        m->normal = normal / distance; // Faster than using Normalized since we already performed sqrt
        m->contacts[0] = m->normal * A->GetRadius() + a->position;
    }
}

void CircleVsPolygon(Manifold* m, Body* a, Body* b) {
    Circle* A = reinterpret_cast<Circle*>(a->shape);
    Polygon* B = reinterpret_cast<Polygon*>(b->shape);

    m->contact_count = 0;

    // Transform circle center to Polygon model space
    auto center = a->position;
    center = B->GetRotationMatrix().Transpose() * (center - b->position);

    // Find edge with minimum penetration
    // Exact concept as using support points in Polygon vs Polygon
    auto separation = -DBL_MAX;
    auto faceNormal = 0;
    for (auto i = 0; i < B->vertices.size(); ++i) {
        auto s = B->normals[i].DotProduct(center - B->vertices[i]);

        if (s > A->GetRadius())
            return;

        if (s > separation) {
            separation = s;
            faceNormal = i;
        }
    }

    // Grab face's vertices
    auto v1 = B->vertices[faceNormal];
    auto i2 = faceNormal + 1 < B->vertices.size() ? faceNormal + 1 : 0;
    auto v2 = B->vertices[i2];

    // Check to see if center is within polygon
    if (separation < DBL_EPSILON) {
        m->contact_count = 1;
        m->normal = -(B->GetRotationMatrix() * B->normals[faceNormal]);
        m->contacts[0] = m->normal * A->GetRadius() + a->position;
        m->penetration = A->GetRadius();
        return;
    }

    // Determine which voronoi region of the edge center of circle lies within
    auto dot1 = (center - v1).DotProduct(v2 - v1);
    auto dot2 = (center - v2).DotProduct(v1 - v2);
    m->penetration = A->GetRadius() - separation;

    // Closest to v1
    if (dot1 <= 0.0) {
        if (DistanceSquared(center, v1) > A->GetRadius() * A->GetRadius())
            return;

        m->contact_count = 1;
        auto n = v1 - center;
        n = B->GetRotationMatrix() * n;
        n = n.Normalized();
        m->normal = n;
        v1 = B->GetRotationMatrix() * v1 + b->position;
        m->contacts[0] = v1;
    }

    // Closest to v2
    else if (dot2 <= 0.0) {
        if (DistanceSquared(center, v2) > A->GetRadius() * A->GetRadius())
            return;

        m->contact_count = 1;
        auto n = v2 - center;
        v2 = B->GetRotationMatrix() * v2 + b->position;
        m->contacts[0] = v2;
        n = B->GetRotationMatrix() * n;
        n = n.Normalized();
        m->normal = n;
    }

    // Closest to face
    else {
        auto n = B->normals[faceNormal];
        if ((center - v1).DotProduct(n) > A->GetRadius())
            return;

        n = B->GetRotationMatrix() * n;
        m->normal = -n;
        m->contacts[0] = m->normal * A->GetRadius() + a->position;
        m->contact_count = 1;
    }
}

void PolygonVsCircle(Manifold* m, Body* a, Body* b) {
    CircleVsPolygon(m, b, a);
    m->normal = -m->normal;
}

static double FindAxisLeastPenetration(std::uint32_t* faceIndex, Polygon* A, Polygon* B) {
    double bestDistance = -DBL_MAX;
    std::uint32_t bestIndex;

    for (std::uint32_t i = 0; i < A->vertices.size(); ++i) {
        // Retrieve a face normal from A
        auto n = A->normals[i];
        auto nw = A->GetRotationMatrix() * n;

        // Transform face normal into B's model space
        auto buT = B->GetRotationMatrix().Transpose();
        n = buT * nw;

        // Retrieve support point from B along -n
        auto s = B->GetSupport(-n);

        // Retrieve vertex on face from A, transform into
        // B's model space
        auto v = A->vertices[i];
        v = A->GetRotationMatrix() * v + A->body->position;
        v -= B->body->position;
        v = buT * v;

        // Compute penetration distance (in B's model space)
        auto d = n.DotProduct(s - v);

        // Store greatest distance
        if (d > bestDistance) {
            bestDistance = d;
            bestIndex = i;
        }
    }

    *faceIndex = bestIndex;
    return bestDistance;
}

static std::int32_t Clip(const V2_double& n, double c, V2_double* face) {
    std::uint32_t sp = 0;
    V2_double out[2] = {
      face[0],
      face[1]
    };

    // Retrieve distances from each endpoint to the line
    // d = ax + by - c
    auto d1 = n.DotProduct(face[0]) - c;
    auto d2 = n.DotProduct(face[1]) - c;

    // If negative (behind plane) clip
    if (d1 <= 0.0) out[sp++] = face[0];
    if (d2 <= 0.0) out[sp++] = face[1];

    // If the points are on different sides of the plane
    if (d1 * d2 < 0.0) // less than to ignore -0.0
    {
        // Push interesection point
        auto alpha = d1 / (d1 - d2);
        out[sp] = face[0] + alpha * (face[1] - face[0]);
        ++sp;
    }

    // Assign our new converted values
    face[0] = out[0];
    face[1] = out[1];

    assert(sp != 3);

    return sp;
}

void FindIncidentFace(V2_double* v, Polygon* RefPoly, Polygon* IncPoly, std::uint32_t referenceIndex) {
    auto referenceNormal = RefPoly->normals[referenceIndex];

    // Calculate normal in incident's frame of reference
    referenceNormal = RefPoly->GetRotationMatrix() * referenceNormal; // To world space
    referenceNormal = IncPoly->GetRotationMatrix().Transpose() * referenceNormal; // To incident's model space

    // Find most anti-normal face on incident polygon
    std::int32_t incidentFace = 0;
    auto minDot = DBL_MAX;
    for (std::uint32_t i = 0; i < IncPoly->vertices.size(); ++i) {
        auto dot = referenceNormal.DotProduct(IncPoly->normals[i]);
        if (dot < minDot) {
            minDot = dot;
            incidentFace = i;
        }
    }

    // Assign face vertices for incidentFace
    v[0] = IncPoly->GetRotationMatrix() * IncPoly->vertices[incidentFace] + IncPoly->body->position;
    incidentFace = incidentFace + 1 >= (std::int32_t)IncPoly->vertices.size() ? 0 : incidentFace + 1;
    v[1] = IncPoly->GetRotationMatrix() * IncPoly->vertices[incidentFace] + IncPoly->body->position;
}

inline bool BiasGreaterThan(double a, double b) {
    const double k_biasRelative = 0.95;
    const double k_biasAbsolute = 0.01;
    return a >= b * k_biasRelative + a * k_biasAbsolute;
}

void PolygonVsPolygon(Manifold* m, Body* a, Body* b) {
    Polygon* A = reinterpret_cast<Polygon*>(a->shape);
    Polygon* B = reinterpret_cast<Polygon*>(b->shape);
    m->contact_count = 0;
    
    // Check for a separating axis with A's face planes
    
    std::uint32_t faceA;
    auto penetrationA = FindAxisLeastPenetration(&faceA, A, B);
    if (penetrationA >= 0.0)
        return;

    // Check for a separating axis with B's face planes
    std::uint32_t faceB;
    auto penetrationB = FindAxisLeastPenetration(&faceB, B, A);
    if (penetrationB >= 0.0)
        return;

   // LOG("ACTUAL Collision occured");

    std::uint32_t referenceIndex;
    bool flip; // Always point from a to b

    Polygon* RefPoly; // Reference
    Polygon* IncPoly; // Incident

    // Determine which shape contains reference face
    if (BiasGreaterThan(penetrationA, penetrationB)) {
        RefPoly = A;
        IncPoly = B;
        referenceIndex = faceA;
        flip = false;
    }

    else {
        RefPoly = B;
        IncPoly = A;
        referenceIndex = faceB;
        flip = true;
    }

    // World space incident face
    V2_double incidentFace[2];
    FindIncidentFace(incidentFace, RefPoly, IncPoly, referenceIndex);

    //        y
    //        ^  ->n       ^
    //      +---c ------posPlane--
    //  x < | i |\
    //      +---+ c-----negPlane--
    //             \       v
    //              r
    //
    //  r : reference face
    //  i : incident poly
    //  c : clipped point
    //  n : incident normal

    // Setup reference face vertices
    V2_double v1 = RefPoly->vertices[referenceIndex];
    referenceIndex = referenceIndex + 1 == RefPoly->vertices.size() ? 0 : referenceIndex + 1;
    V2_double v2 = RefPoly->vertices[referenceIndex];

    // Transform vertices to world space
    v1 = RefPoly->GetRotationMatrix() * v1 + RefPoly->body->position;
    v2 = RefPoly->GetRotationMatrix() * v2 + RefPoly->body->position;

    // Calculate reference face side normal in world space
    V2_double sidePlaneNormal = (v2 - v1);
    sidePlaneNormal = sidePlaneNormal.Normalized();

    // Orthogonalize
    V2_double refFaceNormal = { sidePlaneNormal.y, -sidePlaneNormal.x };

    // ax + by = c
    // c is distance from origin
    auto refC = refFaceNormal.DotProduct(v1);
    auto negSide = -sidePlaneNormal.DotProduct(v1);
    auto posSide = sidePlaneNormal.DotProduct(v2);

    // Clip incident face to reference face side planes
    if (Clip(-sidePlaneNormal, negSide, incidentFace) < 2)
        return; // Due to floating point error, possible to not have required points

    if (Clip(sidePlaneNormal, posSide, incidentFace) < 2)
        return; // Due to floating point error, possible to not have required points

      // Flip
    m->normal = flip ? -refFaceNormal : refFaceNormal;

    // Keep points behind reference face
    std::uint32_t cp = 0; // clipped points behind reference face
    auto separation = refFaceNormal.DotProduct(incidentFace[0]) - refC;
    if (separation <= 0.0) {
        m->contacts[cp] = incidentFace[0];
        m->penetration = -separation;
        ++cp;
    } else
        m->penetration = 0;

    separation = refFaceNormal.DotProduct(incidentFace[1]) - refC;
    if (separation <= 0.0) {
        m->contacts[cp] = incidentFace[1];

        m->penetration += -separation;
        ++cp;

        // Average penetration
        m->penetration /= (double)cp;
    }

    m->contact_count = cp;
}