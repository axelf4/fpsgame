#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#include <vmath.h>
#include "state.h"
#include "spriteBatch.h"
#include "gameState.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

SDL_Window *window;
struct StateManager manager;
struct GameState gameState;
struct SpriteBatch batch;
Uint64 frequency, lastTime = 0;
int running = 1;

static void update() {
	const Uint8 *state = SDL_GetKeyboardState(NULL);
	SDL_Event event;
	while (SDL_PollEvent(&event)) switch (event.type) {
		case SDL_QUIT:
			running = 0;
			break;
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
				Sint32 width = event.window.data1,
					   height = event.window.data2;
				printf("window resized to %d,%d\n", width, height);
				batch.projectionMatrix = MatrixOrtho(0, width, height, 0, -1, 1);
				glViewport(0, 0, width, height);
				manager.state->resize(manager.state, width, height);
			}
			break;
	}
	if (state[SDL_SCANCODE_ESCAPE]) running = 0;

	Uint64 now = SDL_GetPerformanceCounter();
	float dt = (now - lastTime) * 1000.0f / frequency;
	lastTime = now;

	manager.state->update(manager.state, dt);
	manager.state->draw(manager.state, dt);

	GLenum error;
	while ((error = glGetError()) != GL_NO_ERROR) {
		fprintf(stderr, "OpenGL error: %d\n", error);
	}

	SDL_GL_SwapWindow(window);
}

int main(int argc, char *arcv[]) {
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
	printf("Starting the engine.\n");
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		printf("SDL_Init Error: %s\n", SDL_GetError());
		return 1;
	}
	SDL_SetRelativeMouseMode(SDL_TRUE); // Capture mouse and use relative coordinates
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8);
	if (!(window = SDL_CreateWindow("Hello", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE))) {
		fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}
	SDL_GLContext glcontext = SDL_GL_CreateContext(window);
	GLenum err = glewInit();
	if (err != GLEW_OK) {
		printf("glewInit Error: %s\n", glewGetErrorString(err));
	}

	glEnable(GL_CULL_FACE);
	glClearColor(0.3, 0.7, 0.1, 1.0);

	spriteBatchInitialize(&batch, 32);
	batch.projectionMatrix = MatrixOrtho(0, 800, 600, 0, -1, 1);

	gameStateInitialize(&gameState, &batch);
	setState(&manager, (struct State *) &gameState);

	frequency = SDL_GetPerformanceFrequency();

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(update, 0, 1);
#else
	while (running) {
		update();
	}
#endif

	/*SDL_HideWindow(window);

	  gameStateDestroy(&gameState);
	  spriteBatchDestroy(&batch);

	  SDL_Quit();*/

	return 0;
}
