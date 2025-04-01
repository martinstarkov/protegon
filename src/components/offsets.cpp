#include "components/offsets.h"

#include "core/transform.h"

namespace ptgn::impl {

Transform Offsets::GetTotal() const {
	return shake.RelativeTo(bounce);
}

} // namespace ptgn::impl