#pragma once

#include <memory>
#include <string_view>
#include <type_traits>

#include "common/assert.h"
#include "core/entity.h"
#include "core/manager.h"
#include "scene/scene.h"
#include "scene/scene_transition.h"

namespace ptgn {

class SceneTransition;

namespace impl {

class Game;
class Renderer;

struct SceneComponent {
	std::unique_ptr<Scene> scene;
};

class SceneManager {
public:
	SceneManager()									 = default;
	~SceneManager()									 = default;
	SceneManager(SceneManager&&) noexcept			 = default;
	SceneManager& operator=(SceneManager&&) noexcept = default;
	SceneManager(const SceneManager&)				 = delete;
	SceneManager& operator=(const SceneManager&)	 = delete;

	// Load a scene into the scene manager.
	// Note: Loading a scene means it will be constructed but not entered.
	// If the provided scene is already loaded, nothing happens.
	// @tparam TScene The type of scene to be loaded.
	// @param scene_key A unique identifier for the loaded scene.
	// @param constructor_args Optional: Arguments passed to TScene's constructor.
	template <typename TScene, typename... TArgs>
	TScene& Load(std::string_view scene_key, TArgs&&... constructor_args) {
		static_assert(
			std::is_constructible_v<TScene, TArgs...>,
			"Loaded scene type must be constructible from provided constructor arguments"
		);

		static_assert(
			std::is_convertible_v<TScene*, Scene*>,
			"Loaded scene type must inherit from ptgn::Scene"
		);
		auto key{ GetInternalKey(scene_key) };
		auto scene{ GetScene(key) };
		SceneComponent* sc{ nullptr };
		if (!scene) { // New scene.
			scene = scenes_.CreateEntity();
			sc	  = &scene.Add<SceneComponent>(
				   std::make_unique<TScene>(std::forward<TArgs>(constructor_args)...)
			   );
			sc->scene->key_ = key;
			scenes_.Refresh();
		} else { // Existing scene.
			sc = &scene.Get<SceneComponent>();
		}
		return *static_cast<TScene*>(sc->scene.get());
	}

	// Makes a scene active.
	// The scene must first be loaded into the scene manager using Load().
	// If the provided scene is already active, it is restarted.
	// Active scenes are updated every frame of the main game loop.
	// The most recently loaded scene is updated and rendered last.
	// @param scene_key The unique identifier for the scene to be made active.
	void Enter(std::string_view scene_key) {
		EnterImpl(GetInternalKey(scene_key));
	}

	// Load a scene into the scene manager and make it active.
	// The scene will be constructed immediately and initialized before the start of the next frame.
	// @tparam TScene The type of scene to be loaded and set as active.
	// @param scene_key A unique identifier for the loaded scene.
	// @param constructor_args Optional: Arguments passed to TScene's constructor.
	template <typename TScene, typename... TArgs>
	TScene& Enter(std::string_view scene_key, TArgs&&... constructor_args) {
		auto& scene{ Load<TScene>(scene_key, std::forward<TArgs>(constructor_args)...) };
		Enter(scene_key);
		return scene;
	}

	// Unload a scene from the scene manager. If active, the scene is exited first.
	// If the provided scene is not loaded, nothing happens.
	// @param scene_key The unique identifier for the scene.
	void Unload(std::string_view scene_key) {
		UnloadImpl(GetInternalKey(scene_key));
	}

	// Exits an active scene.
	// Note: This will not call the scene destructor, but instead its Exit function.
	// If the provided scene is not active, nothing happens.
	// @param scene_key The unique identifier for the scene to be removed from active scenes.
	void Exit(std::string_view scene_key) {
		ExitImpl(GetInternalKey(scene_key));
	}

	// Transitions from one active scene to another.
	// @param from_scene_key The unique identifier for the scene to be exited.
	// @param to_scene_key The unique identifier for the scene to be entered.
	// @param transition An optional class allowing for custom scene transitions. If {}, no
	// transition is used.
	void Transition(
		std::string_view from_scene_key, std::string_view to_scene_key,
		const SceneTransition& transition = {}
	) {
		TransitionImpl(GetInternalKey(from_scene_key), GetInternalKey(to_scene_key), transition);
	}

	template <typename TScene, typename... TArgs>
	TScene& Transition(
		std::string_view from_scene_key, std::string_view to_scene_key,
		const SceneTransition& transition, TArgs&&... constructor_args
	) {
		auto& scene{ Load<TScene>(to_scene_key, std::forward<TArgs>(constructor_args)...) };
		TransitionImpl(GetInternalKey(from_scene_key), GetInternalKey(to_scene_key), transition);
		return scene;
	}

	// Retrieve a scene from the scene manager. If the scene does not exist in the scene manager an
	// assertion is called.
	// @param scene_key The unique identifier for the retrieved scene.
	// @tparam TScene An optional type to cast the retrieved scene pointer to (i.e. return will be
	// shared_ptr<TScene>).
	// @return A shared pointer to the desired scene.
	template <typename TScene = Scene>
	[[nodiscard]] TScene& Get(std::string_view scene_key) {
		static_assert(
			std::is_base_of_v<Scene, TScene> || std::is_same_v<TScene, Scene>,
			"Cannot cast retrieved scene to type which does not inherit from the Scene class"
		);
		auto scene{ GetScene(GetInternalKey(scene_key)) };
		PTGN_ASSERT(scene, "Scene key does not exist in the scene manager");
		PTGN_ASSERT(scene.Has<SceneComponent>());
		return *static_cast<TScene*>(scene.Get<SceneComponent>().scene.get());
	}

	// Exits all active scenes. This does not unload the scenes from the
	// scene manager.
	// The most recently loaded scene is removed last.
	void ExitAll();

	// Unloads all scenes from the scene manager.
	// If a scene was active, it is exited first.
	void UnloadAllScenes();

private:
	friend class ptgn::SceneTransition;
	friend class Game;
	friend class Renderer;

	[[nodiscard]] static std::size_t GetInternalKey(std::string_view key);

	// Updates all the active scenes.
	void Update();

	// Clears the frame buffers of each scene.
	// void ClearSceneTargets();

	void EnterScene(std::size_t scene_key);

	void UnloadImpl(std::size_t scene_key);

	void EnterImpl(std::size_t scene_key);

	void ExitImpl(std::size_t scene_key);

	void TransitionImpl(
		std::size_t from_scene_key, std::size_t to_scene_key, const SceneTransition& transition
	);

	// Switches the places of the components of two scenes. This will invalidate all pointers or
	// references to both of the given scenes.
	void SwitchActiveScenesImpl(std::size_t scene1, std::size_t scene2);

	void Reset();
	void Shutdown();

	void HandleSceneEvents();

	[[nodiscard]] std::size_t GetActiveSceneCount() const;

	[[nodiscard]] bool HasScene(std::size_t scene_key) const;
	[[nodiscard]] bool HasActiveScene(std::size_t scene_key) const;

	[[nodiscard]] Entity GetScene(std::size_t scene_key) const;
	[[nodiscard]] Entity GetActiveScene(std::size_t scene_key) const;

	Manager scenes_;
};

} // namespace impl

} // namespace ptgn