#include "components/relatives.h"

#include <string_view>
#include <utility>
#include <vector>

#include "core/entity.h"
#include "debug/log.h"
#include "utility/span.h"

namespace ptgn {

Parent::Parent(const Entity& entity) : Entity{ entity } {}

void Children::Clear() {
	children_.clear();
}

void Children::Add(Entity& child, std::string_view name) {
	if (!name.empty()) {
		impl::EntityAccess::Add<impl::ChildKey>(child, name);
	}
	if (VectorContains(children_, child)) {
		return;
	}
	children_.emplace_back(child);
}

void Children::Remove(const Entity& child) {
	VectorErase(children_, child);
	// TODO: Consider adding a use count to impl::ChildKey so it can be removed once an entity is no
	// longer a child of any other entity.
}

void Children::Remove(std::string_view name) {
	impl::ChildKey k{ name };
	for (auto it = children_.begin(); it != children_.end();) {
		if (it->Has<impl::ChildKey>() && it->Get<impl::ChildKey>() == k) {
			it = children_.erase(it);
		} else {
			++it;
		}
	}
}

[[nodiscard]] const Entity& Children::Get(std::string_view name) const {
	impl::ChildKey k{ name };
	for (const auto& entity : children_) {
		if (entity.Has<impl::ChildKey>() && entity.Get<impl::ChildKey>() == k) {
			return entity;
		}
	}
	PTGN_ERROR("Failed to find child with the given name");
}

Entity& Children::Get(std::string_view name) {
	return const_cast<Entity&>(std::as_const(*this).Get(name));
}

[[nodiscard]] bool Children::IsEmpty() const {
	return children_.empty();
}

[[nodiscard]] bool Children::Has(const Entity& child) const {
	return VectorContains(children_, child);
}

[[nodiscard]] bool Children::Has(std::string_view name) const {
	impl::ChildKey k{ name };
	for (const auto& entity : children_) {
		if (entity.Has<impl::ChildKey>() && entity.Get<impl::ChildKey>() == k) {
			return true;
		}
	}
	return false;
}

} // namespace ptgn