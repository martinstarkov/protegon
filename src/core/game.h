#pragma once

#include <memory> // std::unique_ptr

#include "core/sdl_instance.h"
#include "core/resource_manager.h"
#include "protegon/font.h"
#include "protegon/sound.h"
#include "protegon/text.h"
#include "protegon/texture.h"

namespace ptgn {

struct Game {
public:
	Game() = default;
	~Game() = default;
	SDLInstance sdl;
	ResourceManager<Font> font;
	ResourceManager<Sound> sound;
	ResourceManager<Music> music;
	ResourceManager<Text> text;
	ResourceManager<Texture> texture;
};

namespace global {

namespace hidden {

extern std::unique_ptr<Game> game;

} // namespace hidden

void InitGame();
void DestroyGame();
Game& GetGame();

} // namespace global

} // namespace ptgn