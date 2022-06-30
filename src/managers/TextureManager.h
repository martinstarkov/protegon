#pragma once

#include "managers/Manager.h"
#include "texture/Texture.h"

namespace ptgn {

namespace internal {

namespace managers {

class TextureManager : public Manager<Texture> {
public:
    TextureManager();
};

} // namespace managers

managers::TextureManager& GetTextureManager();

} // namespace internal

} // namespace ptgn