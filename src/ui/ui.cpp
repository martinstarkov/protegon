#include "ui/ui.h"

#include <memory>

#include "core/manager.h"
#include "protegon/button.h"

namespace ptgn::impl {

void ButtonManager::DrawFilledImpl(const InternalKey& button_key) const {
	MapManager<std::shared_ptr<Button>>::Get(button_key)->DrawFilled();
}

void ButtonManager::DrawHollowImpl(const InternalKey& button_key, float line_width) const {
	MapManager<std::shared_ptr<Button>>::Get(button_key)->DrawHollow(line_width);
}

void ButtonManager::DrawAllFilled() const {
	for (const auto& [key, button] : GetMap()) {
		button->DrawFilled();
	}
}

void ButtonManager::DrawAllHollow(float line_width) const {
	for (const auto& [key, button] : GetMap()) {
		button->DrawHollow(line_width);
	}
}

} // namespace ptgn::impl