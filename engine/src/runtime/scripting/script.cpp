#include "runtime/scripting/script.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "core/event/event.h"

namespace ptgn {

void Script::SetHash(std::size_t hash) {
	hash_ = hash;
}

std::size_t Script::GetHash() const {
	return hash_;
}

namespace impl {

void Scripts::Update() const {
	// Scripts wont be modified during dispatch because pending script changes are applied by
	// ApplyPending.
	for (const auto& s : scripts_) {
		s->OnUpdate();
	}
}

void Scripts::ApplyPending() {
	if (!pending_remove_.empty()) {
		std::erase_if(scripts_, [this](const auto& s) {
			return std::ranges::contains(pending_remove_, s->hash_);
		});
	}

	// If a script was added and removed in the same frame, we never add it.
	if (!pending_remove_.empty()) {
		std::erase_if(pending_add_, [this](const auto& s) {
			return std::ranges::contains(pending_remove_, s->hash_);
		});
	}

	pending_remove_.clear();

	for (auto& s : pending_add_) {
		s->OnCreate();
		scripts_.push_back(std::move(s));
	}

	pending_add_.clear();
}

void Scripts::OnEvent(Event event) const {
	// Scripts wont be modified during dispatch because pending script changes are applied by
	// ApplyPending.
	for (const auto& s : scripts_) {
		s->OnEvent(event);
		if (event.IsHandled()) {
			break; // bubbling within this entity's scripts
		}
	}
}

void from_json(const json& j, Scripts& scripts) {
	// TODO: Implement.
}

void to_json(json& j, const Scripts& scripts) {
	// TODO: Implement.
}

} // namespace impl

} // namespace ptgn