#include "Game.h"

int main(int argc, char* args[]) {

    Game* game = Game::getInstance();

    game->init();

    game->loop();

    game->clean();

    delete game;
    game = nullptr;

    return 0;
}