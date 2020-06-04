
#include "InputHandler.h"
#include "Game.h"
//#include "Player.h"
//#include "Camera.h"
//#include "LevelController.h"

std::unique_ptr<InputHandler> InputHandler::_instance = nullptr;
const Uint8* InputHandler::_states = nullptr;
//
//static Player* player;
//static Camera* camera;

InputHandler& InputHandler::getInstance() {
	if (!_instance) {
		_instance = std::make_unique<InputHandler>();
	}
	return *_instance;
}

void InputHandler::keyStateCheck() {
	//playerMotion();
	//cameraMotion();
	//if (states[SDL_SCANCODE_X]) {
	//	Game::bulletTime = true;
	//}
	//if (!states[SDL_SCANCODE_X]) {
	//	if (Game::bulletTime) {
	//		player->setVelocity(Vec2D());
	//		Game::getInstance()->reset();
	//	}
	//	Game::bulletTime = false;
	//}
}

void InputHandler::playerMotion() {
	//if (states[SDL_SCANCODE_A] && !states[SDL_SCANCODE_D]) {
	//	player->accelerate(Keys::LEFT);
	//}
	//if (states[SDL_SCANCODE_D] && !states[SDL_SCANCODE_A]) {
	//	player->accelerate(Keys::RIGHT);
	//}
	//if ((states[SDL_SCANCODE_W] || states[SDL_SCANCODE_SPACE]) && !states[SDL_SCANCODE_S]) {
	//	player->accelerate(Keys::UP);
	//}
	//if (states[SDL_SCANCODE_S] && !(states[SDL_SCANCODE_W] || states[SDL_SCANCODE_SPACE])) {
	//	player->accelerate(Keys::DOWN);
	//}
	//if ((!states[SDL_SCANCODE_A] && !states[SDL_SCANCODE_D]) || (states[SDL_SCANCODE_A] && states[SDL_SCANCODE_D])) {
	//	player->stop(Axis::HORIZONTAL);
	//}
	//if (!states[SDL_SCANCODE_W] && !states[SDL_SCANCODE_S] || (states[SDL_SCANCODE_W] && states[SDL_SCANCODE_S])) {
	//	player->stop(Axis::VERTICAL);
	//	// do nothing with gravity, without gravity player->stop(VERTICAL);
	//}
}

void InputHandler::cameraMotion() {
	//if (states[SDL_SCANCODE_LEFT] && !states[SDL_SCANCODE_RIGHT]) {
	//	camera->addPosition(Vec2D(1.0f, 0.0f));
	//}
	//if (states[SDL_SCANCODE_RIGHT] && !states[SDL_SCANCODE_LEFT]) {
	//	camera->addPosition(Vec2D(-1.0f, 0.0f));
	//}
	//if (states[SDL_SCANCODE_UP] && !states[SDL_SCANCODE_DOWN]) {
	//	camera->addPosition(Vec2D(0.0f, 1.0f));
	//}
	//if (states[SDL_SCANCODE_DOWN] && !states[SDL_SCANCODE_UP]) {
	//	camera->addPosition(Vec2D(0.0f, -1.0f));
	//}
	//if (states[SDL_SCANCODE_Q] && !states[SDL_SCANCODE_E]) {
	//	camera->multiplyScale(1.0f + CAMERA_ZOOM_SPEED);
	//}
	//if (states[SDL_SCANCODE_E] && !states[SDL_SCANCODE_Q]) {
	//	camera->multiplyScale(1.0f - CAMERA_ZOOM_SPEED);
	//}
}

//Vec2D InputHandler::getMousePosition() {
//	int x, y;
//	SDL_GetMouseState(&x, &y);
//	return Vec2D(x, y);
//}

void InputHandler::keyPress(SDL_KeyboardEvent press) {
	switch (press.keysym.scancode) {
		case SDL_SCANCODE_C: {
			//player->shoot();
			break;
		}
		case SDL_SCANCODE_R: {
			//std::cout << "Resetting game..." << std::endl;
			//if (LevelController::getCurrentLevel()->getId() == 4) {
			//	Game::attempts = 1;
			//}
			//LevelController::changeCurrentLevel(-99);
			//Game::getInstance()->reset();
			break;
		}
		default:
			break;
	}
}

void InputHandler::keyRelease(SDL_KeyboardEvent release) {}

void InputHandler::update() {
	SDL_Event event;
	_states = SDL_GetKeyboardState(NULL);
	keyStateCheck();
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				Game::getInstance().quit();
				break;
			case SDL_KEYDOWN:
				keyPress(event.key);
				break;
			case SDL_KEYUP:
				keyRelease(event.key);
				break;
			case SDL_MOUSEMOTION:
				break;
			case SDL_MOUSEBUTTONDOWN:
				break;
			case SDL_MOUSEBUTTONUP:
				break;
			default:
				break;
		}
	}
}
