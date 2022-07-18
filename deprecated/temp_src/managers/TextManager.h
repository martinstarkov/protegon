#pragma once

#include "managers/SDLManager.h"
#include "text/Text.h"

namespace ptgn {

namespace managers {

class TextManager : public SDLManager<Text> {};

} // namespace managers

} // namespace ptgn