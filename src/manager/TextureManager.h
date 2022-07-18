#pragma once

#include "manager/ResourceManager.h"
#include "renderer/Texture.h"

namespace ptgn {

class TextureManager : public manager::ResourceManager<Texture> {};

} // namespace ptgn