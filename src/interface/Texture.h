#pragma once

namespace ptgn {

namespace texture {

// Loads a texture with the given key and path into the texture manager.
void Load(const char* texture_key, const char* texture_path);

// Unloads a texture with the given key from the texture manager.
void Unload(const char* texture_key);

} // namespace texture

} // namespace ptgn