#include "Game.h"

int main(int argc, char* args[]) {

    Game* game = Game::getInstance();

    game->init();

    game->loop();

    game->clean();

    return 0;
}