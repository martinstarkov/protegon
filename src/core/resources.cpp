#include "protegon/resources.h"

#include "SDL_mixer.h"

#include "core/game.h"

namespace ptgn {

ResourceManagers& GetManagers() {
	return global::GetGame().managers;
}

namespace font {

void Unload(FontKey key) {
	GetManagers().font.Unload(key);
}

bool Has(FontKey key) {
	return GetManagers().font.Has(key);
}

Font Get(FontKey key) {
	return GetManagers().font.Get(key);
}

void Clear() {
	GetManagers().font.Clear();
}

} // namespace font

namespace music {

void Unload(MusicKey key) {
	GetManagers().music.Unload(key);
}

bool Has(MusicKey key) {
	return GetManagers().music.Has(key);
}

Music Get(MusicKey key) {
	return GetManagers().music.Get(key);
}

void Clear() {
	GetManagers().music.Clear();
}

void Stop() {
	Mix_HaltMusic();
}

void FadeOut(milliseconds time) {
	auto time_int =
		std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(time
		);
	Mix_FadeOutMusic(time_int.count());
}

void Pause() {
	Mix_PauseMusic();
}

void Resume() {
	Mix_ResumeMusic();
}

void Toggle(int optional_new_volume) {
	if (GetVolume() != 0) {
		Mute();
	} else {
		Unmute(optional_new_volume);
	}
}

int GetVolume() {
	return Mix_VolumeMusic(-1);
}

void SetVolume(int new_volume) {
	Mix_VolumeMusic(new_volume);
}

void Mute() {
	SetVolume(0);
}

void Unmute(int optional_new_volume) {
	if (optional_new_volume == -1) {
		SetVolume(MIX_MAX_VOLUME);
		return;
	}
	PTGN_CHECK(optional_new_volume >= 0, "Cannot unmute to volume below 0");
	PTGN_CHECK(
		optional_new_volume <= MIX_MAX_VOLUME,
		"Cannot unmute to volume above max volume (128)"
	);
	SetVolume(optional_new_volume);
}

bool IsPlaying() {
	return Mix_PlayingMusic();
}

bool IsPaused() {
	return Mix_PausedMusic();
}

bool IsFading() {
	switch (Mix_FadingMusic()) {
		case MIX_NO_FADING:	 return false;
		case MIX_FADING_OUT: return true;
		case MIX_FADING_IN:	 return true;
		default:			 return false;
	}
}

} // namespace music

namespace sound {

void Unload(SoundKey key) {
	GetManagers().sound.Unload(key);
}

bool Has(SoundKey key) {
	return GetManagers().sound.Has(key);
}

Sound Get(SoundKey key) {
	return GetManagers().sound.Get(key);
}

void Clear() {
	GetManagers().sound.Clear();
}

void HaltChannel(int channel) {
	Mix_HaltChannel(channel);
}

void ResumeChannel(int channel) {
	Mix_Resume(channel);
}

} // namespace sound

namespace text {

void Unload(TextKey key) {
	GetManagers().text.Unload(key);
}

bool Has(TextKey key) {
	return GetManagers().text.Has(key);
}

Text Get(TextKey key) {
	return GetManagers().text.Get(key);
}

void Clear() {
	GetManagers().text.Clear();
}

} // namespace text

namespace texture {

void Unload(TextureKey key) {
	GetManagers().texture.Unload(key);
}

bool Has(TextureKey key) {
	return GetManagers().texture.Has(key);
}

Texture Get(TextureKey key) {
	return GetManagers().texture.Get(key);
}

void Clear() {
	GetManagers().texture.Clear();
}

} // namespace texture

namespace shader {

void Unload(ShaderKey key) {
	GetManagers().shader.Unload(key);
}

bool Has(ShaderKey key) {
	return GetManagers().shader.Has(key);
}

Shader Get(ShaderKey key) {
	return GetManagers().shader.Get(key);
}

void Clear() {
	GetManagers().shader.Clear();
}

} // namespace shader

namespace scene {

bool Has(SceneKey key) {
	return GetManagers().scene.Has(key);
}

void Unload(SceneKey key) {
	GetManagers().scene.Unload(key);
}

void SetActive(SceneKey key) {
	PTGN_CHECK(
		Has(key) || key == impl::start_scene_key,
		"Cannot set active scene if it has not been loaded into the scene "
		"manager"
	);
	GetManagers().scene.SetActive(key);
}

void AddActive(SceneKey key) {
	PTGN_CHECK(
		Has(key), "Cannot add active scene if it has not been loaded into the "
				  "scene manager"
	);
	GetManagers().scene.AddActive(key);
}

void RemoveActive(SceneKey key) {
	PTGN_CHECK(
		Has(key), "Cannot remove active scene if it has not been loaded into "
				  "the scene manager"
	);
	GetManagers().scene.RemoveActive(key);
}

std::shared_ptr<Scene> Get(SceneKey key) {
	PTGN_CHECK(
		Has(key),
		"Cannot get scene if it has not been loaded into the scene manager"
	);
	auto scene{ GetManagers().scene.Get(key) };
	PTGN_ASSERT(scene != nullptr, "Cannot get scene which has been destroyed");
	return scene;
}

std::vector<std::shared_ptr<Scene>> GetActive() {
	return GetManagers().scene.GetActive();
}

void Update(float dt) {
	GetManagers().scene.Update(dt);
}

} // namespace scene

} // namespace ptgn
