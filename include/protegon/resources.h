#pragma once

#include "manager.h"
#include "font.h"
#include "sound.h"
#include "text.h"
#include "texture.h"
#include "time.h"
#include "scene.h"
#include "shader.h"

namespace ptgn {

struct ResourceManagers {
	HandleManager<Font>    font;
	HandleManager<Music>   music;
	HandleManager<Sound>   sound;
	HandleManager<Text>    text;
	HandleManager<Texture> texture;
	HandleManager<Shader>  shader;
	SceneManager scene;
};

ResourceManagers& GetManagers();

using FontKey    = std::size_t;
using MusicKey   = std::size_t;
using SoundKey   = std::size_t;
using TextKey    = std::size_t;
using TextureKey = std::size_t;
using ShaderKey  = std::size_t;
using SceneKey   = std::size_t;

namespace font {

template <typename ...TArgs, type_traits::constructible<Font, TArgs...> = true>
Font Load(FontKey key, TArgs&&... constructor_args) {
	return GetManagers().font.Load(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(FontKey key);
[[nodiscard]] bool Has(FontKey key);
[[nodiscard]] Font Get(FontKey key);
void Clear();

} // namespace font

namespace music {

template <typename ...TArgs, type_traits::constructible<Music, TArgs...> = true>
Music Load(MusicKey key, TArgs&&... constructor_args) {
	return GetManagers().music.Load(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(MusicKey key);
[[nodiscard]] bool Has(MusicKey key);
[[nodiscard]] Music Get(MusicKey key);
void Clear();
void Pause();
void Resume();
// Returns the current music track volume from 0 to 128 (MIX_MAX_VOLUME).
[[nodiscard]] int GetVolume();
// Volume can be set from 0 to 128 (MIX_MAX_VOLUME).
void SetVolume(int new_volume);
// Default: -1 for max volume 128 (MIX_MAX_VOLUME).
void Toggle(int optional_new_volume = -1);
// Sets volume to 0.
void Mute();
// Default: -1 for max volume 128 (MIX_MAX_VOLUME).
void Unmute(int optional_new_volume = -1);
void Stop();
void FadeOut(milliseconds time);
[[nodiscard]] bool IsPlaying();
[[nodiscard]] bool IsPaused();
[[nodiscard]] bool IsFading();

} // namespace music

namespace sound {

template <typename ...TArgs, type_traits::constructible<Sound, TArgs...> = true>
Sound Load(SoundKey key, TArgs&&... constructor_args) {
	return GetManagers().sound.Load(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(SoundKey key);
[[nodiscard]] bool Has(SoundKey key);
[[nodiscard]] Sound Get(SoundKey key);
void Clear();

void HaltChannel(int channel);
void ResumeChannel(int channel);

} // namespace sound

namespace text {

template <typename ...TArgs, type_traits::constructible<Text, TArgs...> = true>
Text Load(TextKey key, TArgs&&... constructor_args) {
	return GetManagers().text.Load(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(TextKey key);
[[nodiscard]] bool Has(TextKey key);
[[nodiscard]] Text Get(TextKey key);
void Clear();

} // namespace text

namespace texture {

template <typename ...TArgs, type_traits::constructible<Texture, TArgs...> = true>
Texture Load(TextureKey key, TArgs&&... constructor_args) {
	return GetManagers().texture.Load(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(TextureKey key);
[[nodiscard]] bool Has(TextureKey key);
[[nodiscard]] Texture Get(TextureKey key);
void Clear();

} // namespace texture


namespace shader {

template <typename ...TArgs, type_traits::constructible<Shader, TArgs...> = true>
Shader Load(ShaderKey key, TArgs&&... constructor_args) {
	return GetManagers().shader.Load(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(ShaderKey key);
[[nodiscard]] bool Has(ShaderKey key);
[[nodiscard]] Shader Get(ShaderKey key);
void Clear();

} // namespace shader

namespace scene {

namespace impl {

template <typename T, typename ...TArgs,
	type_traits::constructible<T, TArgs...> = true,
	type_traits::convertible<T*, Scene*> = true>
void SetStartScene(TArgs&&... constructor_args) {
	PTGN_ASSERT(!scene::Has(start_scene_key), "Cannot load more than one start scene");
	// This may be unintuitive order but since the starting scene may set other active scenes,
	// it is important to set it first so it is the "earliest" active scene in the list.
	scene::SetActive(start_scene_key);
	GetManagers().scene.Load(start_scene_key, std::make_shared<T>(std::forward<TArgs>(constructor_args)...));
}

} // namespace impl

template <typename T, typename ...TArgs,
	type_traits::constructible<T, TArgs...> = true,
	type_traits::convertible<T*, Scene*> = true>
std::shared_ptr<T> Load(SceneKey key, TArgs&&... constructor_args) {
	PTGN_CHECK(key != impl::start_scene_key, "Cannot load scene with key == 0, it is reserved for the starting scene");
	return std::static_pointer_cast<T>(GetManagers().scene.Load(key, std::make_shared<T>(std::forward<TArgs>(constructor_args)...)));
}

[[nodiscard]] bool Has(SceneKey key);
void Unload(SceneKey key);
[[nodiscard]] std::shared_ptr<Scene> Get(SceneKey key);
template <typename TScene, type_traits::is_base_of<TScene, Scene> = true>
[[nodiscard]] std::shared_ptr<TScene> Get(SceneKey key) {
	return std::static_pointer_cast<TScene>(Get(key));
}

[[nodiscard]] std::vector<std::shared_ptr<Scene>> GetActive();
void SetActive(SceneKey key);
void AddActive(SceneKey key);
void RemoveActive(SceneKey key);

void Update(float dt);

} // namespace scene

} // namespace ptgn