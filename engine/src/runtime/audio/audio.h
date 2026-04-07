#pragma once

#include "core/util/entity_handle.h"
#include "ecs/ecs.h"

struct MIX_Audio;

namespace ptgn {

class AssetManager;
class AudioSystem;

namespace impl {

struct MIX_AudioDeleter {
	void operator()(MIX_Audio* audio) const;
};

} // namespace impl

class Audio : public EntityHandle {
public:
	using EntityHandle::EntityHandle;

private:
	friend class AudioSystem;
	friend class AssetManager;
};

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::Audio> {
	std::size_t operator()(const ptgn::Audio& audio) const {
		return std::hash<ecs::Entity>()(audio.GetEntity());
	}
};

} // namespace std