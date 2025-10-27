#pragma once

namespace ptgn {

namespace impl {

class DebugSystem;
// TODO: Remove.
class ShaderManager;

} // namespace impl

class Application;
class Renderer;
class Window;
class SceneManager;
class InputHandler;
class ShaderManager;

struct EngineContext {
	Window* window{ nullptr };
	InputHandler* input{ nullptr };
	SceneManager* scene{ nullptr };
	Renderer* render{ nullptr };
	impl::DebugSystem* debug{ nullptr };
	// TODO: Remove.
	impl::ShaderManager* shader{ nullptr };

	static EngineContext Get(Application& app);
};

} // namespace ptgn