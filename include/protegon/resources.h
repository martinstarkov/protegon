#pragma once

#include <memory> // std::shared_ptr

#include "manager.h"
#include "font.h"
#include "sound.h"
#include "text.h"
#include "texture.h"
#include "time.h"
#include "scene.h"

namespace ptgn {

struct ResourceManagers {
	ResourceManager<Font> font;
	ResourceManager<Music> music;
	ResourceManager<Sound> sound;
	ResourceManager<Text> text;
	ResourceManager<Texture> texture;
	SceneManager scene;
};

ResourceManagers& GetManagers();

using FontKey = std::size_t;
using MusicKey = std::size_t;
using SoundKey = std::size_t;
using TextKey = std::size_t;
using TextureKey = std::size_t;
using SceneKey = std::size_t;

namespace font {

template <typename ...TArgs, type_traits::constructible<Font, TArgs...> = true>
std::shared_ptr<Font> Load(FontKey key, TArgs&&... constructor_args) {
	return GetManagers().font.Load(key, std::forward<TArgs>(constructor_args)...);
}
void Unload(FontKey key);
bool Has(FontKey key);
std::shared_ptr<Font> Get(FontKey key);
void Clear();

} // namespace font

namespace music {

template <typename ...TArgs, type_traits::constructible<Music, TArgs...> = true>
std::shared_ptr<Music> Load(MusicKey key, TArgs&&... constructor_args) {
	return GetManagers().music.Load(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(MusicKey key);
bool Has(MusicKey key);
std::shared_ptr<Music> Get(MusicKey key);
void Clear();
void Pause();
void Resume();
// Returns the current music track volume from 0 to 128 (MIX_MAX_VOLUME).
int GetVolume();
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
bool IsPlaying();
bool IsPaused();
bool IsFading();

} // namespace music

namespace sound {

template <typename ...TArgs, type_traits::constructible<Sound, TArgs...> = true>
std::shared_ptr<Sound> Load(SoundKey key, TArgs&&... constructor_args) {
	return GetManagers().sound.Load(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(SoundKey key);
bool Has(SoundKey key);
std::shared_ptr<Sound> Get(SoundKey key);
void Clear();

} // namespace sound

namespace text {

template <typename ...TArgs, type_traits::constructible<Text, TArgs...> = true>
std::shared_ptr<Text> Load(TextKey key, TArgs&&... constructor_args) {
	return GetManagers().text.Load(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(TextKey key);
bool Has(TextKey key);
std::shared_ptr<Text> Get(TextKey key);
void Clear();

} // namespace text

namespace texture {

template <typename ...TArgs, type_traits::constructible<Texture, TArgs...> = true>
std::shared_ptr<Texture> Load(TextureKey key, TArgs&&... constructor_args) {
	return GetManagers().texture.Load(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(TextureKey key);
bool Has(TextureKey key);
std::shared_ptr<Texture> Get(TextureKey key);
void Clear();

} // namespace texture


namespace scene {

namespace impl {

inline constexpr SceneKey start_scene_key{ 0 };

template <typename T, type_traits::is_base_of<T, Scene> = true>
std::shared_ptr<T> LoadStartScene(T&& start_scene) {
	assert(!Has(start_scene_key) && "Only one start scene can be loaded");
	return GetManagers().scene.LoadPolymorphic<T>(start_scene_key, start_scene);
}

void SetStartSceneActive();

} // namespace impl

template <typename T, typename ...TArgs,
	type_traits::constructible<T, TArgs...> = true,
	type_traits::convertible<T*, Scene*> = true>
std::shared_ptr<T> Load(SceneKey key, TArgs&&... constructor_args) {
	assert(key != impl::start_scene_key && "Cannot load scene with key == 0, that is reserved for the starting scene");
	return GetManagers().scene.LoadPolymorphic<T>(key, std::forward<TArgs>(constructor_args)...);
}

bool Has(SceneKey key);

void Unload(SceneKey key);

void SetActive(SceneKey key);

void AddActive(SceneKey key);

void RemoveActive(SceneKey key);

std::shared_ptr<Scene> Get(SceneKey key);

template <typename TScene, type_traits::is_base_of<TScene, Scene> = true>
std::shared_ptr<TScene> Get(SceneKey key) {
	return std::static_pointer_cast<TScene>(Get(key));
}

std::vector<std::shared_ptr<Scene>> GetActive();

void Update(float dt);

} // namespace scene

} // namespace ptgn