#include "components/relatives.h"

#include <string_view>

#include "core/entity.h"
#include "utility/span.h"

namespace ptgn::impl {

Parent::Parent(const Entity& entity) : Entity{ entity } {}

void Children::Clear() {
	children_.clear();
}

void Children::Add(Entity& child, std::string_view name) {
	if (!name.empty()) {
		child.Add<ChildKey>(name);
	}
	if (VectorContains(children_, child)) {
		return;
	}
	children_.emplace_back(child);
}

void Children::Remove(const Entity& child) {
	VectorErase(children_, child);
	// TODO: Consider adding a use count to ChildKey so it can be removed once an entity is no
	// longer a child of any other entity.
}

void Children::Remove(std::string_view name) {
	ChildKey k{ name };
	for (auto it = children_.begin(); it != children_.end();) {
		if (it->Has<ChildKey>() && it->Get<ChildKey>() == k) {
			it = children_.erase(it);
		} else {
			++it;
		}
	}
}

[[nodiscard]] Entity Children::Get(std::string_view name) const {
	ChildKey k{ name };
	for (const auto& entity : children_) {
		if (entity.Has<ChildKey>() && entity.Get<ChildKey>() == k) {
			return entity;
		}
	}
	return {};
}

[[nodiscard]] bool Children::IsEmpty() const {
	return children_.empty();
}

[[nodiscard]] bool Children::Has(const Entity& child) const {
	return VectorContains(children_, child);
}

[[nodiscard]] bool Children::Has(std::string_view name) const {
	ChildKey k{ name };
	for (const auto& entity : children_) {
		if (entity.Has<ChildKey>() && entity.Get<ChildKey>() == k) {
			return true;
		}
	}
	return false;
}

} // namespace ptgn::impl