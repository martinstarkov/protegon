// #pragma once
//
// #include <memory>
//
// #include "core/asset/asset_handle.h"
// #include "ecs/components/generic.h"
// #include "core/util/file.h"
// #include "core/util/time.h"
//
// namespace ptgn {
//
// namespace music {
//
// constexpr int max_volume{ 128 };
//
//// @param loops The number of loops to play the music for, -1 for infinite looping.
// void Play(const ResourceHandle& key, int loops = -1);
//
//// @param fade_time How long to fade the music in for.
//// @param loops The number of loops to play the music for, -1 for infinite looping.
// void FadeIn(const ResourceHandle& key, milliseconds fade_time, int loops = -1);
//
//// Pause the currently playing music.
// void Pause();
//
//// Resume the currently playing music.
// void Resume();
//
//// Toggles the pause state of the music.
// void TogglePause();
//
//// @return The current music track volume in range [0, 128].
//[[nodiscard]] int GetVolume();
//
//// @param volume Volume of the music in range [0, 128].
// void SetVolume(int volume);
//
//// Toggles the volume between 0 and new_volume.
//// @param new_volume When toggle unmutes, it will set the new volume of the music to this value
//// in range [0, 128].
// void ToggleVolume(int new_volume = max_volume);
//
//// Stop the currently playing music.
// void Stop();
//
//// @param fade_time Time over which to fade the music out.
// void FadeOut(milliseconds fade_time);
//
//// @return True if any music is currently playing, false otherwise.
//[[nodiscard]] bool IsPlaying();
//
//// @return True if the currently playing music is paused, false otherwise.
//[[nodiscard]] bool IsPaused();
//
//// @return True if the currently playing music is fading in OR out, false otherwise.
//[[nodiscard]] bool IsFading();
//
//} // namespace music
//
// namespace sound {
//
// constexpr int max_volume{ 128 };
//
//// @param channel The channel on which to play the sound on, -1 plays on the first available
//// channel.
//// @param loops Number of times to loop sound, -1 for infinite looping.
// void Play(const ResourceHandle& key, int channel = -1, int loops = 0);
//
//// @param fade_time Time over which to fade the sound in.
//// @param channel The channel on which to play the sound on, -1 plays on the first available
//// channel.
//// @param loops Number of times to loop sound, -1 for infinite looping.
// void FadeIn(const ResourceHandle& key, milliseconds fade_time, int channel = -1, int loops = 0);
//
//// Set volume of the sound. Volume range: [0, 128].
// void SetVolume(const ResourceHandle& key, int volume);
//
//// Set volume of the channel.
//// @param channel The channel for which the volume is set, -1 sets the volume for all sound
//// channels.
//// @param volume Volume of the sound channel. Volume range: [0, 128].
// void SetVolume(int channel, int volume);
//
//// @return Volume of the sound. Volume range: [0, 128].
//[[nodiscard]] int GetVolume(const ResourceHandle& key);
//
//// Toggles the sound volume between 0 and new_volume.
//// @param new_volume When toggle unmutes, it will set the new volume of the sound to this value
//// in range [0, 128].
// void ToggleVolume(const ResourceHandle& key, int new_volume = max_volume);
//
//// Stops the sound playing on the specified channel, -1 stops all sound channels.
// void Stop(int channel);
//
//// Resumes the sound playing on the specified channel, -1 resumes all paused sound channels.
// void Resume(int channel);
//
//// Pauses the sound playing on the specified channel, -1 pauses all sound channels.
// void Pause(int channel);
//
//// Toggles the pause state of the channel.
// void TogglePause(int channel);
//
//// @param fade_time Time over which to fade the sound in.
//// @param channel The channel on which to play the sound on, -1 plays on the first available
//// channel.
// void FadeOut(milliseconds fade_time, int channel);
//
//// @param channel The channel for which to query the volume, -1 gets the average of all sound
//// channels.
//// @return Volume of the sound. Volume range: [0, 128].
//[[nodiscard]] int GetVolume(int channel);
//
//// @return True if the sound channel is playing, -1 to check if any channel is playing.
//[[nodiscard]] bool IsPlaying(int channel);
//
//// @return True if the sound channel is paused, -1 to check if any channel is paused.
//[[nodiscard]] bool IsPaused(int channel);
//
//// @return True if the sound channel is fading in or out, -1 to check if any channel is playing.
//[[nodiscard]] bool IsFading(int channel);
//
//} // namespace sound
//
//} // namespace ptgn