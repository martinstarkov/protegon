#pragma once

#include "managers/Manager.h"
#include "renderer/Renderer.h"

namespace ptgn {

namespace internal {

namespace managers {

class RendererManager : public Manager<Renderer> {
public:
	RendererManager();
};

} // namespace managers

managers::RendererManager& GetRendererManager();

} // namespace internal

} // namespace ptgn