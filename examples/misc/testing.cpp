#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// ============================================================
// TRANSITIONS
// ============================================================

struct Transition {
	virtual ~Transition()			= default;
	virtual void Update(float dt)	= 0;
	virtual bool IsFinished() const = 0;
};

struct TimedTransition : public Transition {
	float duration;
	float time = 0.f;

	TimedTransition(float d) : duration(d) {}

	void Update(float dt) override {
		time += dt;
	}

	bool IsFinished() const override {
		return time >= duration;
	}
};

// ============================================================
// ENUMS
// ============================================================

enum class SceneState {
	Active,
	TransitionIn,
	TransitionOut
};

enum class CommandType {
	Enter,
	Exit,
	ReEnter
};

// ============================================================
// FORWARD
// ============================================================
struct SceneManager;

// ============================================================
// COMMAND
// ============================================================

class Scene;

struct SceneCommand {
	CommandType type;
	std::string target_key;
	std::string caller_key;
	int priority = 0;

	std::function<std::unique_ptr<Scene>()> factory;

	std::function<std::unique_ptr<Transition>()> inTransitionFactory;
	std::function<std::unique_ptr<Transition>()> outTransitionFactory;
};

// ============================================================
// LOCAL SCENE MANAGER
// ============================================================

struct LocalSceneManager {
	SceneManager* manager;
	std::string owner_key;

	bool CanIssueCommands() const;

	template <typename T, typename... Args>
	void Enter(
		const std::string& key, std::function<std::unique_ptr<Transition>()> inT, int priority,
		Args&&... args
	);

	void Exit(
		const std::string& key, std::function<std::unique_ptr<Transition>()> outT, int priority
	);

	template <typename T, typename... Args>
	void ReEnter(
		const std::string& key, std::function<std::unique_ptr<Transition>()> inT,
		std::function<std::unique_ptr<Transition>()> outT, Args&&... args
	);
};

// ============================================================
// SCENE
// ============================================================

class Scene {
public:
	std::string key;
	SceneState state = SceneState::Active;
	bool locked		 = false;

	std::unique_ptr<Transition> transition;

	LocalSceneManager local;

	Scene(const std::string& k, SceneManager* mgr) : key(k), local{ mgr, k } {}

	virtual ~Scene() = default;

	virtual void Update(float dt) {}
};

// ============================================================
// SCENE MANAGER
// ============================================================

struct SceneManager {
	std::vector<std::unique_ptr<Scene>> scenes;
	std::vector<SceneCommand> commandBuffer;

	std::unordered_map<std::string, std::string> reenterMap;

	Scene* Find(const std::string& key) {
		for (auto& s : scenes) {
			if (s->key == key) {
				return s.get();
			}
		}
		return nullptr;
	}

	void AddCommand(SceneCommand cmd) {
		commandBuffer.push_back(std::move(cmd));
	}

	bool CanModifyTarget(Scene* target) {
		if (!target) {
			return true; // entering new scene is fine
		}

		if (target->state == SceneState::TransitionIn) {
			return false;
		}
		if (target->state == SceneState::TransitionOut) {
			return false;
		}

		return true;
	}

	// ---------------- RESOLVE ----------------

	std::unordered_map<std::string, SceneCommand> Resolve() {
		std::unordered_map<std::string, std::vector<SceneCommand>> grouped;

		for (auto& c : commandBuffer) {
			grouped[c.target_key].push_back(std::move(c));
		}

		std::unordered_map<std::string, SceneCommand> resolved;

		for (auto& [key, cmds] : grouped) {
			// ReEnter wins
			auto re = std::find_if(cmds.begin(), cmds.end(), [](auto& c) {
				return c.type == CommandType::ReEnter;
			});
			if (re != cmds.end()) {
				resolved[key] = std::move(*re);
				continue;
			}

			std::vector<SceneCommand*> exits;
			std::vector<SceneCommand*> enters;

			for (auto& c : cmds) {
				if (c.type == CommandType::Exit) {
					exits.push_back(&c);
				} else if (c.type == CommandType::Enter) {
					enters.push_back(&c);
				}
			}

			auto pick = [](auto& vec) {
				return *std::max_element(vec.begin(), vec.end(), [](auto* a, auto* b) {
					return a->priority < b->priority;
				});
			};

			if (!exits.empty()) {
				resolved[key] = std::move(*pick(exits));
			} else if (!enters.empty()) {
				resolved[key] = std::move(*pick(enters));
			}
		}

		return resolved;
	}

	// ---------------- APPLY ----------------

	void Apply(auto& resolved) {
		for (auto& [key, cmd] : resolved) {
			Scene* scene = Find(key);

			switch (cmd.type) {
				case CommandType::Enter: {
					Scene* target = Find(cmd.target_key);

					// Block if target exists and is transitioning
					if (target && !CanModifyTarget(target)) {
						break;
					}

					auto newScene	= cmd.factory();
					newScene->state = SceneState::TransitionIn;
					// TODO: Figure out delay system.
					/*if (!cmd.use_delay) {
						s->scene_on_entered = true;
						newScene->OnEnter();
					}*/
					// newScene->use_delay = cmd.use_delay;
					newScene->transition = cmd.inTransitionFactory();
					scenes.push_back(std::move(newScene));
					break;
				}

				case CommandType::Exit: {
					if (!scene) {
						break;
					}
					if (!CanModifyTarget(scene)) {
						break;
					}
					scene->state	  = SceneState::TransitionOut;
					scene->locked	  = true;
					scene->transition = cmd.outTransitionFactory();
					break;
				}

				case CommandType::ReEnter: {
					if (!scene) {
						break;
					}
					if (!CanModifyTarget(scene)) {
						break;
					}

					std::string temp = key + "_reenter";
					auto newScene	 = cmd.factory();
					newScene->key	 = temp;
					newScene->state	 = SceneState::TransitionIn;
					// TODO: Figure out delay system.
					/*if (!cmd.use_delay) {
						s->scene_on_entered = true;
						newScene->OnEnter();
					}*/
					// newScene->use_delay = cmd.use_delay;
					newScene->transition = cmd.inTransitionFactory();

					scene->state	  = SceneState::TransitionOut;
					scene->locked	  = true;
					scene->transition = cmd.outTransitionFactory();

					scenes.push_back(std::move(newScene));
					reenterMap[temp] = key;
					break;
				}
			}
		}
	}

	// ---------------- UPDATE ----------------

	void Update(float dt) {
		for (auto& s : scenes) {
			s->Update(dt);
		}

		auto resolved = Resolve();
		Apply(resolved);
		commandBuffer.clear();

		// Update transitions
		for (auto& s : scenes) {
			// TODO: Put a check here for delay:
			/*
			if (s->use_delay) {
				s->delay_elapsed += dt;
				if (s->delay_elapsed >= s->delay) {
					s->OnEnter();
					s->scene_on_entered = true;
					s->use_delay = false;
				}
			}
			*/
			if (s->transition) {
				s->transition->Update(dt);

				if (s->transition->IsFinished()) {
					if (s->state == SceneState::TransitionIn) {
						s->state = SceneState::Active;
					}

					s->transition.reset();
				}
			} else {
				if (s->state == SceneState::TransitionIn) {
					s->state = SceneState::Active;
					/*		if (!s->scene_on_entered) {
								s->OnEnter();
								s->scene_on_entered = true;
							}*/
				}
			}
		}

		// TODO: Move this erase into the upper for loop.
		// Cleanup
		scenes.erase(
			std::remove_if(
				scenes.begin(), scenes.end(),
				[](auto& s) { return s->state == SceneState::TransitionOut && !s->transition; }
			),
			scenes.end()
		);

		// Reenter finalize
		for (auto it = reenterMap.begin(); it != reenterMap.end();) {
			Scene* s = Find(it->first);
			if (s && s->state == SceneState::Active) {
				// TODO: This can be done by simply removing _reenter from scene key when it turns
				// active.
				s->key = it->second;
				it	   = reenterMap.erase(it);
			} else {
				++it;
			}
		}
	}
};

// ============================================================
// LOCAL IMPLEMENTATION
// ============================================================

bool LocalSceneManager::CanIssueCommands() const {
	Scene* s = manager->Find(owner_key);
	if (!s) {
		return false;
	}
	if (s->state == SceneState::TransitionIn) {
		return false;
	}
	if (s->locked) {
		return false;
	}
	return true;
}

template <typename T, typename... Args>
void LocalSceneManager::Enter(
	const std::string& key, std::function<std::unique_ptr<Transition>()> inT, int priority,
	Args&&... args
) {
	if (!CanIssueCommands()) {
		return;
	}
	// TODO: Check CanModifyTarget here for key.
	manager->AddCommand(SceneCommand{
		CommandType::Enter, key, owner_key, priority,
		std::function([=]() { return std::make_unique<T>(key, manager, args...); }), inT, nullptr }
	);
}

void LocalSceneManager::Exit(
	const std::string& key, std::function<std::unique_ptr<Transition>()> outT, int priority
) {
	if (!CanIssueCommands()) {
		return;
	}
	// TODO: Check CanModifyTarget here for key.

	manager->AddCommand({ CommandType::Exit, key, owner_key, priority, nullptr, nullptr, outT });
}

template <typename T, typename... Args>
void LocalSceneManager::ReEnter(
	const std::string& key, std::function<std::unique_ptr<Transition>()> inT,
	std::function<std::unique_ptr<Transition>()> outT, Args&&... args
) {
	if (!CanIssueCommands()) {
		return;
	}

	// TODO: Check CanModifyTarget here for key.
	manager->AddCommand({ CommandType::ReEnter, key, owner_key, INT_MAX,
						  [=]() { return std::make_unique<T>(key, manager, args...); }, inT, outT }
	);
}

// ============================================================
// EXAMPLE SCENES
// ============================================================

struct GameScene : Scene {
	int counter = 0;

	GameScene(const std::string& k, SceneManager* mgr) : Scene(k, mgr) {}

	void Update(float dt) override;
};

struct MenuScene : Scene {
	int ticks = 0;

	MenuScene(const std::string& k, SceneManager* mgr) : Scene(k, mgr) {}

	void Update(float dt) override;
};

void MenuScene::Update(float dt) {
	ticks++;
	std::cout << "MenuScene update " << ticks << "\n";

	if (ticks == 3) {
		local.ReEnter<MenuScene>(
			"menu", [] { return std::make_unique<TimedTransition>(1.f); },
			[] { return std::make_unique<TimedTransition>(1.f); }
		);
	}
}

void GameScene::Update(float dt) {
	counter++;
	std::cout << "GameScene update " << counter << "\n";

	if (counter == 2) {
		local.Enter<MenuScene>("menu", [] { return std::make_unique<TimedTransition>(1.f); }, 1);
	}

	if (counter == 4) {
		local.Exit("game", [] { return std::make_unique<TimedTransition>(1.f); }, 2);
	}
}

// ============================================================
// MAIN LOOP
// ============================================================

int main() {
	SceneManager mgr;

	mgr.scenes.push_back(std::make_unique<GameScene>("game", &mgr));

	for (int i = 0; i < 10; ++i) {
		std::cout << "--- Frame " << i << " ---\n";
		mgr.Update(0.5f);
	}
}

/*
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#define WINDOW_W   800
#define WINDOW_H   600
#define WRAP_WIDTH 300

GLuint texture_from_surface(SDL_Surface* surf) {
	SDL_Surface* converted = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
		converted->pixels
	);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	SDL_DestroySurface(converted);

	return tex;
}

int main(int argc, char* argv[]) {
	SDL_Init(SDL_INIT_VIDEO);
	TTF_Init();

	SDL_Window* window =
		SDL_CreateWindow("SDL3_ttf OpenGL Test", WINDOW_W, WINDOW_H, SDL_WINDOW_OPENGL);

	SDL_GLContext glctx = SDL_GL_CreateContext(window);

	glViewport(0, 0, WINDOW_W, WINDOW_H);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, WINDOW_W, WINDOW_H, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	TTF_Font* font = TTF_OpenFont("assets/Arial.ttf", 24);
	if (!font) {
		printf("Font load error: %s\n", SDL_GetError());
		return 1;
	}

	char text[4096]	   = "Wrapping test: ";
	const char* source = "The quick brown fox jumps over the lazy dog. ";

	int source_index = 0;
	Uint64 last_add	 = SDL_GetTicks();

	SDL_Color white = { 255, 255, 255, 255 };

	int running = 1;

	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT) {
				running = 0;
			}
		}

		Uint64 now = SDL_GetTicks();

		size_t len = strlen(text);

		if (now - last_add > 50) {
			text[len]	  = source[source_index];
			text[len + 1] = '\0';

			source_index++;
			if (source[source_index] == '\0') {
				source_index = 0;
			}

			last_add = now;
		}

		glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		SDL_Surface* surf = TTF_RenderText_Blended_Wrapped(font, text, len, white, WRAP_WIDTH);

		if (surf) {
			GLuint tex = texture_from_surface(surf);

			float x = 50.5f;
			float y = 50.5f;
			float w = (float)surf->w;
			float h = (float)surf->h;

			glBindTexture(GL_TEXTURE_2D, tex);

			glBegin(GL_QUADS);

			glTexCoord2f(0.0f, 0.0f);
			glVertex2f(x, y);

			glTexCoord2f(1.0f, 0.0f);
			glVertex2f(x + w, y);

			glTexCoord2f(1.0f, 1.0f);
			glVertex2f(x + w, y + h);

			glTexCoord2f(0.0f, 1.0f);
			glVertex2f(x, y + h);

			glEnd();

			glDeleteTextures(1, &tex);
			SDL_DestroySurface(surf);
		}

		SDL_GL_SwapWindow(window);
	}

	TTF_CloseFont(font);

	SDL_GL_DestroyContext(glctx);
	SDL_DestroyWindow(window);

	TTF_Quit();
	SDL_Quit();

	return 0;
}
*/