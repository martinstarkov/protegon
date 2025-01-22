#pragma once

#include <memory>
#include <type_traits>

#include "core/manager.h"
#include "scene/scene.h"
#include "utility/debug.h"

namespace ptgn {

class SceneTransition;
class CameraManager;

namespace impl {

class SceneCamera;
class Game;
class Renderer;

/*

Possible uses:

Only load a scene:
game.scene.Load<StartupScene>("startup", scene_args...);

Load and enter a scene:
game.scene.Enter<StartupScene>("startup", SceneTransition{}, scene_args...);

Enter a scene that is already loaded:
game.scene.Enter("startup", SceneTransition{});

Unload a scene. If the scene is current, the game loop is exited:
game.scene.Unload("startup");

Retrieve a scene casted to that pointer:
game.scene.Get<StartupScene>("startup").DoStuff();

*/
class SceneManager : public MapManager<std::shared_ptr<Scene>> {
public:
	SceneManager()									 = default;
	~SceneManager() override						 = default;
	SceneManager(SceneManager&&) noexcept			 = default;
	SceneManager& operator=(SceneManager&&) noexcept = default;
	SceneManager(const SceneManager&)				 = delete;
	SceneManager& operator=(const SceneManager&)	 = delete;

	// Load a scene into the scene manager.
	// Note: Loading a scene means it will be constructed but not entered.
	// A scene is entered when the user calls game.scene.Enter(key).
	// @tparam TScene The type of scene to be loaded.
	// @param scene_key A unique identifier for the loaded scene.
	// @param constructor_args Optional: Arguments passed to TScene's constructor.
	template <typename TScene, typename TKey, typename... TArgs>
	TScene& Load(const TKey& scene_key, TArgs&&... constructor_args) {
		static_assert(
			std::is_constructible_v<TScene, TArgs...>,
			"Loaded scene type must be constructible from provided constructor arguments"
		);

		static_assert(
			std::is_convertible_v<TScene*, Scene*>,
			"Loaded scene type must inherit from ptgn::Scene"
		);
		auto k{ GetInternalKey(scene_key) };
		auto scene{ std::make_shared<TScene>(std::forward<TArgs>(constructor_args)...) };
		PTGN_ASSERT(scene->actions_.empty(), "Scene must be initialized with no actions");
		return *std::static_pointer_cast<TScene>(
			MapManager<std::shared_ptr<Scene>>::Load(k, std::move(scene))
		);
	}

	// Load a scene into the scene manager and enter it.
	// @tparam TScene The type of scene to be loaded and entered.
	// @param scene_key A unique identifier for the loaded scene.
	// @param transition Optional: An optional class allowing for custom scene transitions (e.g.
	// fading or power point slides).
	// @param constructor_args Optional: Arguments passed to TScene's constructor.
	template <typename TScene, typename TKey, typename... TArgs>
	TScene& Enter(
		const TKey& scene_key, const SceneTransition& transition = {}, TArgs&&... constructor_args
	) {
		auto& scene{ Load<TScene>(scene_key, std::forward<TArgs>(constructor_args)...) };
		EnterImpl(GetInternalKey(scene_key), transition);
		return scene;
	}

	// Enter a scene. The current scene is updated every frame of the main game loop.
	// @param scene_key A unique identifier for the loaded scene.
	// @param transition Optional: An optional class allowing for custom scene transitions (e.g.
	// fading or power point slides).
	template <typename TKey>
	void Enter(const TKey& scene_key, const SceneTransition& transition = {}) {
		EnterImpl(GetInternalKey(scene_key), transition);
	}

	// Retrieve a scene from the scene manager. If the scene does not exist in the scene manager an
	// assertion is called.
	// @param scene_key The unique identifier for the retrieved scene.
	// @tparam TScene An optional type to cast the retrieved scene pointer to (i.e. return will be
	// shared_ptr<TScene>).
	// @return A shared pointer to the desired scene.
	template <typename TScene = Scene, typename TKey = Key>
	[[nodiscard]] std::shared_ptr<TScene> Get(const TKey& scene_key) {
		static_assert(
			std::is_base_of_v<Scene, TScene> || std::is_same_v<TScene, Scene>,
			"Cannot cast retrieved scene to type which does not inherit from the Scene class"
		);
		PTGN_ASSERT(
			Has(scene_key),
			"Cannot retrieve a scene which has not been loaded into the scene manager"
		);
		return std::static_pointer_cast<TScene>(MapManager<std::shared_ptr<Scene>>::Get(scene_key));
	}

	// Unload a scene from the scene manager. If the unloaded scene is current, another scene must
	// be entered during the update call, otherwise the game loop will be exited.
	// @param scene_key The unique identifier for the scene.
	template <typename TKey>
	void Unload(const TKey& scene_key) {
		UnloadImpl(GetInternalKey(scene_key));
	}

	// Unloads all scenes from the scene manager.
	void UnloadAll();

	// @return The scene which is current.
	[[nodiscard]] Scene& GetCurrent();
	[[nodiscard]] const Scene& GetCurrent() const;

	// @return True if a scene has been entered, false otherwise.
	[[nodiscard]] bool HasCurrent() const;

private:
	friend class ptgn::SceneTransition;
	friend class impl::SceneCamera;
	friend class impl::Game;
	friend class impl::Renderer;

	void EnterScene(const std::shared_ptr<Scene>& scene);

	// Updates and flushes the current scene.
	void Update();

	void UnloadImpl(const InternalKey& scene_key);

	void EnterImpl(const InternalKey& scene_key, const SceneTransition& scene_transition);

	void Reset();
	void Shutdown();

	// Calls the enter / exit / unload of the current scene after the current frame has
	// completed. This prevents unloading the scene or entering a new scene while the old one is
	// still being updated.
	void HandleSceneEvents();

	std::shared_ptr<Scene> current_scene_{ nullptr };

	// Set to true when a scene is currently transitioning. Prevents repeated triggering of scene
	// transitions.
	bool transitioning_{ false };
};

} // namespace impl

} // namespace ptgn
