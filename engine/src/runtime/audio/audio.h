#pragma once

#include "core/util/entity_handle.h"

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

} // namespace ptgn