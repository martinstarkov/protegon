#pragma once

#include "math/Vector2.h"
#include "interface/Texture.h"

namespace ptgn {

namespace component {

struct Texture {
    Texture() = delete;
    Texture(const char* key, const char* path, const V2_double& size) : key{ key }, size{ size } {
        texture::Load(key, path);
    }
    ~Texture() {
        texture::Unload(key);
    }
    const char* key;
    V2_double size;
};

} // namespace component

} // namespace ptgn