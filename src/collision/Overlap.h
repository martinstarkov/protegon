#pragma once

#include <type_traits> // std::enable_if_t, ...

#include "math/Vector2.h"
#include "overlap/OverlapAABBAABB.h"
#include "overlap/OverlapCapsuleAABB.h"
#include "overlap/OverlapCircleAABB.h"
#include "overlap/OverlapCircleCapsule.h"
#include "overlap/OverlapCircleCircle.h"
#include "overlap/OverlapLineAABB.h"
#include "overlap/OverlapLineCapsule.h"
#include "overlap/OverlapLineCircle.h"
#include "overlap/OverlapLineLine.h"
#include "overlap/OverlapPointAABB.h"
#include "overlap/OverlapPointCapsule.h"
#include "overlap/OverlapPointCircle.h"
#include "overlap/OverlapPointPoint.h"
#include "overlap/OverlapPointLine.h"

namespace ptgn {

namespace collision {

namespace overlap {

} // namespace overlap

} // namespace collision

} // namespace ptgn