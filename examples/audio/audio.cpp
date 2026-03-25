#include "runtime/audio/audio.h"

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>

#include "app/application.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/scene/scene.h"

#include "runtime/ui/button.h"
#include "runtime/world/grid.h"

using namespace ptgn;

class AudioScene : public Scene {
public:
	float volume_increment{ 0.25f };
	float starting_volume{ 0.25f };

	Grid<Button> grid{ { 4, 12 } };

	Button b1;
	Button b2;
	Button b3;
	Button b4;
	Button b5;
	Button b6;
	Button b7;
	Button b8;
	Button b9;
	Button b10;
	Button b11;
	Button b12;

	Color music_color{ color::Teal };
	Color sound1_color{ color::Gold };
	Color sound2_color{ color::LightPink };

	Button CreateAudioButton(
		std::string_view content, const std::function<void()>& on_activate,
		Color bg_color = color::LightGray
	) {
		Button b{ CreateButton(*this) };
		b.SetText(content, color::Black);
		b.SetBackgroundColor(bg_color);
		b.SetBackgroundColor(color::Gray, ButtonState::Hover);
		b.SetBackgroundColor(color::DarkGray, ButtonState::Press);
		b.SetBorderColor(color::LightGray);
		b.SetBorderWidth(3.0f);
		b.OnActivate(on_activate);
		return b;
	}

	void OnEnter() override {
		ctx().asset.Load("music1", "assets/music.ogg");
		ctx().asset.Load("music2", "assets/music2.ogg");
		ctx().asset.Load("sound1", "assets/sound.ogg");
		ctx().asset.Load("sound2", "assets/sound2.ogg");

		ctx().audio.SetVolume(starting_volume);
		ctx().audio.SetVolume("sound1", starting_volume);
		ctx().audio.SetVolume("sound2", starting_volume);

		b1	= grid.Set({ 0, 0 }, CreateAudioButton("Music Volume: ", nullptr, music_color));
		b2	= grid.Set({ 0, 1 }, CreateAudioButton("Music Is Playing: ", nullptr, music_color));
		b3	= grid.Set({ 0, 2 }, CreateAudioButton("Music Is Paused: ", nullptr, music_color));
		b4	= grid.Set({ 0, 3 }, CreateAudioButton("Music Is Fading: ", nullptr, music_color));
		b5	= grid.Set({ 0, 4 }, CreateAudioButton("Channel 1 Volume: ", nullptr, sound1_color));
		b7	= grid.Set({ 0, 5 }, CreateAudioButton("Channel 1 Playing: ", nullptr, sound1_color));
		b9	= grid.Set({ 0, 6 }, CreateAudioButton("Channel 1 Paused: ", nullptr, sound1_color));
		b11 = grid.Set({ 0, 7 }, CreateAudioButton("Channel 1 Fading: ", nullptr, sound1_color));
		b6	= grid.Set({ 0, 8 }, CreateAudioButton("Channel 2 Volume: ", nullptr, sound2_color));
		b8	= grid.Set({ 0, 9 }, CreateAudioButton("Channel 2 Playing: ", nullptr, sound2_color));
		b10 = grid.Set({ 0, 10 }, CreateAudioButton("Channel 2 Paused: ", nullptr, sound2_color));
		b12 = grid.Set({ 0, 11 }, CreateAudioButton("Channel 2 Fading: ", nullptr, sound2_color));

		grid.Set(
			{ 1, 0 }, CreateAudioButton(
						  "Play Music 1", [&]() { ctx().audio.Play("music1"); }, music_color
					  )
		);
		grid.Set(
			{ 1, 1 }, CreateAudioButton(
						  "Play Music 2", [&]() { ctx().audio.Play("music2"); }, music_color
					  )
		);
		grid.Set(
			{ 1, 2 }, CreateAudioButton(
						  "Stop Music", [&]() { ctx().audio.StopAll(); }, music_color
					  )
		);
		grid.Set(
			{ 1, 3 },
			CreateAudioButton(
				"Fade In Music 1 (3s)",
				[&]() { /* TODO: Fix: ctx().audio.FadeIn("music1", milliseconds{ 3000 });*/ },
				music_color
			)
		);
		grid.Set(
			{ 1, 4 },
			CreateAudioButton(
				"Fade In Music 2 (3s)",
				[&]() { /* TODO: Fix: ctx().audio.FadeIn("music2", milliseconds{ 3000 });*/ },
				music_color
			)
		);
		grid.Set(
			{ 1, 5 },
			CreateAudioButton(
				"Fade Out Music (3s)",
				[&]() { /* TODO: Fix: ctx().audio.FadeOut(milliseconds{ 3000 });*/ }, music_color
			)
		);
		grid.Set(
			{ 1, 6 },
			CreateAudioButton(
				"Toggle Music 1 Pause", [&]() { ctx().audio.TogglePause("music1"); }, music_color
			)
		);
		grid.Set(
			{ 1, 7 }, CreateAudioButton(
						  "Toggle Volume", [this]() { ctx().audio.ToggleVolume(starting_volume); },
						  music_color
					  )
		);
		grid.Set(
			{ 1, 8 }, CreateAudioButton(
						  "+ Music Volume",
						  [&]() {
							  ctx().audio.SetVolume(std::clamp(
								  ctx().audio.GetVolume() + volume_increment, kMinVolume, kMaxVolume
							  ));
						  },
						  music_color
					  )
		);
		grid.Set(
			{ 1, 9 }, CreateAudioButton(
						  "- Music Volume",
						  [&]() {
							  ctx().audio.SetVolume(std::clamp(
								  ctx().audio.GetVolume() - volume_increment, kMinVolume, kMaxVolume
							  ));
						  },
						  music_color
					  )
		);

		grid.Set(
			{ 2, 0 },
			CreateAudioButton(
				"Play Channel 1",
				[&]() { ctx().audio.Play("sound1", 1.0, 0, RandomNumber(0.01f, 100.0f)); },
				sound1_color
			)
		);
		grid.Set(
			{ 2, 1 }, CreateAudioButton(
						  "Stop Channel 1", [this]() { ctx().audio.Stop("sound1"); }, sound1_color
					  )
		);
		grid.Set(
			{ 2, 2 },
			CreateAudioButton(
				"Fade In Sound 1 (3s)",
				[this](
				) { /* TODO: Fix: ctx().audio.FadeIn("sound1", milliseconds{ 3000 }, channel1);*/ },
				sound1_color
			)
		);
		grid.Set(
			{ 2, 3 },
			CreateAudioButton(
				"Fade Out Channel 1 (3s)",
				[this]() { /* TODO: Fix: ctx().audio.FadeOut(milliseconds{ 3000 }, channel1);*/ },
				sound1_color
			)
		);
		grid.Set(
			{ 2, 4 }, CreateAudioButton(
						  "Toggle Channel 1 Pause", [this]() { ctx().audio.TogglePause("sound1"); },
						  sound1_color
					  )
		);
		grid.Set(
			{ 2, 5 },
			CreateAudioButton(
				"Toggle Sound 1 Mute",
				[this]() { ctx().audio.ToggleVolume("sound1", starting_volume); }, sound1_color
			)
		);
		grid.Set(
			{ 2, 6 }, CreateAudioButton(
						  "+ Channel 1 Volume",
						  [this]() {
							  ctx().audio.SetVolume(
								  "sound1", std::clamp(
												ctx().audio.GetVolume("sound1") + volume_increment,
												kMinVolume, kMaxVolume
											)
							  );
						  },
						  sound1_color
					  )
		);
		grid.Set(
			{ 2, 7 }, CreateAudioButton(
						  "- Channel 1 Volume",
						  [this]() {
							  ctx().audio.SetVolume(
								  "sound1", std::clamp(
												ctx().audio.GetVolume("sound1") - volume_increment,
												kMinVolume, kMaxVolume
											)
							  );
						  },
						  sound1_color
					  )
		);

		grid.Set(
			{ 3, 0 },
			CreateAudioButton(
				"Play Channel 2",
				[this]() { ctx().audio.Play("sound2", 1.0, 0, RandomNumber(0.1f, 2.0f)); },
				sound2_color
			)
		);
		grid.Set(
			{ 3, 1 }, CreateAudioButton(
						  "Stop Channel 2", [this]() { ctx().audio.Stop("sound2"); }, sound2_color
					  )
		);
		grid.Set(
			{ 3, 2 },
			CreateAudioButton(
				"Fade In Sound 2 (3s)",
				[this](
				) { /* TODO: Fix: ctx().audio.FadeIn("sound2", milliseconds{ 3000 }, channel2);*/ },
				sound2_color
			)
		);
		grid.Set(
			{ 3, 3 },
			CreateAudioButton(
				"Fade Out Channel 2 (3s)",
				[this]() { /* TODO: Fix: ctx().audio.FadeOut(milliseconds{ 3000 }, channel2);*/ },
				sound2_color
			)
		);
		grid.Set(
			{ 3, 4 }, CreateAudioButton(
						  "Toggle Channel 2 Pause", [this]() { ctx().audio.TogglePause("sound2"); },
						  sound2_color
					  )
		);
		grid.Set(
			{ 3, 5 },
			CreateAudioButton(
				"Toggle Sound 2 Mute",
				[this]() { ctx().audio.ToggleVolume("sound2", starting_volume); }, sound2_color
			)
		);
		grid.Set(
			{ 3, 6 }, CreateAudioButton(
						  "+ Channel 2 Volume",
						  [this]() {
							  ctx().audio.SetVolume(
								  "sound2", std::clamp(
												ctx().audio.GetVolume("sound2") + volume_increment,
												kMinVolume, kMaxVolume
											)
							  );
						  },
						  sound2_color
					  )
		);
		grid.Set(
			{ 3, 7 }, CreateAudioButton(
						  "- Channel 2 Volume",
						  [this]() {
							  ctx().audio.SetVolume(
								  "sound2", std::clamp(
												ctx().audio.GetVolume("sound2") - volume_increment,
												kMinVolume, kMaxVolume
											)
							  );
						  },
						  sound2_color
					  )
		);

		V2_int offset{ 6, 6 };
		V2_int size{ (ctx().renderer.GetGameSize() - offset * (grid.GetSize() + V2_int{ 1, 1 })) /
					 grid.GetSize() };

		grid.ForEach([&, size, offset](auto coord, Button& b) {
			if (b != Button{}) {
				SetPosition(
					b, -ctx().renderer.GetGameSize() * 0.5f + coord * (size + offset) + offset
				);
				SetDrawOrigin(b, Origin::TopLeft);
				b.SetShape(size);
			}
		});
	}

	void OnUpdate() override {
		b1.SetTextContent(std::string("Music Volume: ") + std::to_string(ctx().audio.GetVolume()));
		b2.SetTextContent(
			std::string("Music Is Playing: ") + (ctx().audio.IsPlaying("music1") ? "true" : "false")
		);
		b3.SetTextContent(
			std::string("Music Is Paused: ") + (ctx().audio.IsPaused("music1") ? "true" : "false")
		);
		b4.SetTextContent(std::string("Music Is Fading: "
		) /* TODO: Fix: + (ctx().audio.IsFading() ? "true" : "false")*/
		);
		b5.SetTextContent(
			std::string("Channel 1 Volume: ") + std::to_string(ctx().audio.GetVolume("sound1"))
		);
		b6.SetTextContent(
			std::string("Channel 2 Volume: ") + std::to_string(ctx().audio.GetVolume("sound2"))
		);
		b7.SetTextContent(
			std::string("Channel 1 Playing: ") +
			(ctx().audio.IsPlaying("sound1") ? "true" : "false")
		);
		b8.SetTextContent(
			std::string("Channel 2 Playing: ") +
			(ctx().audio.IsPlaying("sound2") ? "true" : "false")
		);
		b9.SetTextContent(
			std::string("Channel 1 Paused: ") + (ctx().audio.IsPaused("sound1") ? "true" : "false")
		);
		b10.SetTextContent(
			std::string("Channel 2 Paused: ") + (ctx().audio.IsPaused("sound2") ? "true" : "false")
		);
		b11.SetTextContent(std::string("Channel 1 Fading: "
		) /* TODO: Fix: + (ctx().audio.IsFading("sound1") ? "true" : "false")*/
		);
		b12.SetTextContent(std::string("Channel 2 Fading: "
		) /* TODO: Fix: + (ctx().audio.IsFading("sound2") ? "true" : "false")*/
		);
	}
};

int main(int, char**) {
	Application game{ "AudioScene" };
	game.StartWith<AudioScene>();
}