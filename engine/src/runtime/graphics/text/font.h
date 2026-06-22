#pragma once

#include "core/util/entity_handle.h"
#include "core/util/hash.h"

namespace ptgn {

class Font : public EntityHandle {
public:
	using EntityHandle::EntityHandle;
};

} // namespace ptgn

template <>
struct std::hash<ptgn::Font> {
	std::size_t operator()(const ptgn::Font& font) const {
		return ptgn::Hash(font.GetEntity());
	}
};