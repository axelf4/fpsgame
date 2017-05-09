#include "gameState.h"
#include <stdio.h>
#include <SDL.h>
#include "pngloader.h"
#include "glUtil.h"
#include "ddsloader.h"

#include "image.h"
#include "label.h"

#define MOUSE_SENSITIVITY 0.006f
#define MOVEMENT_SPEED .05f * 5

static const GLuint SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

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
	VECTOR forward = VectorSet(-MOVEMENT_SPEED * sin(gameState->yaw) * dt, 0, -MOVEMENT_SPEED * cos(gameState->yaw) * dt, 0),
		   up = VectorSet(0, 1, 0, 0),
		   right = VectorCross(forward, up);
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

MATRIX lookAt(VECTOR eye, VECTOR center, VECTOR up) {
	ALIGN(16) float eyeV[4], centerV[4], upV[4];
	VectorGet(eyeV, eye);
	VectorGet(centerV, center);
	VectorGet(upV, up);

	float z0 = eyeV[0] - centerV[0];
	float z1 = eyeV[1] - centerV[1];
	float z2 = eyeV[2] - centerV[2];

	float len = 1 / sqrt(z0 * z0 + z1 * z1 + z2 * z2);
	z0 *= len;
	z1 *= len;
	z2 *= len;

	float x0 = upV[1] * z2 - upV[2] * z1,
		  x1 = upV[2] * z0 - upV[0] * z2,
		  x2 = upV[0] * z1 - upV[1] * z0;
	len = sqrt(x0 * x0 + x1 * x1 + x2 * x2);
	if (len) {
		len = 1 / len;
		x0 *= len;
		x1 *= len;
		x2 *= len;
	} else {
		x0 = x1 = x2 = 0;
	}

	float y0 = z1 * x2 - z2 * x1,
		  y1 = z2 * x0 - z0 * x2,
		  y2 = z0 * x1 - z1 * x0;
	len = sqrt(y0 * y0 + y1 * y1 + y2 * y2);
	if (len) {
		len = 1 / len;
		y0 *= len;
		y1 *= len;
		y2 *= len;
	} else {
		y0 = y1 = y2 = 0;
	}

	return MatrixSet(x0, y0, z0, 0,
			x1, y1, z1, 0,
			x2, y2, z2, 0,
			-(x0 * eyeV[0] + x1 * eyeV[1] + x2 * eyeV[2]), -(y0 * eyeV[0] + y1 * eyeV[1] + y2 * eyeV[2]), -(z0 * eyeV[0] + z1 * eyeV[1] + z2 * eyeV[2]), 1);
}

static void gameStateDraw(struct State *state, float dt) {
	struct GameState *gameState = (struct GameState *) state;
	struct SpriteBatch *batch = gameState->batch;
	ALIGN(16) float vv[4], mv[16];

	// Render to depth map
	glEnable(GL_DEPTH_TEST);
	glCullFace(GL_FRONT); // Avoid peter-panning
	glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, gameState->depthMapFbo);
	glClear(GL_DEPTH_BUFFER_BIT);

	const MATRIX lightProjection = MatrixOrtho(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 100.0f);
	const MATRIX lightView = lookAt(VectorSet(-2.0f, 4.0f, -1.0f, 0.0f), VectorSet(0.0f, 0.0f, 0.0f, 0.0f), VectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	const MATRIX lightMVP = MatrixMultiply(lightProjection, lightView);
	// const MATRIX lightMVP = MatrixMultiply(lightView, lightProjection);
	glUseProgram(gameState->depthProgram);
	glUniformMatrix4fv(glGetUniformLocation(gameState->depthProgram, "lightMVP"), 1, GL_FALSE, MatrixGet(mv, lightMVP));

	// RenderScene();
	// Draw the model
	const GLuint attrib = glGetAttribLocation(gameState->depthProgram, "position");
	glEnableVertexAttribArray(attrib);

	glBindBuffer(GL_ARRAY_BUFFER, gameState->objModel->vertexBuffer);
	glVertexAttribPointer(attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gameState->objModel->indexBuffer);
	glDrawElements(GL_TRIANGLES, gameState->objModel->indexCount, GL_UNSIGNED_INT, 0);

	glBindBuffer(GL_ARRAY_BUFFER, gameState->groundModel->vertexBuffer);
	glVertexAttribPointer(attrib, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gameState->groundModel->indexBuffer);
	glDrawElements(GL_TRIANGLES, gameState->groundModel->indexCount, GL_UNSIGNED_INT, 0);

	glDisableVertexAttribArray(attrib);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glCullFace(GL_BACK);

	// Main pass: render scene as normal with shadow mapping (using depth map)
	glViewport(0, 0, 800, 600);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the screen

	MATRIX rotationViewMatrix = MatrixRotationQuaternion(QuaternionRotationRollPitchYaw(gameState->pitch, gameState->yaw, 0));
	gameState->view = MatrixInverse(MatrixMultiply(
				MatrixTranslationFromVector(gameState->position),
				rotationViewMatrix));
	MATRIX modelView = MatrixMultiply(gameState->view, gameState->model);

	// Draw the model
	glUseProgram(gameState->program);
	glUniformMatrix4fv(gameState->viewUniform, 1, GL_FALSE, MatrixGet(mv, gameState->view));
	// glUniformMatrix4fv(gameState->viewUniform, 1, GL_FALSE, MatrixGet(mv, lightView));
	glUniformMatrix4fv(glGetUniformLocation(gameState->program, "lightMVP"), 1, GL_FALSE, MatrixGet(mv, lightMVP));
	glEnableVertexAttribArray(gameState->posAttrib);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gameState->depthMap);
	glUniform1i(glGetUniformLocation(gameState->program, "depthMap"), 0);

	struct Model *model = gameState->objModel;
	glBindBuffer(GL_ARRAY_BUFFER, model->vertexBuffer);
	glVertexAttribPointer(gameState->posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->indexBuffer);
	glDrawElements(GL_TRIANGLES, model->indexCount, GL_UNSIGNED_INT, 0);

	struct Model *groundModel = gameState->groundModel;
	glBindBuffer(GL_ARRAY_BUFFER, groundModel->vertexBuffer);
	glVertexAttribPointer(gameState->posAttrib, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundModel->indexBuffer);
	glDrawElements(GL_TRIANGLES, groundModel->indexCount, GL_UNSIGNED_INT, 0);

	glDisableVertexAttribArray(gameState->posAttrib);

	// Draw the skybox
	glUseProgram(gameState->skyboxProgram);
	glUniformMatrix4fv(glGetUniformLocation(gameState->skyboxProgram, "invProjection"), 1, GL_FALSE, MatrixGet(mv, MatrixInverse(gameState->projection)));
	glUniformMatrix4fv(glGetUniformLocation(gameState->skyboxProgram, "modelView"), 1, GL_FALSE, MatrixGet(mv, modelView));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, gameState->skyboxTexture);
	glBindBuffer(GL_ARRAY_BUFFER, gameState->skyboxVertexBuffer);
	glEnableVertexAttribArray(glGetAttribLocation(gameState->skyboxProgram, "vertex"));
	glVertexAttribPointer(glGetAttribLocation(gameState->skyboxProgram, "vertex"), 2, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gameState->skyboxIndexBuffer);
	glDepthFunc(GL_LEQUAL);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glDepthFunc(GL_LESS);

	// Draw GUI
	spriteBatchBegin(batch);

	// spriteBatchDraw(batch, gameState->depthMap, 0, 0, 800, 600);

	// widgetValidate(gameState->flexLayout, 800, 600);
	// widgetDraw(gameState->flexLayout, batch);

	spriteBatchEnd(batch);
}

static void gameStateResize(struct State *state, int width, int height) {
	struct GameState *gameState = (struct GameState *) state;
	widgetLayout(gameState->flexLayout, width, MEASURE_EXACTLY, height, MEASURE_EXACTLY);

	gameState->projection = MatrixPerspective(90.f, (float) width / height, 1.0f, 100.0f);
	glUseProgram(gameState->program);
	ALIGN(16) float mv[16];
	glUniformMatrix4fv(gameState->projectionUniform, 1, GL_FALSE, MatrixGet(mv, gameState->projection));
}

static struct FlexParams params0 = { ALIGN_END, -1, 100, UNDEFINED, 20, 0, 20, 20 },
			 params2 = {ALIGN_CENTER, 1, 100, UNDEFINED, 0, 0, 0, 50},
			 params1 = { ALIGN_CENTER, 1, UNDEFINED, UNDEFINED, 0, 0, 0, 0 };

void gameStateInitialize(struct GameState *gameState, struct SpriteBatch *batch) {
	struct State *state = (struct State *) gameState;
	state->update = gameStateUpdate;
	state->draw = gameStateDraw;
	state->resize = gameStateResize;
	gameState->batch = batch;

	const GLchar *vertexShaderSource = "#version 150 core\n"
		"in vec3 position;"
		"uniform mat4 model;"
		"uniform mat4 view;"
		"uniform mat4 projection;"
		"uniform mat4 lightMVP;"
		"varying vec4 shadowMapCoord;"
		"void main() {"
		"	gl_Position = projection * view * model * vec4(position, 1.0);"
		"	shadowMapCoord = lightMVP * vec4(position, 1.0);"
		"}",
		*fragmentShaderSource = "#version 150 core\n"
			"varying highp vec4 shadowMapCoord;"
			"uniform sampler2D depthMap;"
			"out vec4 outColor;"
			"void main() {"
			"	vec3 projCoords = shadowMapCoord.xyz / shadowMapCoord.w;"
			"	projCoords = projCoords * 0.5 + 0.5;"
			"	float closestDepth = texture(depthMap, projCoords.xy).r;"
			"	float currentDepth = projCoords.z;"
			"	float bias = 0.005;"
			"	float shadow = currentDepth - bias > closestDepth ? 0.0 : 1.0;"
			"	shadow = 0.0;"
			"	vec2 texelSize = 1.0 / textureSize(depthMap, 0);"
			"	for (int x = -1; x <= 1; ++x) {"
			"		for (int y = -1; y <= 1; ++y) {"
			"			float pcfDepth = texture(depthMap, projCoords.xy + vec2(x, y) * texelSize).r;"
			"			shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;"
			"		}"
			"	}"
			"	shadow /= 9.0;"
			"	if (projCoords.z > 1.0) shadow = 0.0;"
			"	outColor = vec4(vec3(1.0, 0.0, 0.0) * (0.2 + 1.0 - shadow), 1.0);"
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
			"	gl_FragDepth = 1.0;"
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

	// Shadow mapping
	const GLchar *depthVertexShaderSource = "attribute vec3 position;"
		"uniform mat4 lightMVP;"
		"uniform mat4 model;"
		"void main() {"
		// "	gl_Position = lightMVP * model * vec4(position, 1.0f);"
		"	gl_Position = lightMVP * vec4(position, 1.0f);"
		"}",
		*depthFragmentShaderSource = "void main() {"
			"	gl_FragColor = vec4(0.2, 0.4, 0.5, 1.0);"
			"}";
	gameState->depthProgram = createProgram(depthVertexShaderSource, depthFragmentShaderSource);
	glLinkProgram(gameState->depthProgram);

	GLuint depthMap;
	glGenTextures(1, &depthMap);
	gameState->depthMap = depthMap;
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	GLuint depthMapFbo;
	glGenFramebuffers(1, &depthMapFbo);
	gameState->depthMapFbo = depthMapFbo;
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Error creating framebuffer.\n");
	}
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	gameState->position = VectorSet(0, 0, 0, 1);
	gameState->yaw = 0;
	gameState->pitch = 0;
	gameState->objModel = loadModelFromObj("cube.obj");
	if (!gameState->objModel) {
		printf("Failed to load model.\n");
	}
	gameState->groundModel = loadModelFromObj("ground.obj");
	if (!gameState->groundModel) {
		printf("Failed to load ground model.\n");
	}

	// Initialize GUI
	gameState->flexLayout = malloc(sizeof(struct FlexLayout));
	flexLayoutInitialize(gameState->flexLayout, DIRECTION_ROW, ALIGN_START);

	png_uint_32 width, height;
	gameState->cat = loadPNGTexture("cat.png", &width, &height);
	if (!gameState->cat) {
		fprintf(stderr, "Failed to load png image.\n");
	}
	gameState->image0 = malloc(sizeof(struct Image));
	imageInitialize(gameState->image0, gameState->cat, width, height, 0);
	gameState->image0->layoutParams = &params0;
	containerAddChild(gameState->flexLayout, gameState->image0);

	gameState->image1 = malloc(sizeof(struct Image));
	imageInitialize(gameState->image1, gameState->cat, width, height, 0);
	gameState->image1->layoutParams = &params1;
	containerAddChild(gameState->flexLayout, gameState->image1);

	gameState->font = loadFont("DejaVuSans.ttf", 512, 512);
	if (!gameState->font) {
		printf("Could not load font.");
	}

	gameState->label = labelNew(gameState->font, "Axel ffi! and the AV. HHHHHHHH Hi! (215): tv-hund. fesflhslg");
	gameState->label->layoutParams = &params2;
	containerAddChild(gameState->flexLayout, gameState->label);
}

void gameStateDestroy(struct GameState *gameState) {
	glDeleteProgram(gameState->program);
	glDeleteProgram(gameState->skyboxProgram);
	glDeleteTextures(1, &gameState->skyboxTexture);
	glDeleteBuffers(1, &gameState->skyboxVertexBuffer);
	glDeleteBuffers(1, &gameState->skyboxIndexBuffer);

	destroyModel(gameState->objModel);

	fontDestroy(gameState->font);
	containerDestroy(gameState->flexLayout);
	free(gameState->flexLayout);
	free(gameState->image0);
	free(gameState->image1);
	labelDestroy(gameState->label);
	free(gameState->label);
	glDeleteTextures(1, &gameState->cat);
}
