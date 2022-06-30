#pragma once

#include "managers/Manager.h"
#include "text/Text.h"

namespace ptgn {

namespace internal {

namespace managers {

class TextManager : public Manager<Text> {
public:
	TextManager();
};

} // namespace managers

managers::TextManager& GetTextManager();

} // namespace internal

} // namespace ptgn