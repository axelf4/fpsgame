#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <SDL.h>
#include <gl/glew.h>
#include <SDL_opengl.h>
#include <vmath.h>

#include "state.h"
#include "gameState.h"
#include "pngloader.h"
#include "glUtil.h"
#include "widget.h"
#include "spriteBatch.h"
#include "label.h"
#include "image.h"

int main(int argc, char *arcv[]) {
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		printf("SDL_Init Error: %s\n", SDL_GetError());
		return 1;
	}
	SDL_SetRelativeMouseMode(SDL_TRUE); // Capture mouse and use relative coordinates
	SDL_Window *window;
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
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0.3, 0.7, 0.1, 1.0);

	// GUI code

	struct Font *font = loadFont("DejaVuSans.ttf", 512, 512);
	if (!font) {
		printf("Could not load font.");
	}

	// rect drawing
	png_uint_32 width, height;
	GLuint cat = loadPNGTexture("cat.png", &width, &height);
	if (!cat) {
		fprintf(stderr, "Failed to load png image.\n");
	}
	struct SpriteBatch *batch = spriteBatchCreate(1);
	batch->projectionMatrix = MatrixOrtho(0, 800, 600, 0, -1, 1);

	struct Widget *flexLayout = malloc(sizeof(struct FlexLayout));
	flexLayoutInitialize(flexLayout, DIRECTION_ROW, ALIGN_START);

	struct Widget *image = malloc(sizeof(struct Image));
	imageInitialize(image, cat, width, height, 0);
	struct FlexParams params0 = { ALIGN_END, -1, 100, UNDEFINED, 20, 0, 20, 20 };
	image->layoutParams = &params0;
	containerAddChild(flexLayout, image);

	struct Widget *testWidget2 = malloc(sizeof(struct Image));
	imageInitialize(testWidget2, cat, width, height, 0);
	struct FlexParams params1 = { ALIGN_CENTER, 1, UNDEFINED, UNDEFINED, 0, 0, 0, 0 };
	testWidget2->layoutParams = &params1;
	containerAddChild(flexLayout, testWidget2);

	struct Widget *label = labelNew(font, "Axel ffi! and the AV. HHHHHHHH Hi! (215): tv-hund. fesflhslg");
	struct FlexParams params2 = {ALIGN_CENTER, 1, 100, UNDEFINED, 0, 0, 0, 50};
	label->layoutParams = &params2;
	containerAddChild(flexLayout, label);
	// end of GUI code

	struct StateManager manager;
	struct GameState gameState;
	gameStateInitialize(&gameState, batch);
	setState(&manager, (struct State *) &gameState);

	const Uint8 *state = SDL_GetKeyboardState(NULL);
	int running = 1;
	SDL_Event event;
	while (running) {
		while (SDL_PollEvent(&event)) switch (event.type) {
			case SDL_QUIT:
				running = 0;
				break;
			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
					int width = event.window.data1,
						height = event.window.data2;
					printf("window resized to %d,%d\n", width, height);
					flexLayout->vtable->layout(flexLayout, width, MEASURE_EXACTLY, height, MEASURE_EXACTLY);
					batch->projectionMatrix = MatrixOrtho(0, width, height, 0, -1, 1);
					glViewport(0, 0, width, height);
				}
				break;
		}
		if (state[SDL_SCANCODE_ESCAPE]) running = 0;

		// Draw sprites
		/*spriteBatchBegin(batch);
		widgetValidate(flexLayout, 800, 600);
		widgetDraw(flexLayout, batch);
		spriteBatchEnd(batch);*/

		manager.state->update(manager.state, 1);
		manager.state->draw(manager.state, 1);

		SDL_GL_SwapWindow(window);
	}
	SDL_HideWindow(window);

	gameStateDestroy(&gameState);
	containerDestroy(flexLayout);
	glDeleteTextures(1, &cat);
	spriteBatchDestroy(batch);
	fontDestroy(font);

	SDL_Quit();

	return 0;
}
