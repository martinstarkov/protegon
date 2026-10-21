#include "runtime/scripting/scripts/audio_scripts.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "app/application_context.h"
#include "app/project.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/math_utils.h"
#include "core/math/noise.h"
#include "core/math/rng.h"
#include "core/math/tolerance.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/animation/scripted_animation.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/sprite_stack.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/platformer_jump.h"
#include "runtime/physics/platformer_jump_registry.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scene/scene_transitions.h"
#include "runtime/scripting/script_common.h"
#include "runtime/ui/button.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"
#include "runtime/world/tilemap.h"

namespace ptgn {

void PlaySoundScript::OnStart() {
	if (sound.value.empty()) {
		PTGN_WARN("Play Sound requires a non empty audio asset key");

		return;
	}

	GetScene().ctx().audio.Play(sound, volume, loops, frequency_ratio, exclusive, force_restart);
}

void AudioStopSoundScript::OnStart() {
	if (!target) {
		return;
	}
	auto& audio{ GetScene().ctx().audio };
	AudioKey key{ text };
	if (key.value.empty()) {
		return;
	}
	audio.Stop(key);
}

void AudioPauseSoundScript::OnStart() {
	if (!target) {
		return;
	}
	auto& audio{ GetScene().ctx().audio };
	AudioKey key{ text };
	if (key.value.empty()) {
		return;
	}
	audio.Pause(key);
}

void AudioResumeSoundScript::OnStart() {
	if (!target) {
		return;
	}
	auto& audio{ GetScene().ctx().audio };
	AudioKey key{ text };
	if (key.value.empty()) {
		return;
	}
	audio.Resume(key);
}

void AudioToggleSoundPauseScript::OnStart() {
	if (!target) {
		return;
	}
	auto& audio{ GetScene().ctx().audio };
	AudioKey key{ text };
	if (key.value.empty()) {
		return;
	}
	audio.TogglePause(key);
}

void AudioSetSoundVolumeScript::OnStart() {
	if (!target) {
		return;
	}
	auto& audio{ GetScene().ctx().audio };
	AudioKey key{ text };
	if (key.value.empty()) {
		return;
	}
	audio.SetVolume(key, number);
}

void AudioToggleSoundVolumeScript::OnStart() {
	if (!target) {
		return;
	}
	auto& audio{ GetScene().ctx().audio };
	AudioKey key{ text };
	if (key.value.empty()) {
		return;
	}
	audio.ToggleVolume(key, number);
}

void AudioSetPlaybackSpeedScript::OnStart() {
	if (!target) {
		return;
	}
	auto& audio{ GetScene().ctx().audio };
	AudioKey key{ text };
	if (key.value.empty()) {
		return;
	}
	audio.SetFrequencyRatio(key, number);
}

void AudioSetMasterVolumeScript::OnStart() {
	if (!target) {
		return;
	}
	auto& audio{ GetScene().ctx().audio };
	audio.SetVolume(number);
}

void AudioToggleMasterVolumeScript::OnStart() {
	if (!target) {
		return;
	}
	auto& audio{ GetScene().ctx().audio };
	audio.ToggleVolume(number);
}

void AudioStopAllAudioScript::OnStart() {
	if (!target) {
		return;
	}
	auto& audio{ GetScene().ctx().audio };
	audio.StopAll();
}

void AudioPauseAllAudioScript::OnStart() {
	if (!target) {
		return;
	}
	auto& audio{ GetScene().ctx().audio };
	audio.PauseAll();
}

void AudioResumeAllAudioScript::OnStart() {
	if (!target) {
		return;
	}
	auto& audio{ GetScene().ctx().audio };
	audio.ResumeAll();
}

void AudioFadeInSoundScript::OnStart() {
	if (!target) {
		return;
	}
	auto& audio{ GetScene().ctx().audio };
	AudioKey key{ text };
	if (key.value.empty()) {
		return;
	}
	if (!audio.IsPlaying(key)) {
		audio.Play(key, 0.0f);
	} else {
		audio.SetVolume(key, 0.0f);
	}
	PropertyTo<float>(
		target, SequenceChannelKey{ "audio.fade." + key.value }, 1.0f,
		milliseconds{ static_cast<std::int64_t>(std::max(1.0f, number)) },
		[key](Entity owner_entity) { return owner_entity.GetScene().ctx().audio.GetVolume(key); },
		[key](Entity owner_entity, float volume) {
			owner_entity.GetScene().ctx().audio.SetVolume(key, volume);
		}
	);
}

void AudioFadeOutSoundScript::OnStart() {
	if (!target) {
		return;
	}
	AudioKey key{ text };
	if (key.value.empty()) {
		return;
	}
	PropertyTo<float>(
		target, SequenceChannelKey{ "audio.fade." + key.value }, 0.0f,
		milliseconds{ static_cast<std::int64_t>(std::max(1.0f, number)) },
		[key](Entity owner_entity) { return owner_entity.GetScene().ctx().audio.GetVolume(key); },
		[key](Entity owner_entity, float volume) {
			owner_entity.GetScene().ctx().audio.SetVolume(key, volume);
		}
	);
}

} // namespace ptgn
