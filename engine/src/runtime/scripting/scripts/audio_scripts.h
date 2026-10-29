#pragma once

#include "runtime/scripting/script_common.h"

namespace ptgn {

struct PlaySoundScript : public Script {
	AudioKey sound{};
	float volume{ 1.0f };
	int loops{ 0 };
	float frequency_ratio{ 1.0f };
	bool exclusive{ false };
	bool force_restart{ true };
	PlaySoundScript() = default;

	explicit PlaySoundScript(AudioKey audio_key, float audio_volume = 1.0f, int audio_loops = 0) :
		sound{ std::move(audio_key) }, volume{ audio_volume }, loops{ audio_loops } {}

	void OnStart() override;
	PTGN_REFLECT(PlaySoundScript, sound, volume, loops, frequency_ratio, exclusive, force_restart)
};

struct AudioStopSoundScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(AudioStopSoundScript, text)
};

struct AudioPauseSoundScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(AudioPauseSoundScript, text)
};

struct AudioResumeSoundScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(AudioResumeSoundScript, text)
};

struct AudioToggleSoundPauseScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(AudioToggleSoundPauseScript, text)
};

struct AudioSetSoundVolumeScript : public Script {
	void OnStart() override;
	std::string text{};
	float number{ 1.0f };

	PTGN_REFLECT(AudioSetSoundVolumeScript, text, number)
};

struct AudioToggleSoundVolumeScript : public Script {
	void OnStart() override;
	std::string text{};
	float number{ 1.0f };

	PTGN_REFLECT(AudioToggleSoundVolumeScript, text, number)
};

struct AudioSetMasterVolumeScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(AudioSetMasterVolumeScript, number)
};

struct AudioToggleMasterVolumeScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(AudioToggleMasterVolumeScript, number)
};

struct AudioStopAllAudioScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(AudioStopAllAudioScript)
};

struct AudioPauseAllAudioScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(AudioPauseAllAudioScript)
};

struct AudioResumeAllAudioScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(AudioResumeAllAudioScript)
};

struct AudioFadeInSoundScript : public Script {
	void OnStart() override;
	std::string text{};
	float number{ 1.0f };

	PTGN_REFLECT(AudioFadeInSoundScript, text, number)
};

struct AudioFadeOutSoundScript : public Script {
	void OnStart() override;
	std::string text{};
	float number{ 1.0f };

	PTGN_REFLECT(AudioFadeOutSoundScript, text, number)
};

struct AudioSetPlaybackSpeedScript : public Script {
	void OnStart() override;
	std::string text{};
	float number{ 1.0f };

	PTGN_REFLECT(AudioSetPlaybackSpeedScript, text, number)
};

} // namespace ptgn
