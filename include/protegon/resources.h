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

namespace font {

template <typename ...TArgs, type_traits::constructible<Font, TArgs...> = true>
std::shared_ptr<Font> Load(std::size_t key, TArgs&&... constructor_args) {
	return GetManagers().font.Load(key, std::forward<TArgs>(constructor_args)...);
}
void Unload(std::size_t key);
bool Has(std::size_t key);
std::shared_ptr<Font> Get(std::size_t key);
void Clear();

} // namespace font

namespace music {

template <typename ...TArgs, type_traits::constructible<Music, TArgs...> = true>
std::shared_ptr<Music> Load(std::size_t key, TArgs&&... constructor_args) {
	return GetManagers().music.Load(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(std::size_t key);
bool Has(std::size_t key);
std::shared_ptr<Music> Get(std::size_t key);
void Clear();
void Pause();
void Resume();
void Stop();
void FadeOut(milliseconds time);
bool IsPlaying();
bool IsPaused();
bool IsFading();

} // namespace music

namespace sound {

template <typename ...TArgs, type_traits::constructible<Sound, TArgs...> = true>
std::shared_ptr<Sound> Load(std::size_t key, TArgs&&... constructor_args) {
	return GetManagers().sound.Load(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(std::size_t key);
bool Has(std::size_t key);
std::shared_ptr<Sound> Get(std::size_t key);
void Clear();

} // namespace sound

namespace text {

template <typename ...TArgs, type_traits::constructible<Text, TArgs...> = true>
std::shared_ptr<Text> Load(std::size_t key, TArgs&&... constructor_args) {
	return GetManagers().text.Load(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(std::size_t key);
bool Has(std::size_t key);
std::shared_ptr<Text> Get(std::size_t key);
void Clear();

} // namespace text

namespace texture {

template <typename ...TArgs, type_traits::constructible<Texture, TArgs...> = true>
std::shared_ptr<Texture> Load(std::size_t key, TArgs&&... constructor_args) {
	return GetManagers().texture.Load(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(std::size_t key);
bool Has(std::size_t key);
std::shared_ptr<Texture> Get(std::size_t key);
void Clear();

} // namespace texture


namespace scene {

bool Has(std::size_t scene_key);

template <typename T, typename ...TArgs,
	type_traits::constructible<T, TArgs...> = true,
	type_traits::convertible<T*, Scene*> = true>
std::shared_ptr<T> Load(std::size_t key, TArgs&&... constructor_args) {
	return GetManagers().scene.LoadPolymorphic<T>(key, std::forward<TArgs>(constructor_args)...);
}

void Unload(std::size_t scene_key);

void SetActive(std::size_t scene_key);

void AddActive(std::size_t scene_key);

void RemoveActive(std::size_t scene_key);

std::vector<std::shared_ptr<Scene>> GetActive();

void Update(float dt);

} // namespace scene

} // namespace ptgn