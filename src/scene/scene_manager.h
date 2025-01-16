#pragma once

#include <memory>
#include <type_traits>
#include <vector>

#include "core/manager.h"
#include "scene/scene.h"
#include "utility/debug.h"
#include "utility/type_traits.h"

namespace ptgn {

class SceneTransition;
struct LayerInfo;
class CameraManager;

namespace impl {

class SceneCamera;
class Game;
class Renderer;

class SceneManager : public MapManager<std::shared_ptr<Scene>> {
public:
	SceneManager()									 = default;
	~SceneManager() override						 = default;
	SceneManager(SceneManager&&) noexcept			 = default;
	SceneManager& operator=(SceneManager&&) noexcept = default;
	SceneManager(const SceneManager&)				 = delete;
	SceneManager& operator=(const SceneManager&)	 = delete;

	// Load a scene into the scene manager.
	// Note: Loading a scene means it will be constructed but not initialized.
	// A scene is initialized when it is made active using the scene manager AddActive() or
	// TransitionActive() functions.
	// @tparam TScene The type of scene to be loaded.
	// @param scene_key A unique identifier for the loaded scene.
	// @param constructor_args Optional: Arguments passed to TScene's constructor.
	template <typename TScene, typename TKey, typename... TArgs>
	std::shared_ptr<TScene> Load(const TKey& scene_key, TArgs&&... constructor_args) {
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
		return std::static_pointer_cast<TScene>(
			MapManager<std::shared_ptr<Scene>>::Load(k, std::move(scene))
		);
	}

	// Load a scene into the scene manager and add it as an active scene.
	// The scene will be constructed immediately and initialized before the start of the next frame.
	// @tparam TScene The type of scene to be loaded and set as active.
	// @param scene_key A unique identifier for the loaded scene.
	// @param constructor_args Optional: Arguments passed to TScene's constructor.
	template <typename TScene, typename TKey, typename... TArgs>
	std::shared_ptr<TScene> LoadActive(const TKey& scene_key, TArgs&&... constructor_args) {
		auto scene{ Load<TScene>(scene_key, std::forward<TArgs>(constructor_args)...) };
		AddActive(scene_key);
		return scene;
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
		return std::static_pointer_cast<TScene>(MapManager<std::shared_ptr<Scene>>::Get(scene_key));
	}

	// Unload a scene from the scene manager. This removes it from active scenes and calls its
	// destructor.
	// @param scene_key The unique identifier for the scene.
	template <typename TKey>
	void Unload(const TKey& scene_key) {
		UnloadImpl(GetInternalKey(scene_key));
	}

	// Adds a scene to the back of the active scenes vector.
	// The scene must first be loaded into the scene manager using Load().
	// Active scenes are updated every frame of the main game loop. T
	// The most recently added (top) active scene is rendered last.
	// @param scene_key The unique identifier for the scene to be made active.
	template <typename TKey>
	void AddActive(const TKey& scene_key) {
		AddActiveImpl(GetInternalKey(scene_key), active_scenes_.empty() && MapManager::Size() == 1);
	}

	// Removes an active scene from the scene manager.
	// Note: This will not call the scene destructor, but instead its virtual Shutdown function.
	// If the provided scene is not active, nothing happens.
	// @param scene_key The unique identifier for the scene to be removed from active scenes.
	template <typename TKey>
	void RemoveActive(const TKey& scene_key) {
		RemoveActiveImpl(GetInternalKey(scene_key));
	}

	// Transitions from one active scene to another.
	// @param from_scene_key The unique identifier for the scene to be removed from active scenes.
	// @param to_scene_key The unique identifier for the scene to be made active.
	// @param transition Optional: An optional class allowing for custom scene transitions (think
	// fading or power point slides).
	template <typename TKey1, typename TKey2>
	void TransitionActive(
		const TKey1& from_scene_key, const TKey2& to_scene_key,
		const SceneTransition& transition = {}
	) {
		TransitionActiveImpl(
			GetInternalKey(from_scene_key), GetInternalKey(to_scene_key), transition
		);
	}

	// Removes all scenes from the active scenes vector. This does not unload the scenes from the
	// scene manager.
	// The most recently added active scene is removed last.
	void ClearActive();

	// Unloads all scenes from the scene manager.
	// If a scene is active, its virtual Shutdown function will be called before its destructor.
	void UnloadAllScenes();

	// @return A vector of currently active scenes.
	[[nodiscard]] std::vector<std::shared_ptr<Scene>> GetActiveScenes();

	// @return The scene which was most recently added as an active scene.
	[[nodiscard]] Scene& GetTopActive();

	// @return The scene which is currently in the process of being updated, initialized, or
	// shutdown (i.e. inside of the scene's Init(), Update(), Shutdown() functions).
	// Do not call this function when a scene is not currently active, such as before loading an
	// active scene.
	[[nodiscard]] Scene& GetCurrent();
	[[nodiscard]] const Scene& GetCurrent() const;

	// @return Camera manager for the the scene which is currently in the process of being updated,
	// initialized, or shutdown (i.e. inside of the scene's Init(), Update(), Shutdown() functions)
	[[nodiscard]] CameraManager& GetCurrentCamera();
	[[nodiscard]] const CameraManager& GetCurrentCamera() const;

	// @return Whether or not the scene manager is currently updating a scene.
	[[nodiscard]] bool HasCurrent() const;

private:
	friend class ptgn::SceneTransition;
	friend class impl::SceneCamera;
	friend class impl::Game;
	friend class impl::Renderer;
	friend struct LayerInfo;

	// Updates all the active scenes.
	void Update();

	void InitScene(const InternalKey& scene_key);

	void UnloadImpl(const InternalKey& scene_key);

	void AddActiveImpl(const InternalKey& scene_key, bool first_scene);
	void RemoveActiveImpl(const InternalKey& scene_key);
	void TransitionActiveImpl(
		const InternalKey& from_scene_key, const InternalKey& to_scene_key,
		const SceneTransition& transition
	);

	// Switches the places of the two scenes in the scene array vector.
	void SwitchActiveScenesImpl(const InternalKey& scene1, const InternalKey& scene2);

	void Reset();
	void Shutdown();

	void UpdateFlagged();

	[[nodiscard]] bool HasActiveSceneImpl(const InternalKey& scene_key) const;

	std::shared_ptr<Scene> current_scene_{ nullptr };

	std::vector<InternalKey> active_scenes_;
};

} // namespace impl

} // namespace ptgn
