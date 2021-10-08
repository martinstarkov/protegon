#include "UIManager.h"

namespace ptgn {

namespace services {

interfaces::UIManager& GetUIManager() {
	// TODO: Change to return specific implementation.
	static interfaces::UIManager ui_manager;
	return ui_manager;
}

} // namespace services

} // namespace ptgn