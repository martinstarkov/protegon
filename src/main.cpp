#include "Game.h"

int main(int argc, char* args[]) { // sdl main override

    Game& game = Game::getInstance();
    game.init();
    game.loop();
    game.clean();

    return 0;
}