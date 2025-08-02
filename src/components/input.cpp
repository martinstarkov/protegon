#include "components/input.h"

#include <vector>

#include "core/entity.h"

namespace ptgn {

void Interactive::Clear() {
	for (Entity shape : shapes) {
		shape.Destroy();
	}
	shapes.clear();
}

} // namespace ptgn