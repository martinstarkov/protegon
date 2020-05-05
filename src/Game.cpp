#include "Game.h"

Game* Game::instance = nullptr;
SDL_Window* Game::window = nullptr;
SDL_Renderer* Game::renderer = nullptr;
bool Game::running = false;

Game::Game() {
	tm = TextureManager::getInstance();
	ih = InputHandler::getInstance();
	player = Player::getInstance();
}

void Game::init() {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) { // failure
		std::cout << "SDL failed to init" << std::endl;
	}
	window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_FLAGS);
	if (window) {
		renderer = SDL_CreateRenderer(window, -1, 0);
		if (renderer) {
			std::cout << "SDL window and renderer init successful" << std::endl;
		} else {
			std::cout << "SDL renderer failed to init" << std::endl;
		}
	} else {
		std::cout << "SDL window failed to init" << std::endl;
	}
	running = true;
}

void Game::update() {
	ih->update();
	player->update();
}

void Game::render() {
	SDL_RenderClear(renderer); // clear screen
	SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
	SDL_Rect rect = player->getPosition().Vec2DtoSDLRect(player->getSize());
	SDL_RenderDrawRect(renderer, &rect);
	SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
	SDL_RenderDrawRect(renderer, &SDL_Rect({ 512, 512, 32, 32 }));
	SDL_RenderDrawRect(renderer, &SDL_Rect({ 544, 512, 32, 32 }));
	SDL_RenderDrawRect(renderer, &SDL_Rect({ 576, 512, 32, 32 }));
	SDL_RenderDrawRect(renderer, &SDL_Rect({ 480, 512, 32, 32 }));
	SDL_RenderDrawRect(renderer, &SDL_Rect({ 448, 512, 32, 32 }));
	SDL_RenderDrawRect(renderer, &SDL_Rect({ 416, 512, 32, 32 }));
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderPresent(renderer); // display
}

void Game::loop() {
	const int FPS = 60;
	const int fDelay = 1000 / FPS;
	Uint32 fStart;
	int fTime;
	while (running) {
		fStart = SDL_GetTicks();
		update();
		render();
		fTime = SDL_GetTicks() - fStart;
		if (fDelay > fTime) {
			SDL_Delay(fDelay - fTime);
		}
	}
}

void Game::clean() {
	SDL_DestroyWindow(window);
	SDL_DestroyRenderer(renderer);
	//Quit SDL subsystems
	IMG_Quit();
	SDL_Quit();
}

void Game::quit() {
	running = false;
}