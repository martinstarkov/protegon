#include "components/offsets.h"

#include "components/transform.h"

namespace ptgn::impl {

Transform Offsets::GetTotal() const {
	return shake.RelativeTo(bounce);
}

} // namespace ptgn::impl