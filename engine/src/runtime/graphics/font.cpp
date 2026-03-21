#include "runtime/graphics/font.h"

#include <ostream>

#include "core/util/entity_handle.h"
#include "ecs/ecs.h"

namespace ptgn {

std::ostream& operator<<(std::ostream& o, const Font& f) {
	o << "{";
	o << "font id: " << f.GetEntity().GetId();
	o << "}";
	return o;
}

} // namespace ptgn
