#pragma once

#include "manager/ResourceManager.h"
#include "text/Font.h"

namespace ptgn {

class FontManager : public manager::ResourceManager<Font> {};

} // namespace ptgn