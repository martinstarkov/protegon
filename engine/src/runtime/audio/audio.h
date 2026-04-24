#pragma once

#include <ecs/ecs.h>

#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "core/util/hash.h"

namespace ptgn {

class AssetManager;
class AudioSystem;

namespace impl {

struct AudioObject {
	path path;
};

}; // namespace impl

class Audio : public EntityHandle {
public:
	using EntityHandle::EntityHandle;

private:
	friend class AudioSystem;
	friend class AssetManager;
};

} // namespace ptgn

template <>
struct std::hash<ptgn::Audio> {
	std::size_t operator()(const ptgn::Audio& audio) const {
		return ptgn::Hash(audio.GetEntity());
	}
};