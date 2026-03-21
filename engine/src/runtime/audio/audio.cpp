#include "runtime/audio/audio.h"

#include <ostream>

#include "core/util/entity_handle.h"
#include "ecs/ecs.h"

namespace ptgn {

std::ostream& operator<<(std::ostream& o, const Audio& a) {
	o << "{";
	o << "audio id: " << a.GetEntity().GetId();
	o << "}";
	return o;
}

} // namespace ptgn