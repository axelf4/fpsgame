#include "gameState.h"
#include <stdio.h>
#include <SDL.h>
#include "pngloader.h"
#include "glUtil.h"
#include "ddsloader.h"

#define MOUSE_SENSITIVITY 0.006f
#define MOVEMENT_SPEED .05f

static void gameStateUpdate(struct State *state, float dt) {
	struct GameState *gameState = (struct GameState *) state;
	int x, y;
	Uint32 button = SDL_GetRelativeMouseState(&x, &y);
	gameState->yaw -= x * MOUSE_SENSITIVITY;
	gameState->pitch -= y * MOUSE_SENSITIVITY;
	if (gameState->yaw > M_PI) gameState->yaw -= 2 * M_PI;
	else if (gameState->yaw < -M_PI) gameState->yaw += 2 * M_PI;
	if (gameState->pitch > M_PI / 2) gameState->pitch = M_PI / 2;
	else if (gameState->pitch < -M_PI / 2) gameState->pitch = -M_PI / 2;
	VECTOR forward = VectorSet(-MOVEMENT_SPEED * sin(gameState->yaw), 0, -MOVEMENT_SPEED * cos(gameState->yaw), 0),
		   up = VectorSet(0, 1, 0, 0),
		   right = Vector3Cross(forward, up);
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	VECTOR position = gameState->position;
	if (keys[SDL_SCANCODE_W]) position = VectorAdd(position, forward);
	if (keys[SDL_SCANCODE_A]) position = VectorSubtract(position, right);
	if (keys[SDL_SCANCODE_S]) position = VectorSubtract(position, forward);
	if (keys[SDL_SCANCODE_D]) position = VectorAdd(position, right);
	if (keys[SDL_SCANCODE_SPACE]) position = VectorAdd(position, VectorSet(0, MOVEMENT_SPEED, 0, 0));
	if (keys[SDL_SCANCODE_LSHIFT]) position = VectorSubtract(position, VectorSet(0, MOVEMENT_SPEED, 0, 0));
	gameState->position = position;
}

static void gameStateDraw(struct State *state, float dt) {
	struct GameState *gameState = (struct GameState *) state;
	struct SpriteBatch *batch = gameState->batch;

	// Clear the screen
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	ALIGN(16) float vv[4], mv[16];
	MATRIX rotationViewMatrix = MatrixRotationQuaternion(QuaternionRotationRollPitchYaw(gameState->pitch, gameState->yaw, 0));
	gameState->view = MatrixInverse(MatrixMultiply(
				rotationViewMatrix,
				MatrixTranslationFromVector(gameState->position)
				));

	// Draw the skybox
	MATRIX modelView = MatrixMultiply(gameState->model, gameState->view);
	glUseProgram(gameState->skyboxProgram);
	glUniformMatrix4fv(glGetUniformLocation(gameState->skyboxProgram, "invProjection"), 1, GL_FALSE, MatrixGet(mv, MatrixInverse(gameState->projection)));
	glUniformMatrix4fv(glGetUniformLocation(gameState->skyboxProgram, "modelView"), 1, GL_FALSE, MatrixGet(mv, modelView));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, gameState->skyboxTexture);
	glBindBuffer(GL_ARRAY_BUFFER, gameState->skyboxVertexBuffer);
	glEnableVertexAttribArray(glGetAttribLocation(gameState->skyboxProgram, "vertex"));
	glVertexAttribPointer(glGetAttribLocation(gameState->skyboxProgram, "vertex"), 2, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gameState->skyboxIndexBuffer);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	// Draw the model
	struct Model *model = gameState->objModel;
	glUseProgram(gameState->program);
	glUniformMatrix4fv(gameState->viewUniform, 1, GL_FALSE, MatrixGet(mv, gameState->view));
	glBindBuffer(GL_ARRAY_BUFFER, model->vertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->indexBuffer);
	glEnableVertexAttribArray(gameState->posAttrib);
	glVertexAttribPointer(gameState->posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawElements(GL_TRIANGLES, model->indexCount, GL_UNSIGNED_INT, 0);
	glDisableVertexAttribArray(gameState->posAttrib);

	spriteBatchBegin(batch);
	spriteBatchDraw(batch, gameState->cat, 0, 0, 100, 100);
	spriteBatchEnd(batch);
}

void gameStateInitialize(struct GameState *gameState, struct SpriteBatch *batch) {
	struct State *state = (struct State *) gameState;
	state->update = gameStateUpdate;
	state->draw = gameStateDraw;
	gameState->batch = batch;

	const GLchar *vertexShaderSource = "#version 150 core\n"
		"in vec3 position;"
		"uniform mat4 model;"
		"uniform mat4 view;"
		"uniform mat4 projection;"
		"void main() {"
		"	gl_Position = projection * model * view * vec4(position, 1.0);"
		"}",
		*fragmentShaderSource = "#version 150 core\n"
			"out vec4 outColor;"
			"void main() {"
			"	outColor = vec4(1.0, 0.0, 0.0, 1.0);"
			"}";
	GLuint program = createProgram(vertexShaderSource, fragmentShaderSource);
	glBindFragDataLocation(program, 0, "outColor");
	glLinkProgram(program);
	gameState->program = program;

	// Specify the layout of the vertex data
	GLint posAttrib = glGetAttribLocation(program, "position");
	// Get the location of program uniforms
	GLint modelUniform = glGetUniformLocation(program, "model");
	GLint viewUniform = glGetUniformLocation(program, "view");
	GLint projectionUniform = glGetUniformLocation(program, "projection");
	gameState->posAttrib = posAttrib;
	gameState->modelUniform = modelUniform;
	gameState->viewUniform = viewUniform;
	gameState->projectionUniform = projectionUniform;

	ALIGN(16) float vv[4], mv[16];
	glUseProgram(program);
	// TODO rename to model, view, projection
	MATRIX model = MatrixIdentity(),
		   view,
		   projection = MatrixPerspective(90.f, 800.0f / 600.0f, 1.0f, 3000.0f);
	glUniformMatrix4fv(modelUniform, 1, GL_FALSE, MatrixGet(mv, model));
	glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, MatrixGet(mv, projection));
	gameState->model = model;
	gameState->view = view;
	gameState->projection = projection;

	// Skybox
	char *skyboxData = readFile("skybox.dds");
	gameState->skyboxTexture = dds_load_texture_from_memory(skyboxData, 0, 0, 0);
	free(skyboxData);
	if (!gameState->skyboxTexture) {
		printf("Failed to load skybox texture.\n");
	}
	glBindTexture(GL_TEXTURE_CUBE_MAP, gameState->skyboxTexture);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	const GLchar *skyboxVertexShaderSource = "attribute vec2 vertex;"
		"uniform mat4 invProjection;"
		"uniform mat4 modelView;"
		"varying vec3 eyeDirection;"
		"void main() {"
		"	eyeDirection = vec3(invProjection * (gl_Position = vec4(vertex, 0.0, 1.0)) * modelView);"
		"}",
		*skyboxFragmentShaderSource = "varying vec3 eyeDirection;"
			"uniform samplerCube texture;"
			"void main() {"
			"	gl_FragColor = textureCube(texture, eyeDirection);"
			// "	gl_FragDepth = 1.0;"
			"}";
	gameState->skyboxProgram = createProgram(skyboxVertexShaderSource, skyboxFragmentShaderSource);
	glLinkProgram(gameState->skyboxProgram);
	const float skyboxVertices[] = { -1, -1, 1, -1, 1, 1, -1, 1 };
	const unsigned int cubeIndices[] = { 0, 1, 2, 0, 2, 3 };
	GLuint skyboxVertexBuffer, skyboxIndexBuffer;
	glGenBuffers(1, &skyboxVertexBuffer);
	glGenBuffers(1, &skyboxIndexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), skyboxVertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyboxIndexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(unsigned int), cubeIndices, GL_STATIC_DRAW);
	gameState->skyboxVertexBuffer = skyboxVertexBuffer;
	gameState->skyboxIndexBuffer = skyboxIndexBuffer;

	gameState->position = VectorSet(0, 0, 0, 1);
	gameState->yaw = 0;
	gameState->pitch = 0;
	gameState->objModel = loadModelFromObj("cube.obj");
	if (!gameState->objModel) {
		printf("Failed to load model.\n");
	}

	png_uint_32 width, height;
	GLuint cat = loadPNGTexture("cat.png", &width, &height);
	if (!cat) {
		fprintf(stderr, "Failed to load png image.\n");
	}
	gameState->cat = cat;
}

void gameStateDestroy(struct GameState *gameState) {
	glDeleteProgram(gameState->program);
	glDeleteProgram(gameState->skyboxProgram);
	glDeleteTextures(1, &gameState->skyboxTexture);
	glDeleteBuffers(1, &gameState->skyboxVertexBuffer);
	glDeleteBuffers(1, &gameState->skyboxIndexBuffer);

	destroyModel(gameState->objModel);

	glDeleteTextures(1, &gameState->cat);
}
