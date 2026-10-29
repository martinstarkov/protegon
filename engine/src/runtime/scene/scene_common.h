#pragma once

#include <concepts>

namespace ptgn {

class Scene;

template <typename T>
concept SceneType = std::derived_from<T, Scene>;

} // namespace ptgn