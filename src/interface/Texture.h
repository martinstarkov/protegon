#pragma once

namespace ptgn {

namespace texture {

// Loads a texture with the given key and path into the texture manager.
void Load(const char* texture_key, const char* texture_path, internal::managers::id window);

// Unloads a texture with the given key from the texture manager.
void Unload(const char* texture_key);

// Returns true if a texture with the given key has been loaded into the texture manager, false otherwise.
bool Exists(const char* texture_key);

} // namespace texture

} // namespace ptgn