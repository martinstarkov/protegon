#include "runtime/graphics/fonts.h"

#include "fonts/LiberationSans-Regular.h"
#include "runtime/graphics/font.h"

namespace ptgn::impl {

FontBinary GetLiberationSansRegular() {
	return FontBinary{ LiberationSans_Regular_ttf, LiberationSans_Regular_ttf_len };
}

} // namespace ptgn::impl