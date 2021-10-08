#include "TextManager.h"

namespace ptgn {

namespace services {

interfaces::TextManager& GetTextManager() {
	// TODO: Change to return specific implementation.
	static interfaces::TextManager text_manager;
	return text_manager;
}

} // namespace services

} // namespace ptgn