#include "InputHandler.h"
#include "Game.h"
#include "Player.h"
#include "Camera.h"

InputHandler* InputHandler::instance = nullptr;

static Player* player;
static Camera* camera;

InputHandler::InputHandler() {
	player = Player::getInstance();
	camera = Camera::getInstance();
}

void InputHandler::keyStateCheck() {
	const Uint8* states = SDL_GetKeyboardState(NULL);
	playerMotion(states);
	cameraMotion(states);
	if (states[SDL_SCANCODE_X]) {
		Game::bulletTime = true;
	}
	if (!states[SDL_SCANCODE_X]) {
		if (Game::bulletTime) {
			player->setVelocity(Vec2D());
			Game::reset();
		}
		Game::bulletTime = false;
	}
}

void InputHandler::playerMotion(const Uint8* states) {
	if (states[SDL_SCANCODE_A] && !states[SDL_SCANCODE_D]) {
		player->accelerate(Keys::LEFT);
	}
	if (states[SDL_SCANCODE_D] && !states[SDL_SCANCODE_A]) {
		player->accelerate(Keys::RIGHT);
	}
	if ((states[SDL_SCANCODE_W] || states[SDL_SCANCODE_SPACE]) && !states[SDL_SCANCODE_S]) {
		player->accelerate(Keys::UP);
	}
	if (states[SDL_SCANCODE_S] && !(states[SDL_SCANCODE_W] || states[SDL_SCANCODE_SPACE])) {
		player->accelerate(Keys::DOWN);
	}
	if ((!states[SDL_SCANCODE_A] && !states[SDL_SCANCODE_D]) || (states[SDL_SCANCODE_A] && states[SDL_SCANCODE_D])) {
		player->stop(Axis::HORIZONTAL);
	}
	if (!states[SDL_SCANCODE_W] && !states[SDL_SCANCODE_S] || (states[SDL_SCANCODE_W] && states[SDL_SCANCODE_S])) {
		player->stop(Axis::VERTICAL);
		// do nothing with gravity, without gravity player->stop(VERTICAL);
	}
}

void InputHandler::cameraMotion(const Uint8* states) {
	if (states[SDL_SCANCODE_LEFT] && !states[SDL_SCANCODE_RIGHT]) {
		camera->addPosition(Vec2D(-1.0f, 0.0f));
	}
	if (states[SDL_SCANCODE_RIGHT] && !states[SDL_SCANCODE_LEFT]) {
		camera->addPosition(Vec2D(1.0f, 0.0f));
	}
	if (states[SDL_SCANCODE_UP] && !states[SDL_SCANCODE_DOWN]) {
		camera->addPosition(Vec2D(0.0f, -1.0f));
	}
	if (states[SDL_SCANCODE_DOWN] && !states[SDL_SCANCODE_UP]) {
		camera->addPosition(Vec2D(0.0f, 1.0f));
	}
}

void InputHandler::keyPress(SDL_KeyboardEvent press) {
	switch (press.keysym.scancode) {
		case SDL_SCANCODE_C:
			//SDL_RenderClear(Game::getRenderer());
			for (Entity* entity : Game::entityObjects) {
				entity->setAcceleration(Vec2D(1.0f / float(rand() % 19 + (-9)), 1.0f / float(rand() % 19 + (-9))));
			}
			break;
		case SDL_SCANCODE_R:
			Game::reset();
			break;
		case SDL_SCANCODE_T:
			Game::reset();
			player->setPosition(Vec2D(10 + 700 - 128 * 4, 1));
			break;
		default:
			break;
	}
}

void InputHandler::keyRelease(SDL_KeyboardEvent release) {}

void InputHandler::update() {
	SDL_Event event;
	keyStateCheck();
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				Game::getInstance()->quit();
				break;
			case SDL_KEYDOWN:
				keyPress(event.key);
				break;
			case SDL_KEYUP:
				keyRelease(event.key);
				break;
			default:
				break;
		}
	}
}
