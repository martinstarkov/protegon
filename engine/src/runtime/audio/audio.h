#pragma once

#include <ostream>

#include "core/util/entity_handle.h"
#include "ecs/ecs.h"

namespace ptgn {

class AssetManager;
class AudioSystem;

class Audio : public EntityHandle {
public:
	using EntityHandle::EntityHandle;

private:
	friend class AudioSystem;
	friend class AssetManager;
};

std::ostream& operator<<(std::ostream& o, const Audio& a);

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::Audio> {
	std::size_t operator()(const ptgn::Audio& audio) const {
		return std::hash<ecs::Entity>()(audio.GetEntity());
	}
};

} // namespace std