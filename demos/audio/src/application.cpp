#include <algorithm>
#include <functional>
#include <string>

#include "audio/audio.h"
#include "components/draw.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/time.h"
#include "core/window.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "renderer/text.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tile/grid.h"
#include "ui/button.h"

using namespace ptgn;

class AudioScene : public Scene {
public:
	int channel1{ 1 };
	int channel2{ 2 };

	int starting_volume{ 30 };

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
		const TextContent& content, const std::function<void()>& on_activate,
		const Color& bg_color = color::LightGray
	) {
		Button b{ CreateTextButton(*this, content, color::Black) };
		b.SetBackgroundColor(bg_color);
		b.SetBackgroundColor(color::Gray, ButtonState::Hover);
		b.SetBackgroundColor(color::DarkGray, ButtonState::Pressed);
		b.SetBorderColor(color::LightGray);
		b.SetBorderWidth(3.0f);
		b.OnActivate(on_activate);
		return b;
	}

	void Enter() override {
		game.window.SetResizable();
		game.music.Load("music1", "resources/music1.ogg");
		game.sound.Load("sound1", "resources/sound1.ogg");
		game.music.Load("music2", "resources/music2.ogg");
		game.sound.Load("sound2", "resources/sound2.ogg");

		game.music.SetVolume(starting_volume);
		game.sound.SetVolume("sound1", starting_volume);
		game.sound.SetVolume("sound2", starting_volume);

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
						  "Play Music 1", []() { game.music.Play("music1"); }, music_color
					  )
		);
		grid.Set(
			{ 1, 1 }, CreateAudioButton(
						  "Play Music 2", []() { game.music.Play("music2"); }, music_color
					  )
		);
		grid.Set(
			{ 1, 2 }, CreateAudioButton(
						  "Stop Music", []() { game.music.Stop(); }, music_color
					  )
		);
		grid.Set(
			{ 1, 3 }, CreateAudioButton(
						  "Fade In Music 1 (3s)",
						  []() { game.music.FadeIn("music1", milliseconds{ 3000 }); }, music_color
					  )
		);
		grid.Set(
			{ 1, 4 }, CreateAudioButton(
						  "Fade In Music 2 (3s)",
						  []() { game.music.FadeIn("music2", milliseconds{ 3000 }); }, music_color
					  )
		);
		grid.Set(
			{ 1, 5 }, CreateAudioButton(
						  "Fade Out Music (3s)", []() { game.music.FadeOut(milliseconds{ 3000 }); },
						  music_color
					  )
		);
		grid.Set(
			{ 1, 6 }, CreateAudioButton(
						  "Toggle Music Pause", []() { game.music.TogglePause(); }, music_color
					  )
		);
		grid.Set(
			{ 1, 7 }, CreateAudioButton(
						  "Toggle Music Mute",
						  [this]() { game.music.ToggleVolume(starting_volume); }, music_color
					  )
		);
		grid.Set(
			{ 1, 8 },
			CreateAudioButton(
				"+ Music Volume",
				[]() { game.music.SetVolume(std::clamp(game.music.GetVolume() + 5, 0, 128)); },
				music_color
			)
		);
		grid.Set(
			{ 1, 9 },
			CreateAudioButton(
				"- Music Volume",
				[]() { game.music.SetVolume(std::clamp(game.music.GetVolume() - 5, 0, 128)); },
				music_color
			)
		);

		grid.Set(
			{ 2, 0 },
			CreateAudioButton(
				"Play Channel 1", [this]() { game.sound.Play("sound1", channel1); }, sound1_color
			)
		);
		grid.Set(
			{ 2, 1 }, CreateAudioButton(
						  "Stop Channel 1", [this]() { game.sound.Stop(channel1); }, sound1_color
					  )
		);
		grid.Set(
			{ 2, 2 }, CreateAudioButton(
						  "Fade In Sound 1 (3s)",
						  [this]() { game.sound.FadeIn("sound1", milliseconds{ 3000 }, channel1); },
						  sound1_color
					  )
		);
		grid.Set(
			{ 2, 3 },
			CreateAudioButton(
				"Fade Out Channel 1 (3s)",
				[this]() { game.sound.FadeOut(milliseconds{ 3000 }, channel1); }, sound1_color
			)
		);
		grid.Set(
			{ 2, 4 }, CreateAudioButton(
						  "Toggle Channel 1 Pause", [this]() { game.sound.TogglePause(channel1); },
						  sound1_color
					  )
		);
		grid.Set(
			{ 2, 5 },
			CreateAudioButton(
				"Toggle Sound 1 Mute",
				[this]() { game.sound.ToggleVolume("sound1", starting_volume); }, sound1_color
			)
		);
		grid.Set(
			{ 2, 6 }, CreateAudioButton(
						  "+ Channel 1 Volume",
						  [this]() {
							  game.sound.SetVolume(
								  channel1, std::clamp(game.sound.GetVolume(channel1) + 5, 0, 128)
							  );
						  },
						  sound1_color
					  )
		);
		grid.Set(
			{ 2, 7 }, CreateAudioButton(
						  "- Channel 1 Volume",
						  [this]() {
							  game.sound.SetVolume(
								  channel1, std::clamp(game.sound.GetVolume(channel1) - 5, 0, 128)
							  );
						  },
						  sound1_color
					  )
		);

		grid.Set(
			{ 3, 0 },
			CreateAudioButton(
				"Play Channel 2", [this]() { game.sound.Play("sound2", channel2); }, sound2_color
			)
		);
		grid.Set(
			{ 3, 1 }, CreateAudioButton(
						  "Stop Channel 2", [this]() { game.sound.Stop(channel2); }, sound2_color
					  )
		);
		grid.Set(
			{ 3, 2 }, CreateAudioButton(
						  "Fade In Sound 2 (3s)",
						  [this]() { game.sound.FadeIn("sound2", milliseconds{ 3000 }, channel2); },
						  sound2_color
					  )
		);
		grid.Set(
			{ 3, 3 },
			CreateAudioButton(
				"Fade Out Channel 2 (3s)",
				[this]() { game.sound.FadeOut(milliseconds{ 3000 }, channel2); }, sound2_color
			)
		);
		grid.Set(
			{ 3, 4 }, CreateAudioButton(
						  "Toggle Channel 2 Pause", [this]() { game.sound.TogglePause(channel2); },
						  sound2_color
					  )
		);
		grid.Set(
			{ 3, 5 },
			CreateAudioButton(
				"Toggle Sound 2 Mute",
				[this]() { game.sound.ToggleVolume("sound2", starting_volume); }, sound2_color
			)
		);
		grid.Set(
			{ 3, 6 }, CreateAudioButton(
						  "+ Channel 2 Volume",
						  [this]() {
							  game.sound.SetVolume(
								  channel2, std::clamp(game.sound.GetVolume(channel2) + 5, 0, 128)
							  );
						  },
						  sound2_color
					  )
		);
		grid.Set(
			{ 3, 7 }, CreateAudioButton(
						  "- Channel 2 Volume",
						  [this]() {
							  game.sound.SetVolume(
								  channel2, std::clamp(game.sound.GetVolume(channel2) - 5, 0, 128)
							  );
						  },
						  sound2_color
					  )
		);

		V2_int offset{ 6, 6 };
		V2_int size{ (game.renderer.GetGameSize() - offset * (grid.GetSize() + V2_int{ 1, 1 })) /
					 grid.GetSize() };

		grid.ForEach([size, offset](auto coord, Button& b) {
			if (b != Button{}) {
				SetPosition(
					b, -game.renderer.GetGameSize() * 0.5f + coord * (size + offset) + offset
				);
				SetDrawOrigin(b, Origin::TopLeft);
				b.SetSize(size);
			}
		});
	}

	void Exit() override {
		game.music.Clear();
		game.sound.Clear();
	}

	void Update() override {
		b1.SetTextContent(std::string("Music Volume: ") + std::to_string(game.music.GetVolume()));
		b2.SetTextContent(
			std::string("Music Is Playing: ") + (game.music.IsPlaying() ? "true" : "false")
		);
		b3.SetTextContent(
			std::string("Music Is Paused: ") + (game.music.IsPaused() ? "true" : "false")
		);
		b4.SetTextContent(
			std::string("Music Is Fading: ") + (game.music.IsFading() ? "true" : "false")
		);
		b5.SetTextContent(
			std::string("Channel 1 Volume: ") + std::to_string(game.sound.GetVolume(channel1))
		);
		b6.SetTextContent(
			std::string("Channel 2 Volume: ") + std::to_string(game.sound.GetVolume(channel2))
		);
		b7.SetTextContent(
			std::string("Channel 1 Playing: ") + (game.sound.IsPlaying(channel1) ? "true" : "false")
		);
		b8.SetTextContent(
			std::string("Channel 2 Playing: ") + (game.sound.IsPlaying(channel2) ? "true" : "false")
		);
		b9.SetTextContent(
			std::string("Channel 1 Paused: ") + (game.sound.IsPaused(channel1) ? "true" : "false")
		);
		b10.SetTextContent(
			std::string("Channel 2 Paused: ") + (game.sound.IsPaused(channel2) ? "true" : "false")
		);
		b11.SetTextContent(
			std::string("Channel 1 Fading: ") + (game.sound.IsFading(channel1) ? "true" : "false")
		);
		b12.SetTextContent(
			std::string("Channel 2 Fading: ") + (game.sound.IsFading(channel2) ? "true" : "false")
		);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("AudioScene", { 800, 800 });
	game.scene.Enter<AudioScene>("");
	return 0;
}