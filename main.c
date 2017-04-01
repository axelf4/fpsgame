#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <SDL.h>
#include <gl/glew.h>
#include <SDL_opengl.h>
#include <vmath.h>

#include "pngloader.h"
#include "glUtil.h"
#include "model.h"
#include "widget.h"
#include "renderer.h"
#include "label.h"
#include "image.h"

#define MOUSE_SENSITIVITY 0.006f
#define MOVEMENT_SPEED .05f

int main(int argc, char *arcv[]) {
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		printf("SDL_Init Error: %s\n", SDL_GetError());
		return 1;
	}
	// SDL_SetRelativeMouseMode(SDL_TRUE); // Capture mouse and use relative coordinates
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

	ALIGN(16) float vv[4], mv[16];

	const char objPath[] = "cube.obj";
	struct Model *model = loadModelFromObj(objPath);

	const GLchar *vertexShaderSource = "#version 150 core\n"
		"in vec3 position;"
		"uniform mat4 model;"
		"uniform mat4 view;"
		"uniform mat4 projection;"
		"void main() {"
		"	gl_Position = projection * model * view * vec4(position, 1.0);"
		"}";
	const GLchar *fragmentShaderSource = "#version 150 core\n"
		"out vec4 outColor;"
		"void main() {"
		"	outColor = vec4(1.0, 0.0, 0.0, 1.0);"
		"}";
	GLuint program = createProgram(vertexShaderSource, fragmentShaderSource);
	glBindFragDataLocation(program, 0, "outColor");
	glLinkProgram(program);

	// Specify the layout of the vertex data
	GLint posAttrib = glGetAttribLocation(program, "position");
	// Get the location of program uniforms
	GLint modelUniform = glGetUniformLocation(program, "model");
	GLint viewUniform = glGetUniformLocation(program, "view");
	GLint projectionUniform = glGetUniformLocation(program, "projection");

	glUseProgram(program);
	// TODO rename to model, view, projection
	MATRIX modelMatrix = MatrixIdentity(),
		   viewMatrix,
		   projectionMatrix = MatrixPerspective(90.f, 800.0f / 600.0f, 1.0f, 3000.0f);
	glUniformMatrix4fv(modelUniform, 1, GL_FALSE, MatrixGet(mv, modelMatrix));
	glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, MatrixGet(mv, projectionMatrix));

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
	struct SpriteRenderer *spriteRenderer = spriteRendererCreate(1);
	spriteRenderer->projectionMatrix = MatrixOrtho(0, 800, 600, 0, -1, 1);
	struct TextRenderer *textRenderer = textRendererCreate();

	FlexLayout flexLayout;
	flexLayoutInitialize(&flexLayout, DIRECTION_ROW, ALIGN_START);

	struct Widget *image = malloc(sizeof(struct Image));
	imageInitialize(image, cat, width, height, 0);
	struct FlexParams params0 = { ALIGN_END, -1, 100, UNDEFINED, 20, 0, 20, 20 };
	image->layoutParams = &params0;
	containerAddChild((struct Widget *) &flexLayout, image);

	struct Widget *testWidget2 = malloc(sizeof(struct Image));
	imageInitialize(testWidget2, cat, width, height, 0);
	struct FlexParams params1 = { ALIGN_CENTER, 1, UNDEFINED, UNDEFINED, 0, 0, 0, 0 };
	testWidget2->layoutParams = &params1;
	containerAddChild((struct Widget *) &flexLayout, testWidget2);

	const char text[] = "Axel ffi! and the AV. HHHHHHHH Hi! (215): tv-hund. fesflhslg";
	struct Widget *label = labelNew(font, textRenderer, text);
	struct FlexParams params2 = {ALIGN_CENTER, 1, 100, UNDEFINED, 0, 0, 0, 50};
	label->layoutParams = &params2;
	containerAddChild((struct Widget *) &flexLayout, label);
	// end of GUI code

	VECTOR position = VectorSet(0, 0, 0, 1);
	float yaw = 0, pitch = 0;

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
					((struct Widget *) &flexLayout)->vtable->layout((struct  Widget *) &flexLayout, width, MEASURE_EXACTLY, height, MEASURE_EXACTLY);
					spriteRenderer->projectionMatrix = MatrixOrtho(0, width, height, 0, -1, 1);
					glViewport(0, 0, width, height);
				}
				break;
		}
		if (state[SDL_SCANCODE_ESCAPE]) running = 0;

		VECTOR forward, up, right;
		forward = VectorSet(MOVEMENT_SPEED * sin(yaw), 0, MOVEMENT_SPEED * cos(yaw), 0);
		up = VectorSet(0, 1, 0, 0);
		right = Vector3Cross(forward, up);
		if (state[SDL_SCANCODE_W]) position = VectorAdd(position, forward);
		if (state[SDL_SCANCODE_A]) position = VectorAdd(position, right);
		if (state[SDL_SCANCODE_S]) position = VectorSubtract(position, forward);
		if (state[SDL_SCANCODE_D]) position = VectorSubtract(position, right);
		if (state[SDL_SCANCODE_LSHIFT]) position = VectorAdd(position, VectorSet(0, MOVEMENT_SPEED, 0, 0));
		if (state[SDL_SCANCODE_SPACE]) position = VectorSubtract(position, VectorSet(0, MOVEMENT_SPEED, 0, 0));

		int x, y;
		Uint32 button = SDL_GetRelativeMouseState(&x, &y);
		yaw += x * MOUSE_SENSITIVITY;
		pitch -= y * MOUSE_SENSITIVITY;

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Draw the model
		glUseProgram(program);
		viewMatrix = MatrixTranslationFromVector(position);
		MATRIX rotationViewMatrix = MatrixRotationQuaternion(QuaternionRotationRollPitchYaw(pitch, yaw, 0));
		viewMatrix = MatrixMultiply(rotationViewMatrix, viewMatrix);
		viewMatrix = MatrixInverse(viewMatrix);
		glUniformMatrix4fv(viewUniform, 1, GL_FALSE, MatrixGet(mv, viewMatrix));

		glBindBuffer(GL_ARRAY_BUFFER, model->vertexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->indexBuffer);
		glEnableVertexAttribArray(posAttrib);
		glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glDrawElements(GL_TRIANGLES, model->indexCount, GL_UNSIGNED_INT, 0);
		glDisableVertexAttribArray(posAttrib);

		// Draw sprites
		spriteRendererBegin(spriteRenderer);

		widgetValidate((struct Widget *) &flexLayout, 800, 600);
		((struct Widget *) &flexLayout)->vtable->draw((struct Widget *) &flexLayout, spriteRenderer);

		spriteRendererEnd(spriteRenderer);

		SDL_GL_SwapWindow(window);
	}
	SDL_HideWindow(window);

	containerDestroy((struct Container *) &flexLayout);
	glDeleteTextures(1, &cat);
	spriteRendererDestroy(spriteRenderer);
	fontDestroy(font);
	glDeleteProgram(program);
	destroyModel(model);

	SDL_Quit();

	return 0;
}
