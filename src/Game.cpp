#include "Game.h"

Game* Game::instance = nullptr;
bool Game::running = false;
TextureManager* Game::tm = nullptr;
std::string Game::input = "";

void Game::init() {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) { // failure
		std::cout << "SDL failed to init" << std::endl;
	}
	tm = TextureManager::getInstance();
	running = true;
}

void Game::update() {
	input = "tile";
}

void Game::render() {
	SDL_RenderClear(tm->getRenderer());
	SDL_Rect dest = { 128, 128, 64, 64 };
	SDL_SetRenderDrawColor(tm->getRenderer(), 0, 0, 255, 255);
	//SDL_RenderDrawRect(renderer, &tileSquare);
	SDL_RenderDrawRect(tm->getRenderer(), &dest);
	SDL_SetRenderDrawColor(tm->getRenderer(), 255, 255, 255, 255);
	SDL_RenderPresent(tm->getRenderer());
}

void Game::loop() {
	while (running) {
		//handleEvents
		update();
		render();
	}
}

void Game::quit() {
	tm->clean();
	//Quit SDL subsystems
	IMG_Quit();
	SDL_Quit();
}
