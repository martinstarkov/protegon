#pragma once

#include "managers/Manager.h"
#include "text/Font.h"

namespace ptgn {

namespace internal {

namespace managers {

class FontManager : public Manager<Font> {
public:
    FontManager();
};

} // namespace managers

managers::FontManager& GetFontManager();

} // namespace internal

} // namespace ptgn